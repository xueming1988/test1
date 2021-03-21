#
# Makefile to build 
#

#Directory definition
CUR_DIR		:= .
TOP_DIR		:=$(BUILD_TOP_DIR)
OBJ_DIR 	:=$(CUR_DIR)/obj/

include $(TOP_DIR)/Rule.make
include $(TOP_DIR)/Makefile.Inc

LIB_DIR		:=$(RELEASE_DIR)/lib
INSTALL_DIR :=$(RELEASE_DIR)/bin

INCLUDES = -I$(RELEASE_DIR)/include -I $(COMMON_INC_DIR) -I $(BUILD_OUT_DIR)/platform/include/  


export PKG_CONFIG_PATH=$(RELEASE_DIR)/lib/pkgconfig

VPATH	:= .
LIBS 	+= -L$(LIB_DIR) -lm -lpthread -lasound $(shell export PKG_CONFIG_PATH=$(RELEASE_DIR)/lib/pkgconfig; pkg-config --libs openssl) -L
LIBS 	+= -L$(RELEASE_DIR)/lib -L$(BUILD_ROMFS_DIR)/lib -L$(BUILD_PLATFORM_DIR)/lib -ljson-c -lmvmsg -lrt\
        -lstdc++ 
ifeq ($(BUILD_PLATFORM), INGENIC)
	LIBS 	+=  -L$(BUILD_PLATFORM_DIR)/lib  -lnvram -lwmdb
else ifeq ($(BUILD_PLATFORM), AMLOGIC)
	LIBS 	+=  -L$(BUILD_PLATFORM_DIR)/lib  -lnvram -lwmdb
else ifeq ($(BUILD_PLATFORM), QUALCOMM)
	LIBS 	+=  -L$(BUILD_PLATFORM_DIR)/lib  -lnvram -lwmdb
else 
	LIBS 	+=  -lnvram
endif

CFLAGS := $(subst -Werror, ,$(CFLAGS))
CFLAGS  += -g -Wall $(shell export PKG_CONFIG_PATH=$(RELEASE_DIR)/lib/pkgconfig; pkg-config --cflags openssl) 

#APP_TARGET := airplay hairtunes
APP_TARGET := airplay

EXTRA_LD_FLAGS :=
ifeq ($(EXPIRE_CHECK_MODULE), YES)
	EXTRA_LD_FLAGS += -lexpire
endif
LIBS += $(EXTRA_LD_FLAGS)

SRCS_hairtunes	:= alac.c audio_alsa.c 
SRCS_shairport	:= socketlib.c shairport.c alac.c hairtunes.c audio_alsa.c  DMAP.c
			
OBJS_hairtunes	:=$(patsubst %.c, $(OBJ_DIR)%.o, $(filter %.c, $(SRCS_hairtunes)))
DEPS_hairtunes	:=$(patsubst %.c, $(OBJ_DIR)%.d, $(filter %.c, $(SRCS_hairtunes)))
OBJS_hairtunes	+=$(patsubst %.cpp, $(OBJ_DIR)%.o, $(filter %.cpp, $(SRCS_hairtunes)))
DEPS_hairtunes	+=$(patsubst %.cpp, $(OBJ_DIR)%.d, $(filter %.cpp, $(SRCS_hairtunes)))

OBJS_shairport	:=$(patsubst %.c, $(OBJ_DIR)%.o, $(filter %.c, $(SRCS_shairport)))
DEPS_shairport	:=$(patsubst %.c, $(OBJ_DIR)%.d, $(filter %.c, $(SRCS_shairport)))
OBJS_shairport +=$(patsubst %.cpp, $(OBJ_DIR)%.o, $(filter %.cpp, $(SRCS_shairport)))
DEPS_shairport	+=$(patsubst %.cpp, $(OBJ_DIR)%.d, $(filter %.cpp, $(SRCS_shairport)))

all release debug: create_dir $(APP_TARGET)

hairtunes:$(OBJS_hairtunes) hairtunes.c 
	$(CC) $(CFLAGS) $(INCLUDES) $(OBJS_hairtunes) hairtunes.c -DHAIRTUNES_STANDALONE $(LIBS) -o $@

airplay:$(OBJS_shairport) 
	$(CC) $(INCLUDES) $(OBJS_shairport) $(LIB_DIR)/libmDNS.a $(LIBS) -o $@

ifeq ($(EXPIRE_CHECK_MODULE), YES)
create_dir: clean
else
create_dir: 
endif
	$(MKDIR) $(INSTALL_DIR)
	$(MKDIR) $(OBJ_DIR)

clean:
	$(RM) -r *.o  $(OBJ_DIR) $(APP_TARGET)

distclean:
	$(RM) -r *.o  $(OBJ_DIR) $(APP_TARGET)

install:
	$(CP) $(APP_TARGET) $(INSTALL_DIR)
	
-include $(DEPS_shairport)
-include $(DEPS_hairtunes)
