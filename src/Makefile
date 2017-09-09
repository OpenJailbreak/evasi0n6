MACHINE=$(shell $(CC) -dumpmachine)

CFLAGS=-I. -Wall -I../include
LDFLAGS=
LDADD=

STRIP_PARAMS=-S -x -X

GUICFLAGS=
GUILDFLAGS=
GUILDADD=

common_objects = backup.o backup_file.o file.o mbdb.o mbdb_record.o common.o cpio.o jailbreak.o idevicebackup2.o LzmaDec.o localize.o hashmap.o
objects = main.o $(common_objects)

gui_objects = main.cpp.o DeviceHandler.cpp.o JailbreakHandler.cpp.o iTunesKiller.cpp.o

ifeq ($(findstring darwin,$(MACHINE)), darwin)
  ifneq ($(wildcard "/Applications/Xcode.app/Contents/Developer"), "")
    SDK_DIR=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs
  else
    SDK_DIR=/Developer/SDKs
  endif

  check_arch = $(shell if gcc -arch $(1) -c -xc - -o /dev/null < /dev/null 2> /dev/null; then echo yes; else echo no; fi)
  ifeq ($(call check_arch,i386), yes)
    CFLAGS+=-arch i386
    LDFLAGS+=-arch i386
  endif
  ifeq ($(call check_arch,ppc), yes)
    CFLAGS+=-arch ppc
    LDFAGS+=-arch ppc
  endif 

  CFLAGS+=-fPIC -DHAVE_ASPRINTF -DHAVE_VASPRINTF -isysroot $(SDK_DIR)/MacOSX10.5.sdk -mmacosx-version-min=10.5
  gui_objects+=MainWnd_osx.cpp.o bsdprocesslist.o
  GUICFLAGS+=-fPIC -ObjC++ -DPIC -D_FILE_OFFSET_BITS=64 -D_LARGE_FILES
  GUILDFLAGS+=-pthread -static-libstdc++
  GUILDADD+=-lobjc -framework IOKit -framework Cocoa -framework System
  LDADD+=../staticlibs/osx/libimobiledevice.a ../staticlibs/osx/libusbmuxd.a ../staticlibs/osx/libplist.a -lxml2 -lssl -lcrypto -lz -liconv -framework CoreFoundation
  LDFLAGS+=-sectcreate __DATA packagelist packagelist.tar.lzma -sectcreate __DATA cydia Cydia.tar.lzma -isysroot $(SDK_DIR)/MacOSX10.5.sdk -mmacosx-version-min=10.5
endif

ifeq ($(findstring linux,$(MACHINE)), linux)
  ifeq ($(PLATFORM), )
    PLATFORM=$(shell echo $(MACHINE) |cut -d "-" -f 1)
  endif
  ifeq ($(PLATFORM), x86)
    LIBPLATFORM=i386
  else
    LIBPLATFORM=$(PLATFORM)
  endif
  ifeq ($(LIBPLATFORM), i386)
    CFLAGS+=-m32
    LDFLAGS+=-m32
    LINUX32=-melf_i386
    LIBGCCPLATFORM=i686
  else
    LIBGCCPLATFORM=$(PLATFORM)
  endif
  CFLAGS+=-fPIC -DHAVE_ASPRINTF -DHAVE_VASPRINTF
  LDFLAGS+=-static-libgcc
  gui_objects+=MainWnd_linux.cpp.o
  common_objects+=linux_cydia.o linux_packagelist.o shim.o
  GUICFLAGS+=-fPIC -DPIC -D_FILE_OFFSET_BITS=64 -D_LARGE_FILES \
	-I/usr/include/gtk-2.0 -I/usr/lib/${LIBPLATFORM}-linux-gnu/gtk-2.0/include \
	-I/usr/include/gdk-pixbuf-2.0 \
	-I/usr/include/atk-1.0 \
	-I/usr/include/glib-2.0 -I/usr/lib/${LIBPLATFORM}-linux-gnu/glib-2.0/include \
	-I/usr/include/cairo \
	-I/usr/include/pango-1.0
  GUILDFLAGS+=-pthread -L/usr/lib/gcc/${LIBGCCPLATFORM}-linux-gnu/4.7 -static-libstdc++
  GUILDADD+=$(shell pkg-config --libs gtk+-2.0 gthread-2.0)
  LDADD+=../staticlibs/linux.${PLATFORM}/libimobiledevice.a ../staticlibs/linux.${PLATFORM}/libusbmuxd.a ../staticlibs/linux.${PLATFORM}/libplist.a -lxml2 -lssl -lcrypto -lz -lpthread
  STRIP_PARAMS+=--strip-unneeded 
