CUR_DIR		:=$(shell pwd)
TOP_DIR		:=$(BUILD_TOP_DIR)

include $(TOP_DIR)/Makefile.Inc
include $(TOP_DIR)/Rule.make

DESTLIB:= ./libxml2.la
RELEASEDESTLIB:=$(addprefix $(RELEASE_DIR)/lib/, $(notdir $(DESTLIB)))

export CC
export NM
export STRIP
export AR
export RANLIB

export Z_CFLAGS=-I$(TOP_DIR)/linkplay_sdk/opensource/a98/include/ 
export Z_LIBS=-L$(TOP_DIR)/linkplay_sdk/opensource/a98/lib/ -lz

#check if build for ARM
ifeq ($(TARGETENV),ARM)
CONFIGOPTION			:= --build=i686-linux --host=arm-none-linux-gnueabi 
else
CONFIGOPTION			:=	
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
	./configure  $(CONFIGOPTION) --prefix=$(RELEASE_DIR) --with-python=$(RELEASE_DIR) --with-lzma=no


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
	rm -f Makefile configure
