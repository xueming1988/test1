#Directory definition
CUR_DIR		:= .
TOP_DIR		:=$(BUILD_TOP_DIR)
OBJ_DIR 	:=$(CUR_DIR)/obj/
INSTALL_DIR	:=$(RELEASE_DIR)/lib
INCLUDE_DIR :=$(RELEASE_DIR)/include

include $(TOP_DIR)/Rule.make
include $(TOP_DIR)/Makefile.Inc

SUBDIRS :=

ifeq ($(YD_SENSORY_MODULE), YES)
SUBDIRS += sensory
else ifeq ($(SENSORY_DOUBLE_TRIGGER), YES)
SUBDIRS += sensory
endif

ifeq ($(YD_IFLYTEK_MODULE), YES)
SUBDIRS += iflytek
endif

ifeq ($(YD_AMAZON_MODULE), YES)
SUBDIRS += amazon
else ifeq ($(AMAZON_DOUBLE_TRIGGER), YES)
SUBDIRS += amazon
endif

ifeq ($(YD_XIAOWEI_HOBOT_WWE_MODULE), YES)
SUBDIRS += tencent
endif

DESTSTATICLIB := $(INSTALL_DIR)/libwwe.a

all release install:distclean
	$(RM) $(DESTSTATICLIB)
	for dir in $(SUBDIRS); do (( make -C $$dir -f Makefile.make) && ( make install -C $$dir -f Makefile.make)) ||exit 1; done

debug:
	for dir in $(SUBDIRS); do (( make debug -C $$dir -f Makefile.make) && ( make debug install -C $$dir -f Makefile.make)) ||exit 1; done

clean:
	for dir in $(SUBDIRS); do make -C $$dir clean -f Makefile.make ; done

distclean:
	for dir in $(SUBDIRS); do make -C $$dir distclean -f Makefile.make ; done