endif

WIN32=
ifeq ($(findstring cygwin,$(MACHINE)), cygwin)
  WIN32=1
  CC=gcc-3
endif
ifeq ($(findstring mingw32,$(MACHINE)), mingw32)
  WIN32=1
endif
ifdef WIN32
  CFLAGS+=-DWIN32 -D_WIN32 -DUNICODE -D_UNICODE -D__LITTLE_ENDIAN__=1 -mthreads
  LDFLAGS+=-s -static-libgcc
  GUILDFLAGS+=-mthreads -Wl,--subsystem,windows -mwindows -static-libstdc++
  LDADD+=../staticlibs/win32/libimobiledevice.a ../staticlibs/win32/libplist.a ../staticlibs/win32/libxml2.a ../staticlibs/win32/libusbmuxd.a ../staticlibs/win32/libssl.a ../staticlibs/win32/libcrypto.a ../staticlibs/win32/libz.a
  LDADD+=-lws2_32 -liphlpapi -lgdi32 -lole32
  GUILDADD+=-lcomctl32
  objects+=win32cli.o
  gui_objects+=MainWnd_win32.cpp.o win32res.o
endif

gui_objects+=$(common_objects)

ifdef WIN32
all: evasi0n.exe evasi0n_gui.exe
else
all: evasi0n evasi0n_gui
endif

../kernel/Cydia.tar.lzma:
	@echo "Please put Cydia.tar.lzma into ../kernel/"
	@false

../kernel/packagelist.tar.lzma:
	@echo "Please put packagelist.tar.lzma into ../kernel/"
	@false

win32res.o: ../kernel/packagelist.tar.lzma ../kernel/Cydia.tar.lzma ../res/gui/win32/res.rc
	windres ../res/gui/win32/res.rc -O coff $@

win32cli.o: ../kernel/packagelist.tar.lzma ../kernel/Cydia.tar.lzma cli.rc
	windres cli.rc -O coff $@

linux_cydia.o: Cydia.tar.lzma	
	ld $(LINUX32) -r -b binary -o linux_cydia.o Cydia.tar.lzma
 
linux_packagelist.o: packagelist.tar.lzma
	ld $(LINUX32) -r -b binary -o $@ packagelist.tar.lzma

%.cpp.o: %.cpp
	g++ -o $@ $(CFLAGS) $(GUICFLAGS) -c $^

%.o: %.c
	gcc -o $@ $(CFLAGS) -c $^

ifdef WIN32
evasi0n.exe: $(objects)
else
evasi0n: $(objects) 
endif
	gcc -o $@ $^ $(LDFLAGS) $(LDADD)
	strip $(STRIP_PARAMS) $@

ifdef WIN32
evasi0n_gui.exe: $(gui_objects)
else
evasi0n_gui: $(gui_objects)
endif
	g++ -o $@ $^ $(LDFLAGS) $(GUILDFLAGS) $(LDADD) $(GUILDADD)
	strip $(STRIP_PARAMS) $@

clean:
	rm -f $(objects)
	rm -f $(gui_objects)
	rm -f evasi0n evasi0n_gui
