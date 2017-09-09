NAME := $(shell uname)

ifeq ($(NAME),Darwin)
        CC=clang
        LD=clang
else
        CC=gcc
        LD=gcc
endif

SRCS = generate_confuse_amfi_dylib.c
OBJS = $(SRCS:.c=.o)
ifeq ($(NAME),Darwin)
        CFLAGS = -O3 -Wall -mmacosx-version-min=10.4
        LDFLAGS = -mmacosx-version-min=10.4
else
        CFLAGS = -O3 -Wall
        LDFLAGS =
endif

.PHONY:	package

ifeq ($(NAME),Darwin)
all: amfi.dylib extras.tar evasi0n packagelist.tar.lzma

generate_confuse_amfi_dylib: $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $@

amfi.dylib: generate_confuse_amfi_dylib
	./generate_confuse_amfi_dylib $@

extras.tar: ios/obj/exception extras/com.planetbeing.exception.plist extras/dirhelper
	cd extras; make
	cp extras/extras.tar extras.tar

evasi0n: ios/kiki.m ios/kiki-offsets.m ios/patchfinder.c ios/exploit.c ios/findmemmove.c ios/nvram.c ios/sb_evaluate_trampoline.s ios/sb_evaluate_hook.s ios/zone_fix.m ios/tfp.m ios/untar.c ios/call_function.s ios/jailbreakinstaller.m ios/framebuffer.c
	cd ios; make
	cp ios/obj/evasi0n evasi0n

ios/obj/exception: ios/exception.m
	cd ios; make

packagelist.tar.lzma: ../kiki/ddi/packagelist.tar
	cp $< .
	lzma -9 `basename $<`
	printf "%016llx\n" `stat -f %z $<` | sed 's/\(..\)\(..\)\(..\)\(..\)\(..\)\(..\)\(..\)\(..\)/0000: \8\7\6\5\4\3\2\1/' | xxd -r | dd of=$@ seek=5 bs=1 conv=notrunc

endif

clean:
ifeq ($(NAME),Darwin)
	rm -f $(OBJS)
	rm -f generate_confuse_amfi_dylib
	rm -f amfi.dylib
	rm -f extras.tar
	rm -f evasi0n
	rm -f packagelist.tar.lzma
	cd extras; make clean
	cd ios; make clean
endif
