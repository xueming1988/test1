#
# Makefile to build
#

#Directory definition
CUR_DIR		:= .
OBJ_DIR 	:=$(CUR_DIR)/obj/
SDK_DIR		:=$(BUILD_OUT_DIR)/sdk

PREFIX  :=arm-linux-gnueabihf-
LD      :=$(PREFIX)ld
CC      :=$(PREFIX)gcc

INCLUDES = \
		-I $(SDK_DIR)/include/ 


CFLAGS := $(subst -Werror, ,$(CFLAGS))
CFLAGS += $(INCLUDES)

VPATH	:= .
LIBS 	:= -L$(SDK_DIR)/lib -L$(SDK_DIR)/../install/dist/release/lib

APP_TARGET := test_soft_prep

SRC := test_soft_prep.c
OBJS := test_soft_prep.o

all release debug: $(APP_TARGET)

$(APP_TARGET):$(OBJS) 
	$(CC)  $(OBJS) $(LIBS) -lsoft_prep -lasound -lpthread -lAWELib -o $@

install: $(APP_TARGET)
	-cp test_soft_prep.c $(SDK_DIR)/src
	-cp Makefile $(SDK_DIR)/src
	-cp $(APP_TARGET) $(SDK_DIR)/src

clean:
	$(RM) -r *.o $(APP_TARGET) 

distclean:
	$(RM) -r *.o $(APP_TARGET)

