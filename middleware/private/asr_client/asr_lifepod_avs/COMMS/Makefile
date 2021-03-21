#
# Makefile to build
#

#Directory definition
CUR_DIR		:= .
TOP_DIR		:=$(BUILD_TOP_DIR)
OBJ_DIR 	:=$(CUR_DIR)/Obj/
INSTALL_DIR	:=$(RELEASE_DIR)/lib
INCLUDE_DIR :=$(RELEASE_DIR)/include

include $(TOP_DIR)/Rule.make
include $(TOP_DIR)/Makefile.Inc

INCLUDES = 	-I$(RELEASE_DIR)/include/ \
			-I$(BUILD_OUT_DIR)/platform/include \
			-I$(RELEASE_DIR)/include/json-c/ \
			-I$(COMMON_INC_DIR)/ \
			-I./

INCLUDES_CPP = 	-I$(RELEASE_DIR)/include/ \
				-I$(BUILD_OUT_DIR)/platform/include \
				-I$(COMMON_INC_DIR)/ \
				-I./
ifeq ($(OPENWRT_AVS), YES)
CFLAGS := $(subst -Werror=implicit-function-declaration, ,$(CFLAGS))
else
CFLAGS := $(subst -Werror, ,$(CFLAGS))
CFLAGS += -O2 -Wall  -Wstrict-prototypes -fPIC  -g  -fomit-frame-pointer -pipe
endif
CXXFLAGS += -std=c++0x

ifneq ($(COMMS_API_V3), YES)
CXXFLAGS += -DCOMMS_APIV2
endif

DESTSHLIB := $(INSTALL_DIR)/libcomms.so
DESTSTATICLIB := $(INSTALL_DIR)/libcomms.a
APP_TARGET := asr_comms

LIBS :=

LIBPATH := $(TOP_DIR)/middleware/private/asr_client/libs/comms

ifeq ($(BUILD_PLATFORM), INGENIC)
ifeq ($(COMMS_API_V3), YES)
AMAZON_COMMS_LIB := $(LIBPATH)/amazon_lib_a76_v3
LIBS += -L$(AMAZON_COMMS_LIB)/lib -lAlexaCommsLib -lresample
else
AMAZON_COMMS_LIB := $(LIBPATH)/amazon_lib_a76
LIBS += -L$(AMAZON_COMMS_LIB)/lib -lAlexaCommsLib
endif
LIBS += -L$(BUILD_PLATFORM_DIR)/lib  -lnvram -lwmdb
else ifeq ($(BUILD_PLATFORM), AMLOGIC)
ifeq ($(CHIPNAME), A113)
AMAZON_COMMS_LIB := $(LIBPATH)/amazon_lib_a98
LIBS += -L$(AMAZON_COMMS_LIB)/lib -lAlexaCommsLib -lresample
else
ifeq ($(COMMS_API_V3), YES)
AMAZON_COMMS_LIB := $(LIBPATH)/amazon_lib_a88_v3
LIBS += -L$(AMAZON_COMMS_LIB)/lib -lAlexaCommsLib -lresample
else
AMAZON_COMMS_LIB := $(LIBPATH)/amazon_lib_a88
LIBS += -L$(AMAZON_COMMS_LIB)/lib -lAlexaCommsLib
endif
endif
LIBS += -L$(BUILD_PLATFORM_DIR)/lib  -lnvram -lwmdb -ljson-c
else ifeq ($(BUILD_PLATFORM), QUALCOMM)
AMAZON_COMMS_LIB :=
LIBS += $(LIB_NVRAM_FLAGS)
else ifeq ($(BUILD_PLATFORM), MTK)

ifeq ($(COMMS_API_V3), YES)
AMAZON_COMMS_LIB := $(LIBPATH)/amazon_lib_a31_v3
LIBS += -L$(AMAZON_COMMS_LIB)/lib -lAlexaCommsLib -lresample
else
AMAZON_COMMS_LIB := $(LIBPATH)/amazon_lib_a31
LIBS += -L$(AMAZON_COMMS_LIB)/lib -lAlexaCommsLib
endif

LIBS += -lnvram
else ifeq ($(BUILD_PLATFORM), ALLWINNER)
AMAZON_COMMS_LIB := $(TOP_DIR)/middleware/private/asr_client/libs/comms/amazon_lib_r18
LIBS += $(LIB_NVRAM_FLAGS)
endif

INCLUDES         += -I$(AMAZON_COMMS_LIB)/include
INCLUDES_CPP     += -I$(AMAZON_COMMS_LIB)/include

LIBS += -L$(RELEASE_DIR)/lib -lssl -lcrypto -lasound
LIBS += -L$(RELEASE_DIR)/lib  -L$(BUILD_ROMFS_DIR)/lib -lmvmsg

OBJS := alexa_comms_client.o notify_avs.o

alexa_comms_client.o: alexa_comms_client.cpp
	$(CXX) -c $(CXXFLAGS)  $(INCLUDES_CPP) -o $@ $<

notify_avs.o: notify_avs.cpp
	$(CXX) -c $(CXXFLAGS)  $(INCLUDES_CPP) -o $@ $<

ifeq ($(USE_COMMS_LIB), YES)
install all release debug: create_dir $(DESTSHLIB)
else
install all release debug: $(APP_TARGET)
endif

$(APP_TARGET):$(OBJS)
	$(CXX) $(CFLAGS) $(INCLUDES) $(OBJS) $(LIBS) -o $@

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
	$(RM) -r *.o $(OBJ_DIR) $(APP_TARGET)

distclean:
	$(RM) -r *.o  $(OBJ_DIR)

-include $(DEPS)
