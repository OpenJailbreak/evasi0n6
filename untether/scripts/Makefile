CC=clang
CXX=clang++
SDK=/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS6.1.sdk
CFLAGS=-arch armv7 -Wall -Werror -fPIC -O2 -std=gnu99 -isysroot $(SDK) -miphoneos-version-min=6.0
CPPFLAGS=-arch armv7 -Wall -Werror -fPIC -O2 -isysroot $(SDK) -miphoneos-version-min=6.0
FRAMEWORKS=-framework Foundation -framework CoreFoundation -lMobileGestalt

all:	extrainst_ postrm

extrainst_: extrainst_.mm Cydia.mm
	$(CXX) $(CPPFLAGS) $(FRAMEWORKS) $^ -o $@
	ldid -S $@

postrm: postrm.mm Cydia.mm
	$(CXX) $(CPPFLAGS) $(FRAMEWORKS) $^ -o $@
	ldid -S $@

clean:
	rm -f extrainst_ postrm
