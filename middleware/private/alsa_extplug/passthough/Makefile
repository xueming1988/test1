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
		-I ../capture/ \
		-I ./ \
		-I $(TOP_DIR)/application/include/

CFLAGS := $(subst -Werror, ,$(CPPFLAGS)) -DHAVE_CONFIG_H  -Wall -g  -O2  -MD -MP  -fPIC -DPIC

VPATH	:= .
LIBS 	:= -Wall -g -L$(RELEASE_DIR)/lib -L$(BUILD_ROMFS_DIR)/lib -g -O2 -export-dynamic -lasound 


DESTSHLIB := ./obj/libasound_module_pcm_linkplay_passthough.so

SRCS	:= pcm_passthough.c

OBJS	:=$(patsubst %.c, $(OBJ_DIR)%.o, $(filter %.c, $(SRCS)))
DEPS	:=$(patsubst %.c, $(OBJ_DIR)%.d, $(filter %.c, $(SRCS)))


all release debug: create_dir $(DESTSHLIB)
	
$(DESTSHLIB):$(OBJS)
	$(RM) $(DESTSHLIB)
	$(CC) -shared $(LDFLAGS) $(LIBS) $(OBJS) -o $@

create_dir:
	$(MKDIR) $(INSTALL_DIR)
	$(MKDIR) $(OBJ_DIR)
	$(MKDIR) $(INCLUDE_DIR)
	
install:
	
clean:
	$(RM) -r *.o  $(OBJ_DIR)

distclean:
	$(RM) -r *.o  $(OBJ_DIR)

-include $(DEPS)
