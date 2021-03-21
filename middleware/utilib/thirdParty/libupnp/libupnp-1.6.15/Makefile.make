CUR_DIR		:=$(shell pwd)
TOP_DIR		:=$(BUILD_TOP_DIR)

include $(TOP_DIR)/Makefile.Inc
include $(TOP_DIR)/Rule.make

INCLUDE 	+=	-I$(RELEASE_DIR)/include

DESTLIB:= ./upnp/.libs/libupnp.so
RELEASEDESTLIB:=$(addprefix $(RELEASE_DIR)/lib/, $(notdir $(DESTLIB)))

export CC
export NM
export STRIP
export AR
export RANLIB

#check if build for ARM
ifeq ($(TARGETENV),ARM)
DEF_CPPFLAGS				:= CPPFLAGS=-I$(RELEASE_DIR)/include
DEF_LDFLAGS				:= LDFLAGS=-L$(RELEASE_DIR)/lib
DEF_PKG_CONFIG_LIBDIR	:= 
CONFIGOPTION			:=	--build=i686-linux --host=arm-linux-gnueabihf
CROSS_COMPILE_TARGET		:=arm-linux
DEF_PKG_CONFIG_LIBDIR	:= PKG_CONFIG_PATH=$(RELEASE_DIR)/lib/pkgconfig

else
ifeq ($(TARGETENV),MIPS)
DEF_CPPFLAGS				:= CPPFLAGS=-I$(RELEASE_DIR)/include
DEF_LDFLAGS				:= LDFLAGS=-L$(RELEASE_DIR)/lib
DEF_PKG_CONFIG_LIBDIR	:= 
CONFIGOPTION			:=	--build=i686-linux --host=mipsel-linux
CROSS_COMPILE_TARGET		:=mipsel-linux
DEF_PKG_CONFIG_LIBDIR	:= PKG_CONFIG_PATH=$(RELEASE_DIR)/lib/pkgconfig

else
DEF_CFLAGS				:=
DEF_LDFLAGS				:= 
DEF_PKG_CONFIG_LIBDIR	:=
CONFIGOPTION			:=
CROSS_COMPILE_TARGET		:=
DEF_PKG_CONFIG_LIBDIR	:= PKG_CONFIG_PATH=$(RELEASE_DIR)/lib/pkgconfig
endif
endif

all release debug:Makefile
	make 


configure:
	rm -rf autom4te.cache
	aclocal -I .
#	libtoolize --force --copy
	autoheader
	autoreconf -i -v
	automake -a

Makefile: configure 
	$(DEF_CPPFLAGS) $(DEF_LDFLAGS) $(DEF_PKG_CONFIG_LIBDIR)  ./configure  --prefix=$(RELEASE_DIR) --target=$(CROSS_COMPILE_TARGET)  $(CONFIGOPTION) \
		--libdir=$(RELEASE_DIR)/lib --includedir=$(RELEASE_DIR)/include --disable-x11 --disable-voodoo --disable-mmx --disable-see --disable-sdl \
		--enable-jpeg --enable-zlib --enable-png --enable-gif --enable-freetype --enable-video4linux --enable-video4linux2 --with-gfxdrivers=none \
		--enable-fbdev=yes --disable-devmem --enable-multi=$(ENABLE_DFB_MP) --with-inputdrivers=all --with-tests  --disable-device --disable-webserver


install:$(RELEASEDESTLIB)


$(RELEASEDESTLIB):$(DESTLIB)
	make install

.PHONY: clean
clean:
	-make clean


.PHONY: distclean
distclean:
	-make clean
	-make distclean
	rm -f Makefile
