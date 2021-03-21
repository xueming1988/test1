#
# Makefile to build 
#

#Directory definition
CUR_DIR		:= .
TOP_DIR		:=$(BUILD_TOP_DIR)
OBJ_DIR 	:=$(CUR_DIR)/obj/
SDK_DIR		:=$(BUILD_OUT_DIR)/sdk
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

CPPFLAGS := $(subst -Werror, ,$(CPPFLAGS)) -fPIC 

VPATH	:= .
LIBS 	:= -L$(RELEASE_DIR)/lib -L$(BUILD_ROMFS_DIR)/lib -lstdc++ 

LIBNAME = libsoft_prep.so
SONAME = $(LIBNAME)
LDFLAGS += -Wl,-soname -Wl,$(SONAME)

DESTSHLIB := $(INSTALL_DIR)/$(LIBNAME)

SRCS	:= soft_prep_rel_interface.c soft_prep_rel_dummy.c

OBJS	:=$(patsubst %.c, $(OBJ_DIR)%.o, $(filter %.c, $(SRCS)))
DEPS	:=$(patsubst %.c, $(OBJ_DIR)%.d, $(filter %.c, $(SRCS)))


all release debug: create_dir $(DESTSHLIB)
	
$(DESTSHLIB):$(OBJS)
	$(RM) $(DESTSHLIB)
	$(CC) -shared $(LDFLAGS) $(LIBS) $(OBJS) -o $@  -lcapture

create_dir:
	$(MKDIR) $(INSTALL_DIR)
	$(MKDIR) $(OBJ_DIR)
	$(MKDIR) $(INCLUDE_DIR)
	$(MKDIR) $(SDK_DIR)/include
	$(MKDIR) $(SDK_DIR)/lib
	$(MKDIR) $(SDK_DIR)/src
	
install:
	-$(CP) $(DESTSHLIB) $(SDK_DIR)/lib
	-$(CP) *.h $(SDK_DIR)/include
	-$(CP) release_note.txt $(SDK_DIR)/lib
	
clean:
	$(RM) -r *.o  $(OBJ_DIR)

distclean:
	$(RM) -r *.o  $(OBJ_DIR)

-include $(DEPS)
