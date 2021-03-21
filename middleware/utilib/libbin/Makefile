CUR_DIR 	:= .
TOP_DIR		:= $(CUR_DIR)/../../..

include $(TOP_DIR)/Rule.make
include $(TOP_DIR)/Makefile.Inc

INSTALL_DIR :=$(RELEASE_DIR)/lib
INCLUDE_DIR :=$(RELEASE_DIR)/include
BIN_DIR :=$(RELEASE_DIR)/bin

all release debug:
	mkdir -p $(RELEASE_DIR)/lib
	mkdir -p $(RELEASE_DIR)/include
ifeq ($(ASR_WEIXIN_MODULE), YES)
	-rm -r libxiaoweiSDK.so
endif
	cp -r *.a *.so $(INSTALL_DIR)
	cp -r *.h  $(INCLUDE_DIR)
ifeq ($(BUILD_PLATFORM), INGENIC)
	cp -r x1000/*.a $(INSTALL_DIR)
endif

ifeq ($(BUILD_PLATFORM), AMLOGIC)
	cp -r amlogic/*.a $(INSTALL_DIR)
endif
	
ifeq ($(XMALAYA_MODULE), YES)
	cp -r libmsc.so.shuke $(INSTALL_DIR)/libmsc.so
endif

ifeq ($(BUILDCMD), a31dosstingshu)
	cp -r libmsc.so.doss $(INSTALL_DIR)/libmsc.so
endif

ifeq ($(JD_RPC), YES)
	cp -r libmsc.so.jd $(INSTALL_DIR)/libmsc.so
endif

ifeq ($(ALI_RPC), YES)
	cp -r libopus_ali.so $(INSTALL_DIR)/libopus.so
endif

ifeq ($(BUILDCMD), a31slaveJdSrs)
	cp -r libmsc.so.srs $(INSTALL_DIR)/libmsc.so
endif

ifeq ($(YD_WEIXIN_MODULE), YES)
	cp -r  libwxvoiceembed.bin $(RELEASE_DIR)/lib/wxvoiceembed.bin
endif

ifeq ($(SOFT_PREP_DSPC_V3_MODULE), YES)
	cp -r libAWELib_belkin_noTimeOut.so                   $(INSTALL_DIR)/libAWELib.so
else

ifneq ($(findstring YES, $(SOFT_PREP_DSPC_V2_MODULE) $(SOFT_EQ_DSPC)),)

ifeq ($(MARSHALL_STEPIN_LED), YES)
	cp -r libAWELib_v2_lowheap_512k.so      $(INSTALL_DIR)/libAWELib.so
else
	cp -r libAWELib_v2.so                   $(INSTALL_DIR)/libAWELib.so
endif

endif

endif

ifeq ($(USE_PREBUILT_LIBRARY), YES)
ifeq ($(BUILD_PLATFORM), INGENIC)
	cp -r prebuilt/lib/a76/* $(INSTALL_DIR)/
	cp -r prebuilt/bin/a76/* $(BIN_DIR)/
endif
ifeq ($(BUILD_PLATFORM), AMLOGIC)
ifeq ($(SPRINT_MODULE), YES)
endif
	cp -r prebuilt/lib/a98/* $(INSTALL_DIR)/
	cp -r prebuilt/bin/a98/* $(BIN_DIR)/
endif
	cp -r prebuilt/inc/* $(INCLUDE_DIR)/
endif

clean:

distclean:
