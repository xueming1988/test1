#Directory definition
CUR_DIR		:= .
TOP_DIR		:=$(BUILD_TOP_DIR)
OBJ_DIR 	:=$(CUR_DIR)/obj/
INSTALL_DIR	:=$(RELEASE_DIR)/lib
INCLUDE_DIR :=$(RELEASE_DIR)/include

include $(TOP_DIR)/Rule.make
include $(TOP_DIR)/Makefile.Inc

INCLUDES = 	-I$(RELEASE_DIR)/include/ \
			-I$(BUILD_OUT_DIR)/platform/include \
			-I$(RELEASE_DIR)/include/json-c/ \
			-I$(COMMON_INC_DIR)/ \
			-I./

INCLUDES_CPP = 	-I$(RELEASE_DIR)/include/ \
				-I$(BUILD_OUT_DIR)/platform/include \
				-I$(COMMON_INC_DIR)/ \
				-I./include \
				-I./

CFLAGS := $(subst -Werror, ,$(CFLAGS))
CFLAGS += -O2 -Wall  -Wstrict-prototypes -fPIC  -g  -fomit-frame-pointer -pipe

VPATH	:= .
LIBS 	:= -L./ -lnelo2 -L$(RELEASE_DIR)/lib -lz -lrt -luuid -lstdc++ -lcurl -lcrypto -lssl -ldl

DESTSHLIB := $(INSTALL_DIR)/libnaver_log.so
DESTSTATICLIB := $(INSTALL_DIR)/libnaver_log.a

INC_FILE := naver_log.h

OBJS := naver_log.o

install all release debug: create_dir $(DESTSHLIB)

install all release debug: create_dir $(DESTSHLIB)

naver_log.o: naver_log.cpp
	$(CXX) -c $(CXXFLAGS) $(INCLUDES_CPP) -o $@ $<

$(DESTSHLIB): $(OBJS)
	$(RM) $(DESTSHLIB)
	$(RM) $(DESTSTATICLIB)
#	$(LD) -shared $(LDFLAGS) $(LIBS) $(OBJS)  -o $@
	$(AR) -cqs $(DESTSTATICLIB) $(OBJS)
	$(CP) $(INC_FILE) $(INCLUDE_DIR)
	$(CP) libnelo2.so $(INSTALL_DIR)

create_dir:
	$(MKDIR) $(INSTALL_DIR)
	$(MKDIR) $(OBJ_DIR)
	$(MKDIR) $(INCLUDE_DIR)

debug:

clean:
	$(RM) -rf $(INSTALL_DIR)/libnaver_log.*
	$(RM) -rf $(INCLUDE_DIR)/naver_post_log*

distclean:
	$(RM) -rf $(INSTALL_DIR)/libnaver_log.*
	$(RM) -rf $(INCLUDE_DIR)/naver_post_log*

