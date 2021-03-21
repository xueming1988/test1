#
# Makefile to build
#

#Directory definition
CUR_DIR		:= .
TOP_DIR		:=$(BUILD_TOP_DIR)
OBJ_DIR 	:=$(CUR_DIR)/obj/
INSTALL_DIR	:=$(RELEASE_DIR)/lib
INCLUDE_DIR :=$(RELEASE_DIR)/include

include $(TOP_DIR)/Rule.make
include $(TOP_DIR)/Makefile.Inc

TEST_TARGET :=

INCLUDES = \
		-I ../soft_prep_rel/ \
		-I $(RELEASE_DIR)/include/ \
		-I $(BUILD_OUT_DIR)/platform/include/ \
		-I $(TOP_DIR)/application/include/

ifeq ($(ASR_DCS_MODULE), YES)
INCLUDES += -I ../../asr_dcs
endif

ifeq ($(OPENWRT_AVS), YES)
CFLAGS := $(subst -Werror=implicit-function-declaration, ,$(CFLAGS))
else
CFLAGS := $(subst -Werror, ,$(CFLAGS))
endif

ifeq ($(ASR_DCS_MODULE), YES)
CFLAGS := $(subst -DAVS_CLOUD_BASED_WWV, ,$(CFLAGS))
endif
CFLAGS += $(INCLUDES)

#CFLAGS += -fsanitize=address -fno-omit-frame-pointer -fno-var-tracking -g1
#CFLAGS += -fsanitize=address -fno-omit-frame-pointer -g

VPATH	:= .
LIBS 	:= -L$(RELEASE_DIR)/lib

DESTSHLIB := $(INSTALL_DIR)/libcapture.so
DESTSTATICLIB := $(INSTALL_DIR)/libcapture.a

SRCS := caiman.c ccaptureGeneral.c intercom_record.c record_resample.c \
	wave_file.c save_record_file.c playback_record.c mic_capture_server.c \
	capture_alsa.c

ifeq ($(EN_AVS_COMMS), YES)
SRCS += shm_record_comms.c
endif

ifeq ($(SOFT_PREP_DSPC_MODULE), YES)
SRCS += soft_prep_dspc.c
LIBS += -lAWELib
TEST_TARGET += soft_prep_test
endif

ifneq (, $(findstring YES, $(SOFT_PREP_DSPC_V2_MODULE) $(SOFT_PREP_DSPC_V3_MODULE)))
SRCS += soft_prep_dspc_v2.c
LIBS += -lAWELib
TEST_TARGET += soft_prep_test_v2
endif

ifeq ($(SOFT_PREP_HOBOT_MODULE), YES)
SRCS += circ_buff.c filterkit.c resample.c resamplesubs.c
ifeq ($(ASR_NAVER_MODULE), YES)
SRCS += soft_prep_hobot.c
INCLUDES += -I ./hobot_clova/
LIBS += -L./hobot_clova/ -lBuildGrammar -lhrsc
else
ifeq ($(ASR_WEIXIN_MODULE), YES)
SRCS += soft_prep_hobot_xiaowei.c
INCLUDES += -I ./hobot_xiaowei/output/hobotspeechapi/include/hobotspeechapi/
LIBS += -L./hobot_xiaowei/output/hobotspeechapi/lib/ -lhrsc
else ifeq ($(HOBOT_LEGACY_STEREO_REF), YES)
SRCS += soft_prep_hobot.c
INCLUDES += -I ./hobot_legacy_stereo_ref/include/hobotspeechapi/
LIBS += -L./hobot_legacy_stereo_ref/lib/ -lhrsc -lnvram
else
SRCS += soft_prep_hobot.c
INCLUDES += -I ./hobot/
LIBS += -L./hobot/ -lBuildGrammar -lhrsc
endif
endif
endif

ifeq ($(ASR_RECORD_SERVER), YES)
SRCS += AudioCaptureServer.c
CFLAGS += -DENABLE_RECORD_SERVER
endif

ifeq ($(ASR_TALK_VIA_FILE), YES)

SRCS += file_record.c
endif

ifeq ($(BT_HFP_ENABLE), YES)
SRCS += playback_bt_hfp.c
endif

OBJS    :=$(patsubst %.c, $(OBJ_DIR)%.o, $(filter %.c, $(SRCS)))

install all release debug: create_dir $(DESTSHLIB) $(TEST_TARGET)

ifeq ($(SOFT_PREP_DSPC_MODULE), YES)
soft_prep_dspc_func.o: soft_prep_dspc_func.cpp capture_alsa.o
	$(CXX) -c $(CFLAGS) $(INCLUDES) $(LIBS)-o soft_prep_dspc_func.o soft_prep_dspc_func.cpp
