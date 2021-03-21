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

CPPFLAGS := $(subst -Werror, ,$(CPPFLAGS))
CPPFLAGS += -fPIC

VPATH	:= .
LIBS 	:= -L$(RELEASE_DIR)/lib

DESTSHLIB := $(INSTALL_DIR)/libesp.so
DESTSTATICLIB := $(INSTALL_DIR)/libesp.a

OBJS := flagbuffer.o xcorr_class.o VAD_class.o minpeakclass.o fixed_pt.o FrameEnergyComputing.o Radix4fft.o
LIBS += -lstdc++

FrameEnergyComputing.o: DA_Metrics/FrameEnergyComputing.cpp
	$(CXX) -c $(CPPFLAGS) $(INCLUDES) -o $@ $<

VAD_class.o: VAD_Features/VAD_class.cpp
	$(CXX) -c $(CPPFLAGS) $(INCLUDES) -o $@ $<

minpeakclass.o: VAD_Features/minpeakclass.cpp
	$(CXX) -c $(CPPFLAGS) $(INCLUDES) -o $@ $<

xcorr_class.o: VAD_Features/xcorr_class.cpp
	$(CXX) -c $(CPPFLAGS) $(INCLUDES) -o $@ $<

flagbuffer.o: VAD_Features/flagbuffer.cpp
	$(CXX) -c $(CPPFLAGS) $(INCLUDES) -o $@ $<

fixed_pt.o: common/fixed_pt.cpp
	$(CXX) -c $(CPPFLAGS) $(INCLUDES) -o $@ $<

Radix4fft.o: common/Radix4fft.c
	$(CC) -c $(CPPFLAGS) $(INCLUDES) -o $@ $<


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

