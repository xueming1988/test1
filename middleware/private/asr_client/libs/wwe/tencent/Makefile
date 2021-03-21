#
# Makefile to build
#

#Directory definition
CUR_DIR		:= .
TOP_DIR		:=$(BUILD_TOP_DIR)
OBJ_DIR 	:=$(CUR_DIR)/obj
INSTALL_DIR	:=$(RELEASE_DIR)/lib
INCLUDE_DIR :=$(RELEASE_DIR)/include
WWE_HOBOT_SRCS_DIR := $(TOP_DIR)/linkplay_sdk/vendor/hobot/wwe_hobot/tencent/


include $(TOP_DIR)/Rule.make
include $(TOP_DIR)/Makefile.Inc

INCLUDES = \
		-I $(RELEASE_DIR)/include/ \
		-I $(BUILD_OUT_DIR)/platform/include/ \
		-I $(TOP_DIR)/application/include/ \
		-I ../../capture/ \
		-I ../../capture/hobot_APIv2/include/

CPPFLAGS := $(subst -Werror, ,$(CFLAGS))
CFLAGS += $(INCLUDES) 

VPATH	:= .
# LIBS 	:= -L$(RELEASE_DIR)/lib -lhrsc
LIBS += -lhrsc

DESTSTATICLIB := $(INSTALL_DIR)/libwwe.a

SRCS := hobot_xiaowei_capture_client.c
OBJS := $(patsubst %.c, %.o, $(filter %.c, $(SRCS)))

install all release debug: create_dir $(DESTSTATICLIB)

hobot_xiaowei_wwe.o:hobot_xiaowei_wwe.cpp
	$(CXX) -c $(CXXFLAGS) $(INCLUDES) $(LIBS) hobot_xiaowei_wwe.cpp -o hobot_xiaowei_wwe.o
OBJS += hobot_xiaowei_wwe.o

$(DESTSTATICLIB): $(OBJS)
ifeq (,$(wildcard $(DESTSTATICLIB)))
		echo $(DESTSTATICLIB) not exist
		$(AR) -cqs $(DESTSTATICLIB) $(OBJS)
else
		echo $(DESTSTATICLIB) exist
		$(RM) -r $(OBJ_DIR)/*
		cd $(OBJ_DIR) && $(AR) x $(DESTSTATICLIB)		
		$(AR) -cqs $(DESTSTATICLIB) $(OBJS) *.o
		$(RANLIB) $(DESTSTATICLIB)
endif

create_dir:
	$(MKDIR) $(INSTALL_DIR)
	$(MKDIR) $(INCLUDE_DIR)
	$(MKDIR) $(OBJ_DIR)
# install common files 
ifneq ($(wildcard $(WWE_HOBOT_SRCS_DIR)/lib/libhrsc.so), )
	-cp -rfdv $(WWE_HOBOT_SRCS_DIR)/lib/libhrsc.so $(INSTALL_DIR)/
	-cp -rfdv $(WWE_HOBOT_SRCS_DIR)/include/*.h $(INCLUDE_DIR)/
endif
# try overwriting libs by the ones in device config 
ifneq ($(wildcard $(BUILD_DEVICE_MK_DIR)/libafe/horizon_speech_sdk/lib/libhrsc.so), )
	-cp -rfdv $(BUILD_DEVICE_MK_DIR)/libafe/horizon_speech_sdk/lib/libhrsc.so $(INSTALL_DIR)/
	-cp -rfdv $(BUILD_DEVICE_MK_DIR)/libafe/horizon_speech_sdk/include/*.h $(INCLUDE_DIR)/
endif

hrsc_demo: hrsc_sdk_demo.cc
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(LIBS) -std=c++11 hrsc_sdk_demo.cc -o hrsc_demo

clean:
	$(RM) -r $(OBJ_DIR) *.o $(DESTSTATICLIB)

distclean:
	$(RM) -r $(OBJ_DIR) *.o $(DESTSTATICLIB)

-include $(DEPS)

