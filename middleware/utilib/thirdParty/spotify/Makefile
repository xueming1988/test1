
CUR_DIR := .
TOP_DIR := $(BUILD_TOP_DIR)
INSTALL_DIR := $(RELEASE_DIR)/lib

include $(TOP_DIR)/Rule.make
include $(TOP_DIR)/Makefile.Inc

INCLUDES = -I $(COMMON_INC_DIR)/ \
           -I $(RELEASE_DIR)/include/json-c/ \
           -I include/

CFLAGS += -D_LINUX_ -D_REENTRANT -fomit-frame-pointer -pipe -Dlinux -D__linux__ -Dunix -DEMBED -D_GNU_SOURCE

####### Compiler, tools and options
INCPATH = $(INCLUDES)
COPY_FILE = cp
DEL_FILE = rm -f

####### Files
OBJECTS = spotify_embedded.o \
          linux_alsa.o \
          webserver.o \
          spotify_avs_cmd.o \
          sp_avs_http.o

TARGET = $(INSTALL_DIR)/libspotify.a

####### Build rules
all debug : $(TARGET)

$(TARGET): $(OBJECTS)
	$(COPY_FILE) include/spotify_api.h $(RELEASE_DIR)/include/
	$(COPY_FILE) lib/libspotify_embedded_static.a $(INSTALL_DIR)/
	$(DEL_FILE) $(TARGET)
	$(AR) cr $(TARGET) $(OBJECTS)

clean:
	$(DEL_FILE) $(OBJECTS) $(TARGET) $(RELEASE_DIR)/include/spotify_api.h $(INSTALL_DIR)/libspotify_embedded_static.a

distclean:
	$(DEL_FILE) $(OBJECTS)

####### Compile
spotify_embedded.o: spotify_embedded.c
	$(CC) -c $(CFLAGS) $(INCPATH) -o $@ $^

spotify_avs_cmd.o: spotify_avs_cmd.c
	$(CC) -c $(CFLAGS) $(INCPATH) -o $@ $^

linux_alsa.o: linux_alsa.c
	$(CC) -c $(CFLAGS) $(INCPATH) -o $@ $^

webserver.o: webserver.c
	$(CC) -c $(CFLAGS) $(INCPATH) -o $@ $^

sp_avs_http.o: sp_avs_http.cc
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -std=c++11 -o $@ $^
