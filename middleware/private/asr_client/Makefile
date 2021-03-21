TOP_DIR := $(BUILD_TOP_DIR)

include $(TOP_DIR)/Rule.make
include $(TOP_DIR)/Makefile.Inc

SUBDIRS := libs libs_test

ifeq ($(UPLUS_AIIF), YES)
SUBDIRS += asr_uplus_clova_dual
else ifeq ($(ASR_NAVER_MODULE), YES)
SUBDIRS += asr_clova
else ifeq ($(ASR_XIAOWEI_MODULE), YES)

else ifeq ($(ASR_WEIXIN_MODULE), YES)
ifeq ($(WEIXIN_NEW_SDK), YES)
SUBDIRS += asr_weixin_voip
else
SUBDIRS += asr_weixin
endif
else ifeq ($(LIFEPOD_AVS), YES)
SUBDIRS += asr_lifepod_avs
else ifeq ($(ASR_LIFEPOD_MODULE), YES)
SUBDIRS += asr_lifepod
else
SUBDIRS += asr_avs
endif

all release:
	for dir in $(SUBDIRS); do (( make -C $$dir -f Makefile.make) && ( make install -C $$dir -f Makefile.make)) ||exit 1; done

debug:
	for dir in $(SUBDIRS); do (( make debug -C $$dir -f Makefile.make) && ( make debug install -C $$dir -f Makefile.make)) ||exit 1; done

clean:
	for dir in $(SUBDIRS); do make -C $$dir clean -f Makefile.make ; done

distclean:
	for dir in $(SUBDIRS); do make -C $$dir distclean -f Makefile.make ; done

