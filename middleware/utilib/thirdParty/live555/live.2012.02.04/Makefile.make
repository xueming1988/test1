CUR_DIR		:=$(shell pwd)
TOP_DIR		:=$(BUILD_TOP_DIR)

include $(TOP_DIR)/Makefile.Inc
include $(TOP_DIR)/Rule.make

INSTALL_LIBDIR := $(RELEASE_DIR)/lib
INSTALL_INCDIR := $(RELEASE_DIR)/include/live555
DESTLIB:= ./liveMedia/libliveMedia.a
RELEASEDESTLIB:=$(addprefix $(RELEASE_DIR)/lib/, $(notdir $(DESTLIB)))

export CC
export NM
export STRIP
export AR
export RANLIB


#check if build for ARM
ifeq ($(TARGETENV),ARM)

ifeq ($(BUILD_PLATFORM), QUALCOMM)
CONFIGOPTION		:=qualcomm

else ifeq ($(BUILD_PLATFORM), LP_PLATFORM_A97)
CONFIGOPTION            :=allwinner 

else ifeq ($(BUILD_PLATFORM), ALLWINNER)
CONFIGOPTION            :=allwinner

else
CONFIGOPTION		:=armlinux
endif

else
ifeq ($(TARGETENV),MIPS)
CONFIGOPTION		:=mips

else
CONFIGOPTION		:=linux
endif
endif




all release:Makefile
	make 


configure:
	rm -rf autom4te.cache
	aclocal 
	libtoolize --force --copy
	autoheader
	autoconf
	automake -a


Makefile:  
	./genMakefiles $(CONFIGOPTION)


install:
	rsync -c liveMedia/libliveMedia.a groupsock/libgroupsock.a UsageEnvironment/libUsageEnvironment.a BasicUsageEnvironment/libBasicUsageEnvironment.a $(INSTALL_LIBDIR)
	rsync -c liveMedia/include/*.h* BasicUsageEnvironment/include/*.h* UsageEnvironment/include/*.h* groupsock/include/*.h* $(INSTALL_INCDIR)

.PHONY: clean
clean:
	-make clean


.PHONY: distclean
distclean:
	-make clean
	rm -f Makefile
