ROOT := $(realpath $(CURDIR)/..)

ifndef TARGET_PRODUCT
$(error TARGET_PRODUCT is not defined. source toolchain/<product>/envsetup.h)
endif

OUT := $(ROOT)/out/$(TARGET_PRODUCT)

PKG_CONFIG_DIR := $(OUT)/lib/pkgconfig

.PHONY: all clean fetch
