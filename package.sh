#!/bin/bash

# this script will make a package for the current platform
# .dmg for OSX
# .zip for win32
# .tar.bz2 for linux

VER=`grep EVASI0N_VERSION_STRING src/version.h |cut -d " " -f 3 |sed 's/"//g'`
PRODUCT=evasi0n

case `uname` in
	Darwin*)
		PKGNAME=$PRODUCT-mac-$VER
	;;
	Linux*)
		PKGNAME=$PRODUCT-linux-$VER
	;;
	MINGW*)
		PKGNAME=$PRODUCT-win-$VER
	;;
	*)
		PKGNAME=$PRODUCT-$VER
	;;
esac

BUILDDIR=build/$PRODUCT/$PKGNAME
COMMIT=`git rev-list HEAD | head -n1`
WATERMARK=$1

if ! test -d $BUILDDIR; then
	./build.sh
fi

case `uname` in
	Darwin*)
		SRCDIR="$BUILDDIR"
		MNT="/tmp/dmgmnt"
		rm -rf ${MNT}
		rm -f $PKGNAME-$COMMIT-$WATERMARK.dmg
		rm -f temp.dmg
		cp res/gui/osx/Icon.icns ${SRCDIR}/.VolumeIcon.icns
		SetFile -c icnC "${SRCDIR}/.VolumeIcon.icns"
		SIZE=`du -sk $SRCDIR |cut -f 1`
		SIZE=`echo $SIZE+512 |bc`	
		hdiutil create -srcfolder "${SRCDIR}" -volname "evasi0n ${VER}" -fs HFS+ -fsargs "-c c=64,a=16,e=16" -format UDRW -size ${SIZE}k temp.dmg
		mkdir -p ${MNT}
		hdiutil attach temp.dmg -mountpoint ${MNT}
		SetFile -a C ${MNT}
		hdiutil detach ${MNT}
		rm -rf ${MNT}
		hdiutil convert temp.dmg -format UDBZ -o $PKGNAME-$COMMIT-$WATERMARK.dmg
		rm -f temp.dmg
	;;
	Linux*)
		rm -f $PKGNAME-$COMMIT-$WATERMARK.tar*
		tar cvf $PKGNAME-$COMMIT-$WATERMARK.tar --numeric-owner --owner=root --group=root -C build/$PRODUCT $PKGNAME
		lzma -9 $PKGNAME-$COMMIT-$WATERMARK.tar
	;;
	MINGW*)
		rm -f $PKGNAME-$COMMIT-$WATERMARK.zip
		PD=`pwd`
		cd build/evasi0n
		7z a -tzip -mx=9 -r $PD/$PKGNAME-$COMMIT-$WATERMARK.zip $PKGNAME
		cd ${PD}
	;;
esac
