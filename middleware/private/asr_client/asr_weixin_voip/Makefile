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

TARGETS := ./asr_tts
OBJS := $(patsubst %.cpp, %.o, $(wildcard *.cpp))
OBJS += $(patsubst %.c, %.o, $(wildcard *.c))

#如果您需要编译嵌入式平台下的libnassdk.so, 请设置CROSS变量
#CROSS   := arm-linux-gnueabihf-


#需要包含的头文件搜索路径
INCLUDE += -I./
INCLUDE += -I./include
INCLUDE += -I $(RELEASE_DIR)/include/ \
                        -I ../libs/capture \
			-I$(BUILD_OUT_DIR)/platform/include \
			-I$(COMMON_INC_DIR)/ \
			-I$(RELEASE_DIR)/include/json-c/ \
			-I$(RELEASE_DIR)/include/interface/ 

#需要引用的库
LIB     += -L.
LIB     += -L./lib
LIB     += -L$(RELEASE_DIR)/lib
LIB     += -lpthread -lmvmsg -lcrypto -lasound -ljson-c -lcapture -ldsp -lplayback -lnghttp2 -lcurl -lssl -lmad -lwl_util

ifeq ($(BUILD_PLATFORM), INGENIC)
	LIB 	+=  -L$(BUILD_PLATFORM_DIR)/lib  -lnvram -lwmdb
else ifeq ($(BUILD_PLATFORM), AMLOGIC)
	LIB 	+=  -L$(BUILD_PLATFORM_DIR)/lib -lgcc_s -lnvram -lwmdb -lz -lrt -lm 
else ifeq ($(BUILD_PLATFORM), QUALCOMM)
	LIB 	+=  -L$(BUILD_PLATFORM_DIR)/lib  -lnvram -lwmdb
else
	LIB 	+=  -lnvram
endif

ifneq (, $(findstring YES, $(SOFT_PREP_DSPC_V2_MODULE) $(SOFT_PREP_DSPC_V3_MODULE)))
LIBS += -lAWELib
endif

ifeq ($(SOFT_PREP_HOBOT_MODULE), YES)
LIB += -lhrsc
endif

ifeq ($(YD_XIAOWEI_HOBOT_WWE_MODULE), YES)
LIBS += -lwwe -lhrsc
endif

LIB += -lsnsr

LIB += -L$(BUILD_ROMFS_DIR)/lib -L$(BUILD_PLATFORM_DIR)/lib

LIB     += -lxiaoweiSDK -lilink_dev -lilink_im -lilink_network -lowl -lprivate_protobuf -lpthread -ldl -lrt
LIB     += -lsqlite -lconfService -lvoipChannel -lvoipCodec -lvoipComm

CFLAGS  := $(subst -Werror, ,$(CFLAGS))
CFLAGS  +=-D_LINUX_ -O2	-D_REENTRANT -fomit-frame-pointer -pipe  -Dlinux -D__linux__ -Dunix -DEMBED -D_GNU_SOURCE -fPIC -g 
CXXFLAGS+= -fpermissive -Wall -Wno-deprecated -Wno-conversion-null -fpermissive -Wno-format  -std=c++11  -fPIC -g 

#增加根据d文件自动推导编译的标志
CFLAGS += $(INCLUDE)
CXXFLAGS += $(INCLUDE)
ifneq ($(VOIP_SUPPORT), YES)
    CFLAGS += -D__NO_VOIP
    CXXFLAGS+= -D__NO_VOIP
endif

LIB += $(EXTRA_LD_FLAGS)

#deps
DEPS    += $(patsubst %.o, %.d, $(OBJS))

.PHONY: all
all:$(DEPS) $(OBJS)
	$(CXX) -o $(TARGETS) $(OBJS) $(LIB)
 
-include $(DEPS)

%.d: %.cpp
	@echo "make $@"
	@set -e $@ &&  rm -f $@; \
	$(CXX) -MM $(CFLAGS) $(CXXFLAGS) $< > $@; \

%.d: %.c
	$(CC) -MM $(CFLAGS) -c $< -o $@
	
.PHONY:install
install:
	$(MKDIR) $(INSTALL_DIR)
	$(MKDIR) $(OBJ_DIR)
	$(RM) -r $(RELEASE_DIR)/lib/libxiaoweiSDK.so
	$(CP) -R ./include $(RELEASE_DIR)/include
	$(CP) -R ./lib/lib* $(RELEASE_DIR)/lib
	$(CP) $(TARGETS) $(INSTALL_DIR)

.PHONY:clean
clean:
	rm -rf $(TARGETS) $(OBJS) $(DEPS)
