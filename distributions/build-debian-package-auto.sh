#!/bin/sh -e

# Create deb files

# set armhf
#sudo dpkg --add-architecture armhf

cp -r debian ..
cd ..

# get the jamulus version from pro file
VERSION=$(cat Koord-RT.pro | grep -oP 'VERSION = \K\w[^\s\\]*')

# patch changelog (with hack)

DATE=$(date "+%a, %d %b %Y %T" )
echo "koord-rt (${VERSION}-0) UNRELEASED; urgency=medium" > debian/changelog
echo "" >> debian/changelog
perl .github/actions_scripts/getChangelog.pl ChangeLog ${VERSION} >> debian/changelog
echo "" >> debian/changelog
echo " -- GitHubActions <noemail@example.com> ${DATE} +0001" >> debian/changelog
echo "" >> debian/changelog
cat distributions/debian/changelog >> debian/changelog

# patch the control file
# cp the modified control file here

cp distributions/autobuilddeb/control debian/control

echo "${VERSION} building..."

sed -i "s/é&%JAMVERSION%&è/${VERSION}/g" debian/control

debuild --preserve-env -b -us -uc
