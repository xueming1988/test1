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
		-I$(RELEASE_DIR)/include/json-c/ \

CFLAGS := $(subst -Werror, ,$(CFLAGS))
CFLAGS += $(INCLUDES)

#CFLAGS += -fsanitize=address -fno-omit-frame-pointer -fno-var-tracking -g1
#CFLAGS += -fsanitize=address -fno-omit-frame-pointer -g

VPATH	:= .
LIBS 	:= -L$(RELEASE_DIR)/lib

DESTSHLIB := $(INSTALL_DIR)/libasr_utils.so
DESTSTATICLIB := $(INSTALL_DIR)/libasr_utils.a

SRC := debug_test.c asr_cfg.c common_flags.c asr_alert.c asr_light_effects.c enc_dec.c local_communication.c
OBJS := debug_test.o asr_cfg.o common_flags.o asr_alert.o asr_light_effects.o enc_dec.o local_communication.o

INC_FILES := asr_alert.h asr_cfg.h asr_light_effects.h common_flags.h debug_test.h enc_dec.h

install all release debug: create_dir $(DESTSHLIB)

$(DESTSHLIB): $(OBJS)
	$(RM) $(DESTSHLIB)
	$(RM) $(DESTSTATICLIB)
#	$(CXX) -shared $(LDFLAGS) $(LIBS) $(OBJS)  -o $@
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

