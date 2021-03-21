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

ifeq ($(OPENWRT_AVS), YES)
CFLAGS := $(subst -Werror=implicit-function-declaration, ,$(CFLAGS))
else
CFLAGS := $(subst -Werror, ,$(CFLAGS))
endif

CFLAGS += $(INCLUDES)

VPATH	:= .
LIBS 	:= -L$(RELEASE_DIR)/lib

DESTSHLIB := $(INSTALL_DIR)/libdsp.so
DESTSTATICLIB := $(INSTALL_DIR)/libdsp.a

OBJS := cxdish_ctrol.o


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

