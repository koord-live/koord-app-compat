#!/bin/bash
set -eu

COMMANDLINETOOLS_VERSION=8512546
ANDROID_NDK_VERSION=r22b
ANDROID_PLATFORM=android-30
ANDROID_BUILD_TOOLS=30.0.2
AQTINSTALL_VERSION=2.1.0
QT_VERSION=6.3.0

# Only variables which are really needed by sub-commands are exported.
# Definitions have to stay in a specific order due to dependencies.
QT_BASEDIR="/opt/Qt"
ANDROID_BASEDIR="/opt/android"
BUILD_DIR=build
export ANDROID_SDK_ROOT="${ANDROID_BASEDIR}/android-sdk"
COMMANDLINETOOLS_DIR="${ANDROID_SDK_ROOT}"/cmdline-tools/latest/
export ANDROID_NDK_ROOT="${ANDROID_BASEDIR}/android-ndk"
# # WARNING: Support for ANDROID_NDK_HOME is deprecated and will be removed in the future. Use android.ndkVersion in build.gradle instead.
# # ref: https://bugreports.qt.io/browse/QTBUG-81978?focusedCommentId=497578&page=com.atlassian.jira.plugin.system.issuetabpanels:comment-tabpanel#comment-497578
# ANDROID_NDK_HOME=$ANDROID_NDK_ROOT
ANDROID_NDK_HOST="linux-x86_64"
ANDROID_SDKMANAGER="${COMMANDLINETOOLS_DIR}/bin/sdkmanager"
export JAVA_HOME="/usr/lib/jvm/java-11-openjdk-amd64/"
export PATH="${PATH}:${ANDROID_SDK_ROOT}/tools"
export PATH="${PATH}:${ANDROID_SDK_ROOT}/platform-tools"

if [[ ! ${JAMULUS_BUILD_VERSION:-} =~ [0-9]+\.[0-9]+\.[0-9]+ ]]; then
    echo "Environment variable JAMULUS_BUILD_VERSION has to be set to a valid version string"
    exit 1
fi

setup_ubuntu_dependencies() {
    export DEBIAN_FRONTEND="noninteractive"

    sudo apt-get -qq update
    sudo apt-get -qq --no-install-recommends -y install \
        build-essential zip unzip bzip2 p7zip-full curl chrpath openjdk-11-jdk-headless
}

setup_android_sdk() {
    mkdir -p "${ANDROID_BASEDIR}"

    if [[ -d "${COMMANDLINETOOLS_DIR}" ]]; then
        echo "Using commandlinetools installation from previous run (actions/cache)"
    else
        mkdir -p "${COMMANDLINETOOLS_DIR}"
        curl -s -o downloadfile "https://dl.google.com/android/repository/commandlinetools-linux-${COMMANDLINETOOLS_VERSION}_latest.zip"
        unzip -q downloadfile
        mv cmdline-tools/* "${COMMANDLINETOOLS_DIR}"
    fi

    yes | "${ANDROID_SDKMANAGER}" --licenses
    "${ANDROID_SDKMANAGER}" --update
    "${ANDROID_SDKMANAGER}" "platforms;${ANDROID_PLATFORM}"
    "${ANDROID_SDKMANAGER}" "build-tools;${ANDROID_BUILD_TOOLS}"
}

setup_android_ndk() {
    mkdir -p "${ANDROID_BASEDIR}"

    if [[ -d "${ANDROID_NDK_ROOT}" ]]; then
        echo "Using NDK installation from previous run (actions/cache)"
    else
        echo "Installing NDK from dl.google.com to ${ANDROID_NDK_ROOT}..."
        curl -s -o downloadfile "https://dl.google.com/android/repository/android-ndk-${ANDROID_NDK_VERSION}-linux-x86_64.zip"
        unzip -q downloadfile
        mv "android-ndk-${ANDROID_NDK_VERSION}" "${ANDROID_NDK_ROOT}"
    fi
}

setup_qt() {
    if [[ -d "${QT_BASEDIR}" ]]; then
        echo "Using Qt installation from previous run (actions/cache)"
    else
        echo "Installing Qt..."
        python3 -m pip install "aqtinstall==${AQTINSTALL_VERSION}"
        # icu needs explicit installation 
        # otherwise: "qmake: error while loading shared libraries: libicui18n.so.56: cannot open shared object file: No such file or directory"
        python3 -m aqt install-qt --outputdir "${QT_BASEDIR}" linux desktop "${QT_VERSION}" \
            --archives qtbase qtdeclarative qttools icu

        # - 64bit required for Play Store
        python3 -m aqt install-qt --outputdir "${QT_BASEDIR}" linux android "${QT_VERSION}" android_arm64_v8a \
            --archives qtbase qtdeclarative qttools \
            --modules qtwebview 

        # Also install for arm_v7 to build for 32bit devices
        python3 -m aqt install-qt --outputdir "${QT_BASEDIR}" linux android "${QT_VERSION}" android_armv7 \
            --archives qtbase qtdeclarative qttools \
            --modules qtwebview 
    fi
}

build_app {
    local ARCH_ABI="${1}"

    local QT_DIR="${QT_BASEDIR}/${QT_VERSION}/android"
    local MAKE="${ANDROID_NDK_ROOT}/prebuilt/${ANDROID_NDK_HOST}/bin/make"

    echo "${GOOGLE_RELEASE_KEYSTORE}" | base64 --decode > android/android_release.keystore

    echo ">>> Compiling for ${ARCH_ABI} ..."
    # if ARCH_ABI=android_armv7 we need to override ANDROID_ABIS for qmake 
    if [ "${ARCH_ABI}" == "android_armv7" ]; then
        ANDROID_ABIS=android_armv7 "${QT_BASEDIR}/${QT_VERSION}/${ARCH_ABI}/bin/qmake" -spec android-clang
    else
        "${QT_BASEDIR}/${QT_VERSION}/${ARCH_ABI}/bin/qmake" -spec android-clang
    fi
    "${MAKE}" -j "$(nproc)"
    "${MAKE}" INSTALL_ROOT="${BUILD_DIR}" -f Makefile install
}

build_aab() {
    echo ">>> Building .aab file ...."

    "${QT_BASEDIR}"/${QT_VERSION}/gcc_64/bin/androiddeployqt --input android-Koord-RT-deployment-settings.json \
        --verbose \
        --output "${BUILD_DIR}" \
        --aab \
        --release \
        --sign android/android_release.keystore koord \
            --storepass ${GOOGLE_KEYSTORE_PASS} \
        --android-platform "${ANDROID_PLATFORM}" \
        --jdk "${JAVA_HOME}" \
        --gradle
}

pass_artifact_to_job() {
    mkdir deploy
    local artifact="Koord_${JAMULUS_BUILD_VERSION}_android.aab"
    # debug to check for filenames
    ls -alR ${BUILD_DIR}/build/
    echo "Moving ${BUILD_DIR}/build/outputs/bundle/release/build-release.aab to deploy/${artifact}"
    mv "./${BUILD_DIR}/build/outputs/bundle/release/build-release.aab" "./deploy/${artifact}"
    echo "::set-output name=artifact_1::${artifact}"
}

case "${1:-}" in
    setup)
        setup_ubuntu_dependencies
        setup_android_ndk
        setup_android_sdk
        setup_qt
        ;;
    build)
        build_app "android_armv7"
        build_app "android_arm64_v8a"
        build_aab
        ;;
    get-artifacts)
        pass_artifact_to_job
        ;;
    *)
        echo "Unknown stage '${1:-}'"
        exit 1
esac
