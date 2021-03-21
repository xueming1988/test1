#
# Makefile to build
#

#Directory definition
CUR_DIR		:= .
TOP_DIR		:=$(BUILD_TOP_DIR)
OBJ_DIR 	:=$(CUR_DIR)/Obj/
INSTALL_DIR	:=$(RELEASE_DIR)/bin

include $(TOP_DIR)/Rule.make
include $(TOP_DIR)/Makefile.Inc

INCLUDES = 	-I $(RELEASE_DIR)/include/ \
			-I$(BUILD_OUT_DIR)/platform/include \
			-I$(COMMON_INC_DIR)/ \
			-I$(TOP_DIR)/application/include \
			-I$(RELEASE_DIR)/include/json-c/ \
			-I ./

INCLUDES_CPP = 	-I $(RELEASE_DIR)/include/ \
			-I$(BUILD_OUT_DIR)/platform/include \
			-I$(TOP_DIR)/application/include \
			-I$(COMMON_INC_DIR)/ \
			-I$(RELEASE_DIR)/include/json-c/ \
			-I ./

CFLAGS := $(subst -Werror, ,$(CFLAGS))
CFLAGS  +=-D_LINUX_ -O2	-D_REENTRANT -fomit-frame-pointer -pipe -Dlinux -D__linux__ -Dunix -DEMBED

#CFLAGS += -fsanitize=address -fno-omit-frame-pointer -fno-var-tracking -g1
#CFLAGS += -fsanitize=address -fno-omit-frame-pointer -g

VPATH	:= .
LIBS 	:= -L$(RELEASE_DIR)/lib -L$(BUILD_ROMFS_DIR)/lib -lpthread -lmvmsg -lcrypto -lasound -ljson-c -lwl_util 
ifeq ($(BUILD_PLATFORM), INGENIC)
	LIBS 	+=  -L$(BUILD_PLATFORM_DIR)/lib  -lnvram -lwmdb
else ifeq ($(BUILD_PLATFORM), AMLOGIC)
	LIBS 	+=  -L$(BUILD_PLATFORM_DIR)/lib  -lnvram -lwmdb
else ifeq ($(BUILD_PLATFORM), QUALCOMM)
	LIBS 	+=  -L$(BUILD_PLATFORM_DIR)/lib  -lnvram -lwmdb
else
	LIBS 	+=  -lnvram
endif

APP_TARGET := asr_tts
TEST_TARGET :=
EXTRA_LD_FLAGS :=

LIBS += $(EXTRA_LD_FLAGS)

SRCS	:= main.c

SRCS += ring_buffer.c asr_json_common.c http2_client.c asr_debug.c \
			asr_event.c asr_cmd.c asr_request_json.c tick_timer.c \
			asr_session.c asr_authorization.c asr_player.c asr_bluetooth.c \
			asr_notification.c asr_result_parse.c asr_record.c asr_system.c \
			clova_context.c clova_speech_recognizer.c \
			clova_playback_controller.c clova_device_control.c \
			clova_device.c clova_clova.c clova_settings.c clova_dnd.c clova_update.c

INCLUDES += -I ../libs/playback -I ../libs/capture -I ../libs/asr_utils

LIBS += -ldl -lcurl -lssl -lz -lmad -lstdc++ -lnghttp2 -lplayback -lasr_utils -lcapture -lwwe -lmulti_partparser

LIBS += -lrt -lnaver_log -lnelo2 -luuid

ifeq ($(YD_SENSORY_MODULE), YES)
ifeq ($(YD_SENSORY_V6_MODULE), YES)
LIBS += -lsnsr
else
LIBS += -lthf
endif
endif

ifeq ($(SOFT_PREP_HOBOT_MODULE), YES)
LIBS += -L../libs/capture/hobot_clova/ -lBuildGrammar -lhrsc
endif

ifneq (, $(findstring YES, $(SOFT_PREP_DSPC_V2_MODULE) $(SOFT_PREP_DSPC_V3_MODULE)))
LIBS += -lAWELib
endif

ifeq ($(LGUPLUS_IPTV_ENABLE), YES)
SRCS += iptv_msg.c
LIBS += -liptv
endif

OBJS	:=$(patsubst %.c, $(OBJ_DIR)%.o, $(filter %.c, $(SRCS)))
DEPS	:=$(patsubst %.c, $(OBJ_DIR)%.d, $(filter %.c, $(SRCS)))

all release debug: create_dir $(APP_TARGET) $(TEST_TARGET)

$(APP_TARGET):$(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) $(OBJS) $(LIBS) -o $@ -lm

create_dir:
	$(MKDIR) $(INSTALL_DIR)
	$(MKDIR) $(OBJ_DIR)

link_clean:
	$(RM) $(APP_TARGET)

link:   link_clean $(APP_TARGET)

install:
	$(CP) $(APP_TARGET) $(TEST_TARGET) $(INSTALL_DIR)

clean:
	$(RM) -rf *.o $(APP_TARGET) $(OBJ_DIR)

distclean:
	$(RM) -r *.o  $(OBJ_DIR)  $(APP_TARGET)

-include $(DEPS)

