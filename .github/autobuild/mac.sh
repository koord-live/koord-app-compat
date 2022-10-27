#!/bin/bash
set -eu

QT_DIR=/usr/local/opt/qt
# The following version pinnings are semi-automatically checked for
# updates. Verify .github/workflows/bump-dependencies.yaml when changing those manually:
AQTINSTALL_VERSION=3.0.1

TARGET_ARCHS="${TARGET_ARCHS:-}"

if [[ ! ${QT_VERSION:-} =~ [0-9]+\.[0-9]+\..* ]]; then
    echo "Environment variable QT_VERSION must be set to a valid Qt version"
    exit 1
fi
if [[ ! ${JAMULUS_BUILD_VERSION:-} =~ [0-9]+\.[0-9]+\.[0-9]+ ]]; then
    echo "Environment variable JAMULUS_BUILD_VERSION has to be set to a valid version string"
    exit 1
fi

setup() {
    if [[ -d "${QT_DIR}" ]]; then
        echo "Using Qt installation from previous run (actions/cache)"
    else
        echo "Installing Qt..."
        python3 -m pip install "aqtinstall==${AQTINSTALL_VERSION}"

        # no need for webengine in Mac! At all! Like iOS
        python3 -m aqt install-qt --outputdir "${QT_DIR}" mac desktop "${QT_VERSION}" \
            --archives qtbase qtdeclarative qtsvg qttools \
            --modules qtwebview
    fi
}

prepare_signing() {
    ##  Certificate types in use:
    # - MAC_ADHOC_CERT - Developer ID Application - for codesigning for adhoc release
    # - MAC_STORE_APP_CERT - Mac App Distribution - codesigning for App Store submission
    # - MAC_STORE_INST_CERT - Mac Installer Distribution - for signing installer pkg file for App Store submission

    [[ "${SIGN_IF_POSSIBLE:-0}" == "1" ]] || return 1

    # Signing was requested, now check all prerequisites:
    [[ -n "${MAC_ADHOC_CERT:-}" ]] || return 1
    [[ -n "${MAC_ADHOC_CERT_ID:-}" ]] || return 1
    [[ -n "${MAC_ADHOC_CERT_PWD:-}" ]] || return 1
    [[ -n "${MAC_STORE_APP_CERT:-}" ]] || return 1
    [[ -n "${MAC_STORE_APP_CERT_ID:-}" ]] || return 1
    [[ -n "${MAC_STORE_APP_CERT_PWD:-}" ]] || return 1
    [[ -n "${MAC_STORE_INST_CERT:-}" ]] || return 1
    [[ -n "${MAC_STORE_INST_CERT_ID:-}" ]] || return 1
    [[ -n "${MAC_STORE_INST_CERT_PWD:-}" ]] || return 1
    [[ -n "${NOTARIZATION_USERNAME:-}" ]] || return 1
    [[ -n "${NOTARIZATION_PASSWORD:-}" ]] || return 1
    [[ -n "${KEYCHAIN_PASSWORD:-}" ]] || return 1

    echo "Signing was requested and all dependencies are satisfied"

    ## Put the certs to files
    echo "${MAC_ADHOC_CERT}" | base64 --decode > macadhoc_certificate.p12
    echo "${MAC_STORE_APP_CERT}" | base64 --decode > macapp_certificate.p12
    echo "${MAC_STORE_INST_CERT}" | base64 --decode > macinst_certificate.p12
    
    # Set up a keychain for the build:
    security create-keychain -p "${KEYCHAIN_PASSWORD}" build.keychain
    security default-keychain -s build.keychain
    # Remove default re-lock timeout to avoid codesign hangs:
    security set-keychain-settings build.keychain
    security unlock-keychain -p "${KEYCHAIN_PASSWORD}" build.keychain
    # add certs to keychain
    security import macadhoc_certificate.p12 -k build.keychain -P "${MAC_ADHOC_CERT_PWD}" -A -T /usr/bin/codesign 
    security import macapp_certificate.p12 -k build.keychain -P "${MAC_STORE_APP_CERT_PWD}" -A -T /usr/bin/codesign
    security import macinst_certificate.p12 -k build.keychain -P "${MAC_STORE_INST_CERT_PWD}" -A -T /usr/bin/productbuild 
    # # add notarization/validation/upload password to keychain
    # xcrun altool --store-password-in-keychain-item --keychain build.keychain APPCONNAUTH -u $NOTARIZATION_USERNAME -p $NOTARIZATION_PASSWORD
    # allow the default keychain access to cli utilities
    security set-key-partition-list -S apple-tool:,apple: -s -k "${KEYCHAIN_PASSWORD}" build.keychain
    # set lock timeout on keychain to 6 hours - possibly optional
    security set-keychain-settings -lut 21600

    # # Store PW for notarization
    # xcrun notarytool store-credentials "AC_PASSWORD" \
    #            --apple-id $NOTARIZATION_USERNAME \
    #            --team-id TXZ4FR95HG \
    #            --password "${KEYCHAIN_PASSWORD}"

    # Tell Github Workflow that we need notarization & stapling:
    echo "::set-output name=macos_signed::true"
    return 0
}

