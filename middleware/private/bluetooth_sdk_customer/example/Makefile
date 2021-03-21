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

#Directory definition
CC = arm-linux-gnueabihf-gcc

INCLUDES := \
		-I $(RELEASE_DIR)/include/json-c/ \
		-I ../include/ \
		-I ../source/ \
		-I ../
		
CFLAGS := $(subst -Werror, ,$(CFLAGS))
CFLAGS  += -D_FORTIFY_SOURCE=2 -DBUILDCFG -Wall -Wextra -Wno-unused-parameter $(INCLUDES)
LIBS 	:= -L $(RELEASE_DIR)/lib -L../lib -lasound -llinkplaybt -lbsa -lpthread -ljson-c -llpresample -lgcc_s  \
                -lasound -lfdk-aac

VPATH	:= .

APP_TARGET := bt_demo

SRCS	:= 	bt_main.c  \
			alsa_pcm_render.c \
			source_sender.c \
			bt_device_list.c \
			stream_buffer.c \
			pcm_capture.c \
			bt_gatts.c 
		
ifeq ($(HFP_ENABLE), YES)
LIBS += -L./lib -lsoft_prep -lBuildGrammar -lhrsc
LIBS += -lmvmsg -lnvram -lwmdb
SRCS += hfp_call_pcm.c
CFLAGS += -DHFP_SUPPORT
endif

OBJS	:=$(patsubst %.c, $(OBJ_DIR)%.o, $(filter %.c, $(SRCS)))
DEPS	:=$(patsubst %.c, $(OBJ_DIR)%.d, $(filter %.c, $(SRCS)))


all release debug:  $(APP_TARGET)

		
$(APP_TARGET):$(OBJS) 
	$(CC) $(OBJS) $(LIBS) -o $@

install:
	$(CP) $(APP_TARGET) $(INSTALL_DIR)

clean:
	$(RM) -r *.o  $(APP_TARGET)

distclean:
	$(RM) -r *.o  $(APP_TARGET)
	$(RM) -rf Obj
include $(DEPS)
