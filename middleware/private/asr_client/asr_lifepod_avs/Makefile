#
# Makefile to build
#

#Directory definition
CUR_DIR		:= .
TOP_DIR		:=$(BUILD_TOP_DIR)
OBJ_DIR 	:=$(CUR_DIR)/Obj/
INSTALL_DIR	:=$(RELEASE_DIR)/bin

INSTALL_LIB_DIR	:=$(RELEASE_DIR)/lib/asr_lib

include $(TOP_DIR)/Rule.make
include $(TOP_DIR)/Makefile.Inc

INCLUDES = 	-I $(RELEASE_DIR)/include/ \
			-I$(BUILD_OUT_DIR)/platform/include \
			-I$(COMMON_INC_DIR)/ \
			-I$(RELEASE_DIR)/include/json-c/ \
			-I ./ \
			-I ../libs/network \
			-I ../libs/capture \
			-I ../libs/dsp/conexant \
			-I ../libs/playback

INCLUDES_CPP = 	-I $(RELEASE_DIR)/include/ \
			-I$(COMMON_INC_DIR)/ \
			-I ./

ifeq ($(OPENWRT_AVS), YES)
CFLAGS := $(subst -Werror=implicit-function-declaration, ,$(CFLAGS))
endif
#CFLAGS  := $(OPT)  -Wall -fPIC $(DEFS) $(SCFLAGS) $(PCGLAGS)
CFLAGS  +=-D_LINUX_ -O2	-D_REENTRANT -fomit-frame-pointer -pipe  -Dlinux -D__linux__ -Dunix -DEMBED -D_GNU_SOURCE
#-DALEXA_AUTO_SWITCH

ifeq ($(ASR_RECORD_SERVER), YES)
CFLAGS += -DENABLE_RECORD_SERVER
endif

VPATH	:= .
LIBS 	:= -L$(RELEASE_DIR)/lib -L$(BUILD_ROMFS_DIR)/lib -lpthread -lmvmsg -lcrypto -llpcrypto -lasound -ljson-c -lz
LIBS += -lcapture -ldsp -lnetwork

ifeq ($(BUILD_PLATFORM), INGENIC)
	LIBS 	+=  -L$(BUILD_PLATFORM_DIR)/lib  -lnvram -lwmdb
else ifeq ($(BUILD_PLATFORM), AMLOGIC)
	LIBS 	+=  -L$(BUILD_PLATFORM_DIR)/lib  -lnvram -lwmdb -lm
else ifneq ($(LIB_NVRAM_FLAGS), "")
	LIBS 	+=  $(LIB_NVRAM_FLAGS)
else
	LIBS 	+=  -lnvram
endif

ifeq ($(ASR_FAR_FIELD), YES)
ifeq ($(SOFT_PREP_DSPC_MODULE), YES)
else
LIBS += -lwwe
endif
endif

APP_TARGET := asr_tts

EXTRA_LD_FLAGS :=
ifeq ($(EXPIRE_CHECK_MODULE), YES)
	EXTRA_LD_FLAGS += -lexpire
endif
LIBS += $(EXTRA_LD_FLAGS)

SRCS	:= main.c asr_config.c

ifeq ($(YD_AMAZON_MODULE), YES)
LIBS += -lpryon_lite
endif

ifeq ($(YD_IFLYTEK_MODULE), YES)
LIBS += -ldl -lmsc -lcurl -lssl -lz
endif

ifeq ($(YD_SENSORY_MODULE), YES)
ifeq ($(YD_SENSORY_V6_MODULE), YES)
LIBS += -lsnsr
else
LIBS += -lthf
endif
endif

ifeq ($(SENSORY_DOUBLE_TRIGGER), YES)
ifeq ($(YD_SENSORY_V6_MODULE), YES)
LIBS += -lsnsr
else
LIBS += -lthf
endif
endif

ifeq ($(YD_BAIDU_MODULE), YES)
LIBS += -lyt_wakeup_mips_16k_cn_man
endif

ifeq ($(YD_CYBERON_MODULE), YES)
LIBS += -lCSpotter
endif

ifeq ($(ASR_ENGINE), TENCENT)
CFLAGS += -DTENCENT
CXXFLAGS += -DTENCENT
endif