build_app_as_dmg_installer() {
    # Add the qt binaries to the PATH.
    # The clang_64 entry can be dropped when Qt <6.2 compatibility is no longer needed.
    export PATH="${QT_DIR}/${QT_VERSION}/macos/bin:${QT_DIR}/${QT_VERSION}/clang_64/bin:${PATH}"

    # Mac's bash version considers BUILD_ARGS unset without at least one entry:
    BUILD_ARGS=("")
    if prepare_signing; then
        BUILD_ARGS=("-s" "${MAC_ADHOC_CERT_ID}" "-a" "${MAC_STORE_APP_CERT_ID}" "-i" "${MAC_STORE_INST_CERT_ID}" "-k" "${KEYCHAIN_PASSWORD}")
    fi
    TARGET_ARCHS="${TARGET_ARCHS}" ./mac/deploy_mac.sh "${BUILD_ARGS[@]}"
}

pass_artifact_to_job() {
    artifact="Koord_${JAMULUS_BUILD_VERSION}_${ARTIFACT_SUFFIX:-}.dmg"
    echo "Moving build artifact to deploy/${artifact}"
    mv ./deploy/Koord-*installer-mac.dmg "./deploy/${artifact}"
    echo "::set-output name=artifact_1::${artifact}"

    artifact2="Koord_${JAMULUS_BUILD_VERSION}_mac_storesign${ARTIFACT_SUFFIX:-}.pkg"
    if [ -f ./deploypkg/Koord*.pkg ]; then
        echo "Moving build artifact2 to deploy/${artifact2}"
        mv ./deploypkg/Koord*.pkg "./deploy/${artifact2}"
        echo "::set-output name=artifact_2::${artifact2}"
    fi
}

valid8_n_upload() {
    echo ">>> Processing validation and upload..."
    # test the signature of package
    pkgutil --check-signature "${ARTIFACT_PATH}"
    # attempt validate and then upload of pkg file, using previously-made keychain item
    xcrun altool --validate-app -f "${ARTIFACT_PATH}" -t macos -u $NOTARIZATION_USERNAME -p $NOTARIZATION_PASSWORD
    xcrun altool --upload-app -f "${ARTIFACT_PATH}" -t macos -u $NOTARIZATION_USERNAME -p $NOTARIZATION_PASSWORD

    # ## Using notarytool:
    # xcrun notarytool submit "${ARTIFACT_PATH}" \
    #     --keychain-profile "AC_PASSWORD" \
    #     --wait
    # #    --webhook "https://example.com/notarization"

}

case "${1:-}" in
    setup)
        setup
        ;;
    build)
        build_app_as_dmg_installer
        ;;
    get-artifacts)
        pass_artifact_to_job
        ;;
    validate_and_upload)
        valid8_n_upload
        ;;
    *)
        echo "Unknown stage '${1:-}'"
        exit 1
        ;;
esac
