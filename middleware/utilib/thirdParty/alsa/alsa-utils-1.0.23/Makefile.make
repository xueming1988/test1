CUR_DIR		:=$(shell pwd)
TOP_DIR		:=$(BUILD_TOP_DIR)

TARGET_LIB=$(RELEASE_DIR)/lib
TARGET_INC=$(RELEASE_DIR)/include

include $(TOP_DIR)/Makefile.Inc
include $(TOP_DIR)/Rule.make

INCLUDE 	+=	-I$(RELEASE_DIR)/include 

DESTLIB:= ./amixer/amixer
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
CONFIGOPTION			:=	--build==mipsel-linux --host=mipsel-linux
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
	aclocal 
	libtoolize --force --copy
	autoheader
	autoconf
	automake -a

Makefile:  
	$(DEF_CPPFLAGS) $(DEF_LDFLAGS) $(DEF_PKG_CONFIG_LIBDIR) ./configure $(CONFIGOPTION) --prefix=$(RELEASE_DIR) --disable-xmlto --disable-alsamixer LDFLAGS="-L$(TARGET_LIB) -lpthread" GST_CFLAGS="-I$(TARGET_INC) " GST_LIBS="-L$(TARGET_LIB)" CFLAGS="-I$(TARGET_INC) -lpthread" 

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
