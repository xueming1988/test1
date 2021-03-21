
TOP_DIR         :=$(BUILD_TOP_DIR)

include $(TOP_DIR)/Rule.make
include $(TOP_DIR)/Makefile.Inc

SUBDIRS := capture wwe multi_partparser

ifeq ($(ASR_NAVER_MODULE), YES)
SUBDIRS += asr_utils
else
SUBDIRS += dsp
ifeq ($(ASR_XIAOWEI_MODULE), YES)

else
ifeq ($(ASR_WEIXIN_MODULE), YES)
else
SUBDIRS +=network
endif
endif
endif

SUBDIRS += playback

ifeq ($(ASR_NAVER_MODULE), YES)
SUBDIRS += nelo2
endif

ifneq ($(ASR_NAVER_MODULE), YES)
ifeq ($(ESP_V14), YES)
SUBDIRS += esp_v14
else
SUBDIRS += esp
endif
endif

ifeq ($(SOFT_PREP_SDK_REL), YES)
SUBDIRS += soft_prep_rel
endif

ifeq ($(LGUPLUS_IPTV_ENABLE), YES)
SUBDIRS += iptv
endif

all release install:
	for dir in $(SUBDIRS); do (( make -C $$dir -f Makefile.make) && ( make install -C $$dir -f Makefile.make)) ||exit 1; done

debug:
	for dir in $(SUBDIRS); do (( make debug -C $$dir -f Makefile.make) && ( make debug install -C $$dir -f Makefile.make)) ||exit 1; done

clean:
	for dir in $(SUBDIRS); do make -C $$dir clean -f Makefile.make ; done

distclean:
	for dir in $(SUBDIRS); do make -C $$dir distclean -f Makefile.make ; done