OBJS += soft_prep_dspc_func.o
endif

ifneq (, $(findstring YES, $(SOFT_PREP_DSPC_V2_MODULE) $(SOFT_PREP_DSPC_V3_MODULE)))
soft_prep_dspc_func_v2.o: soft_prep_dspc_func_v2.cpp
	$(CXX) -c $(CFLAGS) $(INCLUDES) -Os -pipe -mcpu=cortex-a53 -ftree-vectorize -mfpu=neon-fp-armv8 -mfloat-abi=hard -ffast-math -fno-caller-saves -Wno-error=unused-but-set-variable -Wno-error=unused-result -Wall -fpic -Wall -fexceptions -fpic -Wall -fexceptions -Wl,--export-dynamic -z noexecstack $(LIBS) -o soft_prep_dspc_func_v2.o soft_prep_dspc_func_v2.cpp
OBJS += soft_prep_dspc_func_v2.o
endif

ifeq ($(SOFT_PREP_HOBOT_MODULE), YES)
ifeq ($(ASR_WEIXIN_MODULE), YES)
soft_prep_hobot_xiaowei_func.o: soft_prep_hobot_xiaowei_func.cpp
	$(CXX)  -c $(CFLAGS) $(INCLUDES) -std=c++11 -o soft_prep_hobot_xiaowei_func.o soft_prep_hobot_xiaowei_func.cpp
OBJS += soft_prep_hobot_xiaowei_func.o
else
soft_prep_hobot_func.o: soft_prep_hobot_func.cpp
	$(CXX)  -c $(CFLAGS) $(INCLUDES) $(LIBS) -std=c++11 -o soft_prep_hobot_func.o soft_prep_hobot_func.cpp
OBJS += soft_prep_hobot_func.o
endif
endif

INC_FILES := caiman.h ccaptureGeneral.h file_record.h

$(DESTSHLIB): $(OBJS)
	$(RM) $(DESTSHLIB)
	$(RM) $(DESTSTATICLIB)
#	$(LD) -shared $(LDFLAGS) $(LIBS) $(OBJS)  -o $@
	$(AR) -cqs $(DESTSTATICLIB) $(OBJS)
	$(CP) -rf $(INC_FILES) $(INCLUDE_DIR)/
ifeq ($(ASR_XIAOWEI_MODULE), YES)
	$(CP) ./hobot/libhrsc.so $(INSTALL_DIR)/
	$(CP) ./caiman.h $(INCLUDE_DIR)/
	$(CP) ./ccaptureGeneral.h $(INCLUDE_DIR)/
else
ifeq ($(ASR_WEIXIN_MODULE), YES)
	$(CP) ./hobot_xiaowei/output/hobotspeechapi/lib/libhrsc.so $(INSTALL_DIR)/
	$(CP) ./hobot_xiaowei/output/hobotspeechapi/hrsc $(INSTALL_DIR)/
	$(CP) ./hobot_xiaowei/output/hrsctest $(INSTALL_DIR)/hrsc/
	$(CP) ./caiman.h $(INCLUDE_DIR)/
	$(CP) ./ccaptureGeneral.h $(INCLUDE_DIR)/
endif
endif

create_dir:
	$(MKDIR) $(INSTALL_DIR)
	$(MKDIR) $(OBJ_DIR)
	$(MKDIR) $(INCLUDE_DIR)

clean:
	$(RM) -r *.o $(DESTSTATICLIB) $(OBJ_DIR) $(RELEASE_DIR)/bin/soft_prep_test $(RELEASE_DIR)/bin/soft_prep_test_v2

distclean:
	$(RM) -r *.o $(DESTSTATICLIB) $(OBJ_DIR) $(RELEASE_DIR)/bin/soft_prep_test $(RELEASE_DIR)/bin/soft_prep_test_v2

soft_prep_test: soft_prep_dspc_func.cpp capture_alsa.o
	$(CXX) soft_prep_dspc_func.cpp capture_alsa.o $(INCLUDES) -DSOFT_PREP_DSPC_MODULE -DDSPC_MAIN -L$(RELEASE_DIR)/lib -lAWELib -lasound -lpthread -o $(RELEASE_DIR)/bin/soft_prep_test

soft_prep_test_v2: soft_prep_dspc_func_v2.cpp capture_alsa.o
	$(CXX) soft_prep_dspc_func_v2.cpp capture_alsa.o $(INCLUDES) -DSOFT_PREP_DSPC_V2_MODULE -DDSPC_MAIN -L$(RELEASE_DIR)/lib -lAWELib -lasound -lpthread -o $(RELEASE_DIR)/bin/soft_prep_test_v2

-include $(DEPS)

