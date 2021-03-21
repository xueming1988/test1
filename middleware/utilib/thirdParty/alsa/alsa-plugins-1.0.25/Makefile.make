CUR_DIR		:=$(shell pwd)
TOP_DIR		:=$(BUILD_TOP_DIR)

TARGET_LIB=$(RELEASE_DIR)/lib 
TARGET_INC=$(RELEASE_DIR)/include

include $(TOP_DIR)/Makefile.Inc
include $(TOP_DIR)/Rule.make

INCLUDE 	+=	-I$(RELEASE_DIR)/include -I$(RELEASE_DIR)/include/upnp

DESTLIB:= ./rate/.libs/libasound_module_rate_samplerate.so
RELEASEDESTLIB:=$(addprefix $(RELEASE_DIR)/lib/, $(notdir $(DESTLIB)))


export CC
export NM
export STRIP
export AR
export RANLIB

#check if build for ARM
ifeq ($(TARGETENV),ARM)
DEF_CPPFLAGS			:= CPPFLAGS="-I$(RELEASE_DIR)/include -I$(LPGNU_INCDIR)"
DEF_LDFLAGS			:= LDFLAGS="-L$(TARGET_LIB) -L$(RELEASE_DIR)/lib -L$(LPGNU_LIBDIR) -lgcc_s"
DEF_PKG_CONFIG_LIBDIR	:= 
CONFIGOPTION			:= --build=i686-linux --host=arm-linux-gnueabihf
CROSS_COMPILE_TARGET	:= arm-linux
DEF_PKG_CONFIG_LIBDIR	:= PKG_CONFIG_PATH=$(RELEASE_DIR)/lib/pkgconfig:$(LPGNU_PKGDIR)


else
DEF_CFLAGS				:=
DEF_LDFLAGS				:= 
DEF_PKG_CONFIG_LIBDIR	:=
CONFIGOPTION			:=
CROSS_COMPILE_TARGET	:=
DEF_PKG_CONFIG_LIBDIR	:= PKG_CONFIG_PATH=$(RELEASE_DIR)/lib/pkgconfig
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
	./configure --prefix=$(RELEASE_DIR) $(CONFIGOPTION) $(DEF_CPPFLAGS) $(DEF_LDFLAGS) $(DEF_PKG_CONFIG_LIBDIR) --disable-pulseaudio --disable-avcodec --disable-jack

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
