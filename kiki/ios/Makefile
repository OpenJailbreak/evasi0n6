include theos/makefiles/common.mk

export TARGET_STRIP=strip
export TARGET_STRIP_FLAGS=-s save_symbols -i
export ARCHS=armv7

override TARGET_CODESIGN_FLAGS = -Splatform-application-entitlement.plist

TOOL_NAME = jailbreak kiki exception

jailbreak_FILES = jailbreak.m jailbreakinstaller.m untar.c
jailbreak_FRAMEWORKS = CoreFoundation
jailbreak_PRIVATE_FRAMEWORKS = MobileCoreServices
jailbreak_LIBRARIES = MobileGestalt
jailbreak_CFLAGS = -Iheaders

exception_FILES = exception.m
exception_FRAMEWORKS = CoreFoundation Foundation

kiki_FILES = kiki.m kiki-offsets.m patchfinder.c exploit.c findmemmove.c nvram.c sb_evaluate_trampoline.s sb_evaluate_hook.s zone_fix.m tfp.m call_function.s
kiki_FRAMEWORKS = CoreFoundation IOKit
kiki_LIBRARIES = z
kiki_CFLAGS = -Iheaders

include $(THEOS_MAKE_PATH)/library.mk
include $(THEOS_MAKE_PATH)/tool.mk
