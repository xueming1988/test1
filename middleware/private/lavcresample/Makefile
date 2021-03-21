#
# Makefile to build 
#

#Directory definition
CUR_DIR		:= .
TOP_DIR		:=$(BUILD_TOP_DIR)
OBJ_DIR 	:=$(CUR_DIR)/Obj/
INSTALL_DIR	:=$(RELEASE_DIR)/lib
INSTALL_HEADER_DIR	:=$(RELEASE_DIR)/include

include $(TOP_DIR)/Rule.make
include $(TOP_DIR)/Makefile.Inc



INCLUDES = 	-I $(RELEASE_DIR)/include/ \
						-I $(COMMON_INC_DIR) \
						-I../

CFLAGS  +=-D_LINUX_	-D_REENTRANT -fomit-frame-pointer -ffast-math -pipe  -Dlinux -D__linux__ -Dunix -DEMBED 
LDFLAGS   += -L$(RELEASE_DIR)/lib -ffast-math -lm


TARGET = liblpresample.so
OBJS = $(patsubst %.c, %.o, $(SRCS))

SRCS = $(wildcard *.c)

all:$(TARGET)
$(TARGET):$(OBJS)
	$(LD) -shared $(LIBS) $(OBJS)  -o $@
	$(CP) $(TARGET) $(INSTALL_DIR)
	$(CP) linkplay_resample_interface.h $(INSTALL_HEADER_DIR)

install:
	$(CP) $(TARGET) $(INSTALL_DIR)
	$(CP) linkplay_resample_interface.h $(INSTALL_HEADER_DIR)
clean:
	-rm -rf $(OBJS) $(TARGET)

distclean:
	$(RM) -r *.o  $(OBJS) $(TARGET)

-include $(DEPS)
