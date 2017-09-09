include theos/makefiles/common.mk

export TARGET_STRIP=strip
export TARGET_STRIP_FLAGS=-s save_symbols -i
export ARCHS=armv7

override TARGET_CODESIGN_FLAGS = -Splatform-application-entitlement.plist

TOOL_NAME = evasi0n exception

exception_FILES = exception.m
exception_FRAMEWORKS = CoreFoundation Foundation

evasi0n_FILES = kiki.m kiki-offsets.m patchfinder.c exploit.c findmemmove.c nvram.c sb_evaluate_trampoline.s sb_evaluate_hook.s zone_fix.m tfp.m untar.c call_function.s jailbreakinstaller.m framebuffer.c deadman.c kiki-log.m wdt.c localize.m
evasi0n_FRAMEWORKS = CoreFoundation IOKit CoreGraphics CoreText IOSurface
evasi0n_PRIVATE_FRAMEWORKS = IOMobileFramebuffer
evasi0n_LIBRARIES = z
evasi0n_CFLAGS = -Iheaders
evasi0n_LDFLAGS = -sectcreate __DATA logo logo.png -sectcreate __DATA control ../../untether/package-debian/control -sectcreate __DATA languages ../../res/languages.plist

before-evasi0n-all:: watermark.h ../../res/languages.plist

../../res/languages.plist:
	../../generate_languages.py

watermark.h:
	./generate-watermark.sh $(WATERMARK)

clean::
	rm -f watermark.h
	rm -f ../../res/languages.plist

include $(THEOS_MAKE_PATH)/library.mk
include $(THEOS_MAKE_PATH)/tool.mk
