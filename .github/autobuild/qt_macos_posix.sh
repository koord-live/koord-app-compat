#!/bin/bash
set -eu

# QT_VERSION=${QT_VER}
# export QT_VERSION

## Utility to build Qt for macOS 
# References:
# https://doc.qt.io/qt-6/macos-building.html
# https://doc.qt.io/qt-6/qsharedmemory.html#details


## WHY:
# We need it for SingleApplication - ie ensure one instance
# https://doc.qt.io/qt-6/qsharedmemory.html#details - see Mac section



## REQUIREMENTS (provided by Github macos build image):
# - cmake 

build_qt() {
    # Clean up disk to make space
    df -h

    # remove Visual Studio
    echo ">>> Removing Visual Studio* ...."
    rm -fr /Applications/Visual*

    # remove old Xcodes
    echo ">>> Removing Xcode 13* ...."
    rm -fr /Applications/Xcode_13*

    # remove all Android stuff
    echo ">>> Removing /Users/runner/Library/Android/ ...."
    rm -fr /Users/runner/Library/Android/

    # rm chromewebdriver etc
    echo ">>> Removing webdriver stuff ...."
    rm -fr /usr/local/Caskroom/chromedriver
    rm -fr /usr/local/share/edge_driver
    rm -fr /usr/local/opt/geckodriver/bin

    echo ">>> After free-ing up, disk space is:"
    df -h

    # Get Qt source -
    cd ${GITHUB_WORKSPACE}
    MAJOR_VER=$(echo ${QT_VERSION} | cut -c -3) # get eg "6.3" when QT_VERSION=6.3.2
    echo ">>> Downloading Qt source ..."
    wget -q https://download.qt.io/archive/qt/${MAJOR_VER}/${QT_VERSION}/single/qt-everywhere-src-${QT_VERSION}.tar.xz
    echo ">>> Unzipping qt-everywhere tar.xz file ..."
    gunzip qt-everywhere-src-${QT_VERSION}.tar.xz        # uncompress the archive
    echo ">>> Untarring qt-everywhere archive ..."
    tar xf qt-everywhere-src-${QT_VERSION}.tar          # unpack it

    ## Configure Qt
    cd qt-everywhere-src-${QT_VERSION}
        ## to build Qt with POSIX shared memory, instead of System V shared memory:
        # ./configure -feature-ipc_posix
        ## for universal build:
        # ./configure -- -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"
    # By default, Qt is configured for installation in the /usr/local/Qt-${QT_VERSION} directory,
    # but this can be changed by using the -prefix option.
    ./configure -feature-ipc_posix -- -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"

    # build:
    cmake --build . --parallel

    # install:
    cmake --install .

    # Create archive
    cd ${GITHUB_WORKSPACE}
    echo ">>> Archiving QT installation..."
    tar cvf qt_mac_${QT_VERSION}_posix.tar /usr/local/Qt-${QT_VERSION}
    gzip qt_mac_${QT_VERSION}_posix.tar

    # Output: $HOME/qt_mac_${QT_VERSION}_posix.tar.gz

}

pass_artifacts_to_job() {
    mkdir -p ${GITHUB_WORKSPACE}/deploy
    
    mv -v ${GITHUB_WORKSPACE}/qt_mac_${QT_VERSION}_posix.tar.gz ${GITHUB_WORKSPACE}/deploy/qt_mac_${QT_VERSION}_posix.tar.gz

    echo ">>> Setting output as such: name=artifact_1::qt_mac_${QT_VERSION}_posix.tar.gz"
    echo "::set-output name=artifact_1::qt_mac_${QT_VERSION}_posix.tar.gz"

}

case "${1:-}" in
    build)
        build_qt
        ;;
    get-artifacts)
        pass_artifacts_to_job
        ;;
    *)
        echo "Unknown stage '${1:-}'"
        exit 1
esac