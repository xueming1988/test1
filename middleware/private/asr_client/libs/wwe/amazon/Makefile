#
# Makefile to build
#

#Directory definition
CUR_DIR		:= .
TOP_DIR		:=$(BUILD_TOP_DIR)
OBJ_DIR 	:=$(CUR_DIR)/Obj
INSTALL_DIR	:=$(RELEASE_DIR)/lib
INCLUDE_DIR :=$(RELEASE_DIR)/include


include $(TOP_DIR)/Rule.make
include $(TOP_DIR)/Makefile.Inc

WWELIBDIR :=cortexA5
MODELDIR := models/f8

INCLUDES = \
		-I $(RELEASE_DIR)/include/ \
		-I $(BUILD_OUT_DIR)/platform/include/ \
		-I $(TOP_DIR)/application/include/ \
		-I ../../asr_avs -I ../../capture

CXXFLAGS := $(subst -Werror, ,$(CXXFLAGS))
CFLAGS := $(subst -Werror, ,$(CFLAGS))
CFLAGS += $(INCLUDES)

VPATH	:= .
LIBS 	:= -L$(RELEASE_DIR)/lib

DESTSTATICLIB := $(INSTALL_DIR)/libwwe.a

OBJS := we_amazon.o we_amazon_func.o

ifeq ($(AMAZON_DOUBLE_TRIGGER), YES)
OBJS += we_amazon_func_double.o we_amazon_new_double.o
endif

install all release debug: create_dir $(DESTSTATICLIB)

all: $(DESTSTATICLIB)
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
	cp -rfdv $(AMAZON_PRYON_LITE_DIR)/$(WWELIBDIR)/libpryon_lite.a $(INSTALL_DIR)
	rm -rf $(INSTALL_DIR)/models
	cp -rfdv $(AMAZON_PRYON_LITE_DIR)/$(MODELDIR) $(INSTALL_DIR)/models

we_amazon.o: we_amazon_new.cpp
	$(CXX) -c $(CXXFLAGS) $(INCLUDES) -o $@ $<

ifeq ($(AMAZON_DOUBLE_TRIGGER), YES)
we_amazon_new_double.o: we_amazon_new_double.cpp
	$(CXX) -c $(CXXFLAGS) $(INCLUDES) -o $@ $<
endif

create_dir:
	$(MKDIR) $(INSTALL_DIR)
	$(MKDIR) $(OBJ_DIR)
	$(MKDIR) $(INCLUDE_DIR)

clean:
	$(RM) -r *.o  $(OBJ_DIR)  $(DESTSTATICLIB)

distclean:
	$(RM) -r *.o  $(OBJ_DIR)  $(DESTSTATICLIB)

-include $(DEPS)
