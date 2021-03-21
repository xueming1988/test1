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
			-I$(RELEASE_DIR)/include/json-c/ \
			-I$(RELEASE_DIR)/include/interface/ \
			-I$(RELEASE_DIR)/include/interface/app/ \
			-I$(RELEASE_DIR)/include/interface/app/voip/ \
			-I$(RELEASE_DIR)/include/interface/app/xiaowei/ \
			-I ../libs/capture \
			-I ../

INCLUDES_CPP = 	-I $(RELEASE_DIR)/include/ \
			-I$(COMMON_INC_DIR)/ \

CFLAGS := $(subst -Werror, ,$(CFLAGS))
#CFLAGS  := $(OPT)  -Wall -fPIC $(DEFS) $(SCFLAGS) $(PCGLAGS)
CFLAGS  +=-D_LINUX_ -O2	-D_REENTRANT -fomit-frame-pointer -pipe  -Dlinux -D__linux__ -Dunix -DEMBED -D_GNU_SOURCE

VPATH	:= .
LIBS 	:= -L$(RELEASE_DIR)/lib -lpthread -lmvmsg -lcrypto -lasound -ljson-c -lcapture -ldsp -lplayback -lnghttp2 -lcurl -lssl -lmad -lwl_util 
ifeq ($(BUILD_PLATFORM), INGENIC)
	LIBS 	+=  -L$(BUILD_PLATFORM_DIR)/lib  -lnvram -lwmdb
else ifeq ($(BUILD_PLATFORM), AMLOGIC)
	LIBS 	+=  -L$(BUILD_PLATFORM_DIR)/lib -lgcc_s -lnvram -lwmdb -lz -lrt -lm 
else ifeq ($(BUILD_PLATFORM), QUALCOMM)
	LIBS 	+=  -L$(BUILD_PLATFORM_DIR)/lib  -lnvram -lwmdb
else
	LIBS 	+=  -lnvram
endif

ifneq (, $(findstring YES, $(SOFT_PREP_DSPC_V2_MODULE) $(SOFT_PREP_DSPC_V3_MODULE)))
LIBS += -lAWELib
endif

ifeq ($(SOFT_PREP_HOBOT_MODULE), YES)
LIBS += -lhrsc
endif

ifeq ($(YD_XIAOWEI_HOBOT_WWE_MODULE), YES)
LIBS += -lwwe -lhrsc
endif

LIBS += -lsnsr

LIBS += -L$(BUILD_ROMFS_DIR)/lib -L$(BUILD_PLATFORM_DIR)/lib -L./lib -lstdc++ -lxiaoweiSDK -lilink_ota -lilink_dev -lilink_im -lowl -lprivate_protobuf -lilink_network

APP_TARGET := asr_tts

SRCS := main.c asr_qq.c xiaowei_interface.c ring_buffer.c xiaowei_player.c

LIBS += $(EXTRA_LD_FLAGS)

OBJS	:=$(patsubst %.c, $(OBJ_DIR)%.o, $(filter %.c, $(SRCS)))
DEPS	:=$(patsubst %.c, $(OBJ_DIR)%.d, $(filter %.c, $(SRCS)))


all release debug: create_dir $(APP_TARGET)

$(APP_TARGET):$(OBJS)
	$(CC) -g $(CFLAGS) $(INCLUDES) $(OBJS) -o $@ $(LIBS) 

create_dir:
	$(MKDIR) $(INSTALL_DIR)
	$(MKDIR) $(OBJ_DIR)
	$(RM) -r $(RELEASE_DIR)/lib/libxiaoweiSDK.so
	$(CP) -R ./interface $(RELEASE_DIR)/include
	$(CP) -R ./lib/lib* $(RELEASE_DIR)/lib

link_clean:
	$(RM) $(APP_TARGET)

install:
	$(CP) $(APP_TARGET) $(INSTALL_DIR)

clean:
	$(RM) -r *.o  $(OBJ_DIR) $(APP_TARGET)

distclean:
	$(RM) -r *.o  $(OBJ_DIR)  $(APP_TARGET)

-include $(DEPS)
