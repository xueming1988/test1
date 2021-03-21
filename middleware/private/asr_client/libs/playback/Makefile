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

INCLUDES = \
		-I $(RELEASE_DIR)/include/ \
		-I $(BUILD_OUT_DIR)/platform/include/ \
		-I $(TOP_DIR)/application/include/ \
		-I$(RELEASE_DIR)/include/json-c/

CFLAGS := $(subst -Werror, ,$(CFLAGS))
CFLAGS += $(INCLUDES)

#CFLAGS += -fsanitize=address -fno-omit-frame-pointer -fno-var-tracking -g1
#CFLAGS += -fsanitize=address -fno-omit-frame-pointer -g

# TODO: different platform need different marco 
#CFLAGS += -DA31
#CFLAGS += -DA76
#CFLAGS += -DA88
CFLAGS += -DA98

VPATH	:= .
LIBS 	:= -L$(RELEASE_DIR)/lib

DESTSHLIB := $(INSTALL_DIR)/libplayback.so
DESTSTATICLIB := $(INSTALL_DIR)/libplayback.a

SRC := alsa_device.c mp3_decode.c cache_buffer.c cmd_q.c
OBJS := alsa_device.o mp3_decode.o cache_buffer.o cmd_q.o

ifeq ($(ASR_NAVER_MODULE), YES)
SRC  += cid_player_naver.c url_player_naver.c wiimu_player_ctrol.c
OBJS += cid_player_naver.o url_player_naver.o wiimu_player_ctrol.o
INC_FILES := cid_player_naver.h url_player_naver.h wiimu_player_ctrol.h
else
ifeq ($(ASR_WEIXIN_MODULE), YES)
SRC  += url_player_xiaowei.c wiimu_player_ctrol.c cid_player.c
OBJS += url_player_xiaowei.o wiimu_player_ctrol.o cid_player.o
INC_FILES := url_player_xiaowei.h wiimu_player_ctrol.h cid_player.h 
else
SRC  += cid_player.c
OBJS += cid_player.o
INC_FILES := cid_player.h
endif
endif

install all release debug: create_dir $(DESTSHLIB)

$(DESTSHLIB): $(OBJS)
	$(RM) $(DESTSHLIB)
	$(RM) $(DESTSTATICLIB)
#	$(LD) -shared $(LDFLAGS) $(LIBS) $(OBJS)  -o $@
	$(AR) -cqs $(DESTSTATICLIB) $(OBJS)
	$(CP) -rf $(INC_FILES) $(INCLUDE_DIR)/

create_dir:
	$(MKDIR) $(INSTALL_DIR)
	$(MKDIR) $(INCLUDE_DIR)

clean:
	$(RM) -r *.o $(DESTSHLIB) $(DESTSTATICLIB)

distclean:
	$(RM) -r *.o $(DESTSHLIB) $(DESTSTATICLIB)

-include $(DEPS)

