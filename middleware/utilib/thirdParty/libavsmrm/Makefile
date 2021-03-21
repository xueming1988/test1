#Top Makefile for mrm libs build
include $(BUILD_TOP_DIR)/Rule.make
include $(BUILD_TOP_DIR)/Makefile.Inc

export TARGET_PRODUCT=$(BUILD_TARGET)
export CROSS_COMPILE:=arm-linux-gnueabihf-

export CC=$(CROSS_COMPILE)gcc
export CXX=$(CROSS_COMPILE)g++
export TARGET=arm-linux-gnueabihf

include common.mk

BOOT_SRC_DIR:=./boost/src
NEED_FETCH_BOOST := YES
ifeq ($(BOOT_SRC_DIR), $(wildcard $(BOOT_SRC_DIR)))
NEED_FETCH_BOOST := NO
endif
	
ALL_TARGETS := civetweb boost ffmpeg_v4 liburiparser libxml2 cjson_for_sdk_2.6 libffi glib ogg

.PHONY: all install 
all: $(ALL_TARGETS) install
	@echo -e "\033[32m"
	@echo "Build AVS MRM dependency libs SUCCESS"
	@echo -e "\033[0m" 
	
glib: libffi

boost: boost_prepare
boost_prepare:
ifeq ($(NEED_FETCH_BOOST), YES)
	rm -rf $(BOOT_SRC_DIR) 
	tar -xvf boost/boost_1_57_0.tar.gz -C boost 
	mv boost/boost_1_57_0 boost/src 
endif
	
.PHONY:install
install:
	@echo "installing mrm libs into $(RELEASE_DIR)/lib/"
	cp -rf ./out/$(TARGET_PRODUCT)/lib/* $(RELEASE_DIR)/lib/
	cp -rf ./out/$(TARGET_PRODUCT)/include/* $(RELEASE_DIR)/include/
	
# Fetch
.PHONY: fetch fetch-%
fetch: $(ALL_TARGETS:%=fetch-%)
fetch-%:
	$(MAKE) -C $(@:fetch-%=%) fetch

# Build
.PHONY: $(ALL_TARGETS)
$(ALL_TARGETS):
	$(MAKE) -C $@

# Clean
.PHONY: clean clean-%
clean: $(ALL_TARGETS:%=clean-%)
	rm -rf out/$(TARGET_PRODUCT)
clean-%:
	$(MAKE) -C $(@:clean-%=%) clean

