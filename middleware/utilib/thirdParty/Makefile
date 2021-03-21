
CUR_DIR		:= .
TOP_DIR		:=$(CUR_DIR)/../../..

include $(TOP_DIR)/Rule.make
include $(TOP_DIR)/Makefile.Inc

ALLDIRS = alsa libavsmrm libupnp live555 mDMS spotify

SUBDIRS = alsa libupnp live555

ifeq ($(AIRPLAY_MODULE), YES)
SUBDIRS += mDMS
endif

ifeq ($(AVS_MRM_ENABLE), YES)
SUBDIRS += libavsmrm
endif

all release:
	for dir in $(SUBDIRS); do make -C $$dir -f Makefile.make ||exit 1; done

debug:
	for dir in $(SUBDIRS); do make debug -C $$dir -f Makefile.make ||exit 1; done

clean:
	for dir in $(ALLDIRS); do make -C $$dir clean -f Makefile.make ; done

distclean:
	for dir in $(ALLDIRS); do make -C $$dir distclean -f Makefile.make ; done

