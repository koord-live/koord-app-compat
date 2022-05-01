#!/bin/sh -e

# autobuild_3_copy_files: copy the built files to deploy folder

if [ "$#" -gt 1 ]; then
    BUILD_SUFFIX=_$1
    shift
fi

####################
###  PARAMETERS  ###
####################

source "$(dirname "${BASH_SOURCE[0]}")/../../ensure_THIS_JAMULUS_PROJECT_PATH.sh"

###################
###  PROCEDURE  ###
###################

cd "${THIS_JAMULUS_PROJECT_PATH}"

echo ""
echo ""
echo "ls GITROOT/deploy/"
ls "${THIS_JAMULUS_PROJECT_PATH}"/deploy/
echo ""

echo ""
echo ""
artifact_deploy_filename=Koord-RT_${jamulus_buildversionstring}_mac${BUILD_SUFFIX}.dmg
artifactpkg_deploy_filename=KoordRT_${jamulus_buildversionstring}_${BUILD_SUFFIX}.pkg
echo "Move/Rename the built files to deploy/${artifact_deploy_filename}"
mv "${THIS_JAMULUS_PROJECT_PATH}"/deploy/Koord-RealTime-*installer-mac.dmg "${THIS_JAMULUS_PROJECT_PATH}"/deploy/"${artifact_deploy_filename}"
mv "${THIS_JAMULUS_PROJECT_PATH}"/deploy/KoordRT_*.pkg "${THIS_JAMULUS_PROJECT_PATH}"/deploy/"${artifactpkg_deploy_filename}"

echo ""
echo ""
echo "ls GITROOT/deploy/"
ls "${THIS_JAMULUS_PROJECT_PATH}"/deploy/
echo ""


github_output_value()
{
  echo "github_output_value() ${1} = ${2}"
  echo "::set-output name=${1}::${2}"
}

github_output_value artifact_1 ${artifact_deploy_filename}
github_output_value artifact_2 ${artifactpkg_deploy_filename}