ifeq ($(ASR_ENGINE), BAIDU_DUMI)
CFLAGS += -DBAIDU_DUMI
CXXFLAGS += -DBAIDU_DUMI
endif

ifeq ($(AVS_BLE_ENABLE), YES)
SRCS += alexa_bluetooth.c
endif

#CFLAGS += -DASR_AMAZON_MODULE -DHAVE_PROMT -DALEXA_BETA=1 -DFORCE_LINKPLAY=1
CFLAGS += -DASR_AMAZON_MODULE -DHAVE_PROMT -DAVS_NAMED_TIMER_AND_REMINDER=1
#-DVAD_DEBUG=1

SRCS += alexa.c asr_alexa.c membuffer.c ring_buffer.c alexa_result_parse.c
SRCS += alexa_authorization.c alexa_alert.c alexa_json_common.c
SRCS +=	alexa_products.c alexa_cfg.c alexa_debug.c
SRCS +=	alexa_notification.c alexa_emplayer.c
SRCS +=	alexa_cmd.c alexa_request_json.c alexa_gui.c
SRCS +=	alexa_system.c tick_timer.c
SRCS +=	avs_player.c wiimu_player_ctrol.c avs_publish_api.c
SRCS +=	asr_state.c alexa_input_controller.c  asr_event.c alexa_donotdisturb.c
SRCS += alexa_focus_mgmt.c

LIBS += -ldl -lcurl -lssl -lz -lmad -lstdc++ -lnghttp2

ifeq ($(EN_AVS_COMMS), YES)
#fix bug amazon report
CFLAGS += -DMAX_RECORD_TIME_IN_MS=40000
SRCS += alexa_comms.c download_file.c
COMMS_TARGET := comms
endif

LIBS += -lmulti_partparser -lplayback

ifeq ($(AVS_MRM_ENABLE), YES)
SRCS += alexa_mrm.c
endif

ifeq ($(SOFT_PREP_DSPC_MODULE), YES)
LIBS += -lAWELib
TEST_TARGET += soft_prep_test 
endif

ifneq (, $(findstring YES, $(SOFT_PREP_DSPC_V2_MODULE) $(SOFT_PREP_DSPC_V3_MODULE)))
LIBS += -lAWELib
TEST_TARGET += soft_prep_test_v2
endif

ifeq ($(SOFT_PREP_HOBOT_MODULE), YES)
LIBS += -L../libs/capture/hobot/ -lBuildGrammar -lhrsc 
endif

OBJS	:=$(patsubst %.c, $(OBJ_DIR)%.o, $(filter %.c, $(SRCS)))
DEPS	:=$(patsubst %.c, $(OBJ_DIR)%.d, $(filter %.c, $(SRCS)))



ifeq ($(EN_AVS_COMMS), YES)
INCLUDES += -I./COMMS
INCLUDES_CPP += -I./COMMS
endif

AVS_ARBITRATION:=NO
ifeq ($(AVS_ARBITRATION), YES)

ifeq ($(ESP_V14), YES)
    esp_lib_dir=../libs/esp_v14
    CXXFLAGS += -DESP_V14=1
else
    esp_lib_dir=../libs/esp
endif
    LIBS += -lesp
    OBJS += alexa_arbitration.o
    INCLUDES += -I$(esp_lib_dir)/VAD_Features -I$(esp_lib_dir)/DA_Metrics
    INCLUDES_CPP += -I$(esp_lib_dir)/VAD_Features -I$(esp_lib_dir)/DA_Metrics
    CXXFLAGS := $(subst -Werror, ,$(CXXFLAGS))
endif


all release debug: create_dir $(APP_TARGET)

$(APP_TARGET):$(OBJS) $(COMMS_TARGET)
	$(CC) $(CFLAGS) $(INCLUDES) $(OBJS) $(LIBS) -o $@
	gcc vad.c -o vad -DX86 -DVAD_DEBUG=1 -DASR_FAR_FIELD=1
comms:
	make -C COMMS all

create_dir:
	$(MKDIR) $(INSTALL_DIR)
	$(MKDIR) $(OBJ_DIR)
	$(MKDIR) $(INSTALL_LIB_DIR)

