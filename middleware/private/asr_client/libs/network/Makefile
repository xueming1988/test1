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
		-I $(TOP_DIR)/application/include/

ifeq ($(ASR_DCS_MODULE), YES)
INCLUDES += -I ../../asr_dcs
else
INCLUDES += -I ../../asr_avs
endif

ifeq ($(OPENWRT_AVS), YES)
CFLAGS := $(subst -Werror=implicit-function-declaration, ,$(CFLAGS))
else
CFLAGS := $(subst -Werror, ,$(CFLAGS))
endif

ifeq ($(ASR_DCS_MODULE), YES)
CFLAGS += -DBAIDU_DUMI
endif

CFLAGS += $(INCLUDES)

#CFLAGS += -fsanitize=address -fno-omit-frame-pointer -fno-var-tracking -g1
#CFLAGS += -fsanitize=address -fno-omit-frame-pointer -g

VPATH	:= .
LIBS 	:= -L$(RELEASE_DIR)/lib

DESTSHLIB := $(INSTALL_DIR)/libnetwork.so
DESTSTATICLIB := $(INSTALL_DIR)/libnetwork.a

#OBJS := curl_https.o alexa_nghttp2.o
OBJS := curl_https.o http2_client.o
ifeq ($(LIFEPOD_AVS), YES)
OBJS += http2_client2.o
endif

install all release debug: create_dir $(DESTSHLIB)


$(DESTSHLIB): $(OBJS)
	$(RM) $(DESTSHLIB)
	$(RM) $(DESTSTATICLIB)
#	$(LD) -shared $(LDFLAGS) $(LIBS) $(OBJS)  -o $@
	$(AR) -cqs $(DESTSTATICLIB) $(OBJS)


create_dir:
	$(MKDIR) $(INSTALL_DIR)
	$(MKDIR) $(OBJ_DIR)
	$(MKDIR) $(INCLUDE_DIR)

clean:
	$(RM) -r *.o  $(OBJ_DIR)

distclean:
	$(RM) -r *.o  $(OBJ_DIR)

-include $(DEPS)

