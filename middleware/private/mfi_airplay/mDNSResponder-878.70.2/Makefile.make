#
# Makefile to build 
#

#Directory definition
TOP_DIR         :=$(BUILD_TOP_DIR)

include $(TOP_DIR)/Rule.make
include $(TOP_DIR)/Makefile.Inc

all: mdnsd_lib


mdnsd_lib: 
	make -C mDNSPosix os=linux-uclibc STRIP="$(STRIP) -S" LD="$(LD) -shared" CC="$(CC) -fPIC"

install: mdnsd_lib
	mkdir -p $(RELEASE_DIR)/include/mDNSShared
	cp -fr mDNSShared/*.h   $(RELEASE_DIR)/include/mDNSShared/
	cp mDNSPosix/build/prod/libdns_sd.so $(RELEASE_DIR)/lib -f
	cp mDNSPosix/build/prod/mdnsd $(RELEASE_DIR)/bin -f
	cp Clients/build/dns-sd $(RELEASE_DIR)/bin -f

clean:
	make -C mDNSPosix os=linux-uclibc clean
	rm -rf mDNSPosix/objects mDNSPosix/build
