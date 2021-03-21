CUR_DIR		:=$(shell pwd)
TOP_DIR		:=$(CUR_DIR)/../../../..

include $(TOP_DIR)/Makefile.Inc
include $(TOP_DIR)/Rule.make

ifeq ($(BUILD_PLATFORM), INGENIC)
	SUBDIRS= alsa-lib-1.0.25
else ifeq ($(BUILD_PLATFORM), AMLOGIC)
	ifeq ($(A98_BELKIN_MODULE), YES)
		SUBDIRS= alsa-lib-1.1.7 alsa-utils-1.0.23  alsa-plugins-1.0.25
	else
		SUBDIRS= alsa-lib-1.0.25 alsa-utils-1.0.23  alsa-plugins-1.0.25
	endif
else ifeq ($(BUILD_PLATFORM), QUALCOMM)
#	SUBDIRS= alsa-lib-1.0.28
	SUBDIRS= alsa-lib-1.0.25
else ifeq ($(PLATFORM_MT7688), YES)
	SUBDIRS= alsa-lib-1.0.25
else ifeq ($(BUILD_PLATFORM), ALLWINNER)
	SUBDIRS= alsa-lib-1.1.0
else
	SUBDIRS= alsa-lib-misp
endif


all release:
	for dir in $(SUBDIRS); do (( make -C $$dir -f Makefile.make) && ( make install -C $$dir -f Makefile.make)) ||exit 1; done

debug:
	for dir in $(SUBDIRS); do (( make debug -C $$dir -f Makefile.make) && ( make debug install -C $$dir -f Makefile.make)) ||exit 1; done

clean:
	for dir in $(SUBDIRS); do make -C $$dir clean -f Makefile.make ; done

distclean:
	for dir in $(SUBDIRS); do make -C $$dir distclean -f Makefile.make ; done

