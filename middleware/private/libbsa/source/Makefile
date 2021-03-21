#
# Makefile to build
#

#Directory definition
CUR_DIR		:= .
TOP_DIR		:=$(CUR_DIR)/../../../..
OBJ_DIR 	:=$(CUR_DIR)/Obj/
INSTALL_DIR	:=$(RELEASE_DIR)/lib
INCLUDE_DIR :=$(RELEASE_DIR)/include

include $(TOP_DIR)/Rule.make
include $(TOP_DIR)/Makefile.Inc

INCLUDES = \
		-I $(BUILD_OUT_DIR)/platform/include \
		-I $(RELEASE_DIR)/include/ \
		-I $(COMMON_INC_DIR) \
		-I $(RELEASE_DIR)/include/json-c/ \
		-I ../ 			
		
CFLAGS := $(subst -Werror, ,$(CFLAGS))
CFLAGS  += -D_FORTIFY_SOURCE=2 -DBUILDCFG -Wall -Wextra -Wno-unused-parameter 

VPATH	:= .
LIBS 	:= -L$(RELEASE_DIR)/lib  -L../lib -L$(BUILD_ROMFS_DIR)/lib -lbsa -lpthread -lmvmsg -lnvram -lwmdb -lstdc++ -lasound -lwl_util 

APP_TARGET :=btManager
DESTSTATICLIB := $(INSTALL_DIR)/libbt.a

SRCS	:= 	app_ble_main.c                                        \
			app_ble.c                                        \
			app_ble_server.c                                        \
			app_xml_param.c                                       \
			app_xml_utils.c                                       \
			nanoxml.c              \
			app_utils.c            \
			app_dm.c             \
			app_mgt.c            \
			app_disc.c           \
			app_services.c  \
			app_manager_main.c  \
			app_manager.c \
			app_avk.c \
			app_avk_main.c \
			lputil_skew.c \
			cramstadasrc.c


ifeq ($(CHIPNAME), A113)
SRCS += app_av_main.c \
			app_av.c \
			app_mutex.c
endif
#ifeq ($(BT_HFP_ENABLE), YES)
SRCS += app_hs.c\
			app_hs_main.c\
			app_wav.c
#endif

OBJS	:=$(patsubst %.c, $(OBJ_DIR)%.o, $(filter %.c, $(SRCS)))
DEPS	:=$(patsubst %.c, $(OBJ_DIR)%.d, $(filter %.c, $(SRCS)))


all release debug: create_dir $(DESTSTATICLIB)

$(DESTSTATICLIB):$(OBJS)
	$(RM) $(DESTSTATICLIB)
	$(AR) -cqs $(DESTSTATICLIB) $(OBJS)
		
$(APP_TARGET):$(OBJS) 
	$(CC) $(CFLAGS) $(OBJS) $(LIBS) -o $@

create_dir:
	$(MKDIR) $(INSTALL_DIR)
	$(MKDIR) $(OBJ_DIR)
	$(MKDIR) $(INCLUDE_DIR)
	cp ../include/*.h $(INCLUDE_DIR)
	
ifeq ($(CHIPNAME), A113)
install:
	$(CP) ../lib/libbsa_a113.so $(INSTALL_DIR)/libbsa.so
	$(CP) ../lib/libfdk-aac.a   $(INSTALL_DIR)/libfdk-aac.a
else
install:
	$(CP) ../lib/libbsa.a $(INSTALL_DIR)
	$(CP) ../lib/libfdk-aac.a $(INSTALL_DIR)/libfdk-aac.a
endif

clean:
	$(RM) -r *.o  $(OBJ_DIR)

distclean:
	$(RM) -r *.o  $(OBJ_DIR)

-include $(DEPS)
