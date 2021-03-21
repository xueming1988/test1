#
# Makefile to build
#

#Directory definition
CUR_DIR		:= .
TOP_DIR		:=$(BUILD_TOP_DIR)
OBJ_DIR 	:=$(CUR_DIR)/obj
INSTALL_DIR	:=$(RELEASE_DIR)/lib
INCLUDE_DIR :=$(RELEASE_DIR)/include


include $(TOP_DIR)/Rule.make
include $(TOP_DIR)/Makefile.Inc

INCLUDES = \
		-I $(RELEASE_DIR)/include/ \
		-I $(BUILD_OUT_DIR)/platform/include/ \
		-I $(TOP_DIR)/application/include/ \
		-I ../../capture

CPPFLAGS := $(subst -Werror, ,$(CFLAGS))
CFLAGS := $(subst -Werror, ,$(CFLAGS))
CFLAGS += $(INCLUDES) 

#CFLAGS += -fsanitize=address -fno-omit-frame-pointer -fno-var-tracking -g1
#CFLAGS += -fsanitize=address -fno-omit-frame-pointer -g

VPATH	:= .
LIBS 	:= -L$(RELEASE_DIR)/lib

DESTSTATICLIB := $(INSTALL_DIR)/libwwe.a

SRC :=
OBJS :=

ifeq ($(YD_SENSORY_MODULE), YES)

ifeq ($(YD_SENSORY_V6_MODULE), YES)
SRCS += yd_sensory_v6.c
OBJS += yd_sensory_v6.o
LIBS += -lsnsr
else
SRCS += yd_sensory.c
OBJS += yd_sensory.o
LIBS += -lthf
endif

endif

ifeq ($(SENSORY_DOUBLE_TRIGGER), YES)

ifeq ($(YD_SENSORY_V6_MODULE), YES)
SRCS += yd_sensory_double_v6.c
OBJS += yd_sensory_double_v6.o
LIBS += -lsnsr
else
SRCS += yd_sensory_double.c
OBJS += yd_sensory_double.o
LIBS += -lthf
endif

endif

test_sensory: test_sensory.c
	$(CC) test_sensory.c $(INCLUDES) -L$(RELEASE_DIR)/lib -lthf -lm -o test_sensory
	$(STRIP) test_sensory

install all release debug: create_dir $(DESTSTATICLIB)

$(DESTSTATICLIB): $(OBJS)
ifeq (,$(wildcard $(DESTSTATICLIB)))
		echo $(DESTSTATICLIB) not exist
		$(AR) -cqs $(DESTSTATICLIB) $(OBJS)
else
		echo $(DESTSTATICLIB) exist
		$(RM) -r $(OBJ_DIR)/*
		cd $(OBJ_DIR) && $(AR) x $(DESTSTATICLIB)		
		$(AR) -cqs $(DESTSTATICLIB) $(OBJS) $(OBJ_DIR)/*.o
		$(RANLIB) $(DESTSTATICLIB)
endif

create_dir:
	$(MKDIR) $(INSTALL_DIR)
	$(MKDIR) $(INCLUDE_DIR)
	$(MKDIR) $(OBJ_DIR)

clean:
	$(RM) -r $(OBJ_DIR) *.o $(DESTSTATICLIB) test_sensory

distclean:
	$(RM) -r $(OBJ_DIR) *.o $(DESTSTATICLIB) test_sensory

-include $(DEPS)