link_clean:
	$(RM) $(APP_TARGET)

link:   link_clean $(APP_TARGET)

install:
	$(CP) $(APP_TARGET) $(INSTALL_DIR)

clean:
	$(RM) -rf *.o $(APP_TARGET) $(OBJ_DIR) vad test_sensory test_q test_sensory_x86 vad_test_x86
	$(RM) -rf *.o enc_file enc_str_x86 test_amazon emplayer_test cmd_test smoke_test energy_test_x86

ifeq ($(EN_AVS_COMMS), YES)
	make clean -C COMMS
endif
distclean:
	$(RM) -r *.o  $(OBJ_DIR)  $(APP_TARGET)

ifeq ($(AVS_ARBITRATION), YES)
alexa_arbitration.o: alexa_arbitration.cpp
	$(CXX) -c $(CXXFLAGS) $(INCLUDES) -o alexa_arbitration.o alexa_arbitration.cpp
endif

test_sensory: test_sensory.c
	$(CC) test_sensory.c $(INCLUDES) -L$(RELEASE_DIR)/lib -lthf -lm -o test_sensory

test_sensory_x86:
	gcc -D_FILE_OFFSET_BITS=64 test_sensory.c -lthf -lm -o test_sensory_x86 -I./x86 -L./x86

enc_file: enc_file.c enc.c
	gcc  enc_file.c enc.c -I /usr/include  -I /usr/local/ssl/include -L/usr/local/ssl/lib -L/usr/lib -lssl -lcrypto  -o enc_file

enc_str: enc_str.c enc.c
	gcc  enc_str.c enc.c -I /usr/include  -I /usr/local/ssl/include -L/usr/local/ssl/lib -L/usr/lib -lssl -lcrypto  -o enc_str

enc_str_x86: enc_str.c enc.c
	gcc enc_str.c enc.c -I /usr/include -L/usr/lib -lssl -lcrypto -o enc_str

smoke_test:
	$(CC) smoke_test.c $(INCLUDES) -L$(RELEASE_DIR)/lib -L$(BUILD_ROMFS_DIR)/lib -lgcc_s -lc -lpthread -lmvmsg -lnvram -lwmdb -o smoke_test

emplayer_test:
	$(CC) alexa_emplayer_test.c $(INCLUDES) -L$(RELEASE_DIR)/lib -L$(BUILD_ROMFS_DIR)/lib -lmvmsg -lnvram -o emplayer_test

test_amazon: test_amazon.c
	$(CXX)  test_amazon.c -I amazon/ -L amazon/ -lpryon_lite -o test_amazon
	$(STRIP) test_amazon

cmd_test:
	$(CC) alexa_event_cmd.c alexa_event_test.c $(INCLUDES) -L$(RELEASE_DIR)/lib -lmvmsg -L$(BUILD_ROMFS_DIR)/lib -lnvram -o alexa_cmd_test

soft_prep_test: soft_prep_dspc_func.cpp
	$(CXX) soft_prep_dspc_func.cpp $(INCLUDES) -DSOFT_PREP_DSPC_MODULE -DDSPC_MAIN -L$(RELEASE_DIR)/lib -lAWELib -lasound -lpthread -o soft_prep_test

soft_prep_test_v2: soft_prep_dspc_func_v2.cpp
	$(CXX) soft_prep_dspc_func_v2.cpp $(INCLUDES) -DSOFT_PREP_DSPC_V2_MODULE -DDSPC_MAIN -L$(RELEASE_DIR)/lib -lAWELib -lasound -lpthread -o soft_prep_test_v2

energy_test_x86:
	g++ -DENERGY_TEST -DX86 $(INCLUDES_CPP) $(CXXFLAGS)  alexa_arbitration.cpp  ../libs/esp/DA_Metrics/FrameEnergyComputing.cpp ../libs/esp/common/Radix4fft.c ../libs/esp/VAD_Features/VAD_class.cpp ../libs/esp/common/fixed_pt.cpp ../libs/esp/VAD_Features/xcorr_class.cpp ../libs/esp/VAD_Features/flagbuffer.cpp ../libs/esp/VAD_Features/minpeakclass.cpp  -o energy_test

-include $(DEPS)

