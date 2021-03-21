CUR_DIR		:=$(shell pwd)
TOP_DIR		:=$(BUILD_TOP_DIR)

include $(TOP_DIR)/Makefile.Inc
include $(TOP_DIR)/Rule.make

DESTLIB:= ./src/.libs/libasound.so
RELEASEDESTLIB:=$(addprefix $(RELEASE_DIR)/lib/, $(notdir $(DESTLIB)))

export CC
export NM
export STRIP
export AR
export RANLIB
 
#check if debug version
ifneq (,$(findstring debug, $(MAKECMDGOALS)))
DEBUG_OPT :=--with-debug
else
DEBUG_OPT :=
endif

ifeq ($(OPENWRT_AVS), YES)
CONFIGOPTION                                            :=--host=arm-linux-gnueabihf --disable-python
DEF_CFLAGS                                                      := CFLAGS='$(CFLAGS) -Wno-error -I$(RELEASE_DIR)/include -I$(COMMON_INC_DIR)'
DEF_PKG_CONFIG_LIBDIR           :=PKG_CONFIG_PATH=$(RELEASE_DIR)/lib/pkgconfig

else
#check if build for ARM
ifeq ($(TARGETENV),ARM)
CONFIGOPTION						:=--host=arm-linux-gnueabihf --disable-python 
DEF_CFLAGS							:= CFLAGS='$(CFLAGS) -Wno-error -I$(RELEASE_DIR)/include -I$(COMMON_INC_DIR)'
DEF_PKG_CONFIG_LIBDIR		:=PKG_CONFIG_PATH=$(RELEASE_DIR)/lib/pkgconfig

else
ifeq ($(TARGETENV),MIPS)
CONFIGOPTION						:=--host=mipsel-linux --disable-python 
DEF_CFLAGS							:= CFLAGS='$(CFLAGS) -Wno-error -I$(RELEASE_DIR)/include -I$(COMMON_INC_DIR)'
DEF_PKG_CONFIG_LIBDIR		:=PKG_CONFIG_PATH=$(RELEASE_DIR)/lib/pkgconfig

else
CONFIGOPTION						:=
DEF_PKG_CONFIG_LIBDIR		:=PKG_CONFIG_PATH=$(RELEASE_DIR)/lib/pkgconfig
DEF_CFLAGS							:= 

endif
endif
endif

all release debug:Makefile 
	make 

configure:
	rm -rf autom4te.cache
	aclocal 
	libtoolize --force --copy
	autoheader
	autoconf
	automake -a

Makefile:configure  
	$(DEF_CFLAGS) ./configure --prefix=$(RELEASE_DIR) $(DEBUG_OPT) $(CONFIGOPTION) --with-gnu-ld --enable-shared=yes --disable-alisp --disable-aload --disable-rawmidi --disable-hwdep --disable-seq --disable-ucm --disable-python --disable-old-symbols --with-versioned=no --with-configdir=/system/workdir/lib/alsa

install:
	make install


.PHONY: clean
clean:
	-make clean


.PHONY: distclean
distclean:
	-make clean
	-make distclean
	rm -f Makefile configure
