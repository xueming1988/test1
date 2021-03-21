CUR_DIR 	:= .
TOP_DIR		:= $(CUR_DIR)/../..

include $(TOP_DIR)/Rule.make
include $(TOP_DIR)/Makefile.Inc

ALLDIRS := shairport  mplayer mfi_airplay libbsa asr_client lavcresample

SUBDIRS :=

ifneq ($(IS_BACKUP_IMAGE), YES)

ifeq ($(AIRPLAY_MODULE), YES)
SUBDIRS += shairport
endif

ifneq ($(MFI_AIRPLAY_MODULE), YES)
ifeq ($(LINKPLAY_BONJOUR_SERVICE), YES)
SUBDIRS += mfi_airplay 
endif
endif

ifeq ($(MFI_AIRPLAY_MODULE), YES)
SUBDIRS += mfi_airplay
endif

#ifeq ($(MPLAY_MOUDLE), YES)
SUBDIRS += mplayer
#endif

ifeq ($(BCM_NEW_BT_ENABLE), YES)
SUBDIRS += lavcresample libbsa 
endif

ifeq ($(ASR_LIB_ENABLE), YES)
SUBDIRS += asr_client
endif


endif

all release:
	for dir in $(SUBDIRS); do ( make -C $$dir) ||exit 1; done

debug:
	for dir in $(SUBDIRS); do ( make debug -C $$dir) ||exit 1; done

clean:
	for dir in $(ALLDIRS); do ( make -C $$dir clean) ||exit 1; done

distclean:
	for dir in $(ALLDIRS); do ( make -C $$dir distclean) ||exit 1; done

