#!/bin/sh

if [ $(id -u) != "0" ]; then
  echo Please run this with fakeroot or sudo.
  exit 1
fi

# copy files
rm -rf package/

mkdir -p package/DEBIAN
cp -a package-debian/* package/DEBIAN/
cp -a scripts/postrm scripts/extrainst_ package/DEBIAN/

mkdir -p package/var/evasi0n
cp ../kernel/evasi0n package/var/evasi0n/evasi0n
cp ../kernel/amfi.dylib package/var/evasi0n/amfi.dylib
mkdir -p package/usr/libexec
cp ../kernel/extras/dirhelper package/usr/libexec/dirhelper

chown -R 0:0 package

# build package
PKG=`grep Package: package/DEBIAN/control |cut -d " " -f 2`
VER=`grep Version: package/DEBIAN/control |cut -d " " -f 2`
sudo dpkg-deb -z9 -Zlzma -b package ${PKG}_${VER}_iphoneos-arm.deb

