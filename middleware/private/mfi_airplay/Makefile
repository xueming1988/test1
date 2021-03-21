CUR_DIR         := .
TOP_DIR         := $(CUR_DIR)/../../../

include $(TOP_DIR)/Rule.make
include $(TOP_DIR)/Makefile.Inc

MDNS_LIB_LEGACY_AIRPLAY := mDNSResponder-878.70.2
MDNS_LIB_BUFFERED_AIRPLAY2 := mDNSResponder-878.30.4
TARGET_MDNS_BUILD_DIR := mDNSResponder

SUBDIRS= mDNSResponder
ifeq ($(MFI_AIRPLAY2_MODULE), YES)
SUBDIRS += MapleSDK
else 
ifeq ($(MFI_AIRPLAY_MODULE), YES)
SUBDIRS += AirPlayAudioPOSIXReceiver
endif
endif

ifeq ($(MFI_WAC_MODULE), YES)
SUBDIRS += WAC_POSIX_Server
endif

all release: prepare 
	for dir in $(SUBDIRS); do (( make -C $$dir -f Makefile.make) && ( make install -C $$dir -f Makefile.make)) ||exit 1; done

prepare:
ifeq ($(MFI_AIRPLAY2_MODULE), YES)
	rm -f $(TARGET_MDNS_BUILD_DIR)
	ln -s $(MDNS_LIB_BUFFERED_AIRPLAY2) $(TARGET_MDNS_BUILD_DIR)
else
ifeq ($(MFI_AIRPLAY_MODULE), YES)
	rm -f $(TARGET_MDNS_BUILD_DIR)
	ln -s $(MDNS_LIB_LEGACY_AIRPLAY) $(TARGET_MDNS_BUILD_DIR)
else
ifeq ($(LINKPLAY_BONJOUR_SERVICE), YES)
	rm -f $(TARGET_MDNS_BUILD_DIR)
	ln -s $(MDNS_LIB_BUFFERED_AIRPLAY2) $(TARGET_MDNS_BUILD_DIR)
endif	
endif
endif
	
debug:
	for dir in $(SUBDIRS); do (( make debug -C $$dir -f Makefile.make) && ( make debug install -C $$dir -f Makefile.make)) ||exit 1; done

clean:
	for dir in $(SUBDIRS); do make -C $$dir clean -f Makefile.make ; done

distclean:
	for dir in $(SUBDIRS); do make -C $$dir distclean -f Makefile.make ; done

