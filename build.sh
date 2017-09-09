#!/bin/sh

VER=`grep EVASI0N_VERSION_STRING src/version.h |cut -d " " -f 3 |sed 's/"//g'`
PKGNAME=evasi0n-$VER

MACHINE=`gcc -dumpmachine`
case "$MACHINE" in
	*darwin*)
	echo Rebuilding kernel/iOS stuff
	make -C kernel clean
	if ! make -C kernel WATERMARK=$1; then
		echo "Rebuilding kernel/iOS stuff failed"
		exit 1
	fi
	;;
esac

echo Creating resources.h

# icons for DemoApp.app:
RES=""
for I in res/DemoApp.app/Icon*.png; do
  BN=`basename $I`
  CNAME=`echo $BN |tr '[:upper:]' '[:lower:]' |sed -e 's/[\.\@\,\-]/_/g'`
  RES="$RES $I $CNAME "
done

# Info.plist for DemoApp.app
if plutil -i > /dev/null 2> /dev/null; then
  # libplist plutil
  plutil -i res/DemoApp.app/Info.plist -o Info.bplist.tmp
else
  # osx plutil
  plutil -convert binary1 -o Info.bplist.tmp res/DemoApp.app/Info.plist
fi
RES="$RES Info.bplist.tmp demo_app_info_plist "

# shebang fstab launchd.conf
RES="$RES res/shebang shebang res/fstab etc_fstab res/launchd.conf etc_launchd_conf "

# fake backup_kbag.bin
RES="$RES res/backup_kbag.bin backup_kbag "

if ! test -f kernel/evasi0n; then
  echo "ERROR: Please add kernel/evasi0n"
  exit 1
fi
if ! test -f kernel/amfi.dylib; then
  echo "ERROR: Please add kernel/amfi.dylib"
  exit 1
fi

# amfi.dylib evasi0n binary
RES="$RES kernel/amfi.dylib amfi_dylib kernel/evasi0n evasi0n_bin "

if ! test -f kernel/extras.tar; then
  echo "ERROR: Please add kernel/extras.tar"
  exit 1
fi

# extras.tar
RES="$RES kernel/extras.tar extras_tar"

# languages.plist
python ./generate_languages.py
if plutil -i > /dev/null 2> /dev/null; then
  # libplist plutil
  plutil -i res/languages.plist -o languages.bplist.tmp
else
  # osx plutil
  plutil -convert binary1 -o languages.bplist.tmp res/languages.plist
fi
RES="$RES languages.bplist.tmp languages_plist"

# now make a sweet header file
misc/bin2c $RES > src/resources.h
rm -f Info.bplist.tmp
rm -f languages.bplist.tmp

# add gui resources (os specific)
case "$MACHINE" in
	*darwin*)
		PKGNAME=evasi0n-mac-$VER
		misc/bin2c res/gui/osx/background.png gui_bg res/gui/osx/e_logo.png evasi0n_logo > src/guiresources.h
	;;
	*linux*)
		PKGNAME=evasi0n-linux-$VER
	;;
	*mingw32*|*cygwin*|*msys*)
		PKGNAME=evasi0n-win-$VER
	;;
	*)
		echo "Unknown platform $MACHINE"
		exit 1
	;;
esac

rm -rf build

# now rebuild stuff
make -C src -f Makefile clean
if ! make -C src -f Makefile; then
  echo build failed...
  exit 1
fi

case "$MACHINE" in
	*linux*)
	mv src/evasi0n evasi0n.x86_64
	mv src/evasi0n_gui evasi0n_gui.x86_64
	make -C src -f Makefile clean
	if ! PLATFORM=x86 make -C src -f Makefile; then
		echo building x86 failed
	fi
	;;
esac

CLIDEST=build/evasi0n/cli
GUIDEST=build/evasi0n/$PKGNAME
mkdir -p $CLIDEST
mkdir -p $GUIDEST

case "$MACHINE" in
	*darwin*)
		cp src/evasi0n $CLIDEST/
		OSX_BUNDLE_NAME=evasi0n
		mkdir -p $GUIDEST/${OSX_BUNDLE_NAME}.app/Contents/MacOS
		mkdir -p $GUIDEST/${OSX_BUNDLE_NAME}.app/Contents/Resources
		echo "APPL????" > $GUIDEST/${OSX_BUNDLE_NAME}.app/Contents/PkgInfo
		cp res/gui/osx/Info.plist $GUIDEST/${OSX_BUNDLE_NAME}.app/Contents/
		cp res/gui/osx/Icon.icns $GUIDEST/${OSX_BUNDLE_NAME}.app/Contents/Resources/
		APPDEST=$GUIDEST/${OSX_BUNDLE_NAME}.app/Contents/MacOS
		cp src/evasi0n_gui $APPDEST/${OSX_BUNDLE_NAME}
		chmod 755 $APPDEST/${OSX_BUNDLE_NAME}
                codesign -s "Developer ID Application: Xuzz Productions" $APPDEST/${OSX_BUNDLE_NAME}
	;;
	*linux*)
		cp src/evasi0n $CLIDEST/evasi0n.x86
		cp src/evasi0n_gui $GUIDEST/evasi0n.x86
		mv evasi0n.x86_64 $CLIDEST/evasi0n.x86_64
		mv evasi0n_gui.x86_64 $GUIDEST/evasi0n.x86_64
		cp res/gui/linux/evasi0n-launcher.sh $CLIDEST/evasi0n
		chmod 755 $CLIDEST/evasi0n
		cp res/gui/linux/evasi0n-launcher.sh $GUIDEST/evasi0n
		chmod 755 $GUIDEST/evasi0n
	;;
	*mingw32*|*cygwin*|*msys*)
		cp src/evasi0n.exe $CLIDEST/
		cp src/evasi0n_gui.exe $GUIDEST/evasi0n.exe
	;;
esac

cp changelog.txt $CLIDEST/
cp readme.txt $CLIDEST/README.txt
cp changelog.txt $GUIDEST/
cp readme.txt $GUIDEST/README.txt

case "$MACHINE" in
	*mingw32*|*cygwin*|*msys*)
		conv --u2d $CLIDEST/changelog.txt
		conv --u2d $CLIDEST/README.txt	
		conv --u2d $GUIDEST/changelog.txt
		conv --u2d $GUIDEST/README.txt
	;;
	*)
	;;
esac

echo build complete.

