include ../common.mk

## build
TOP_DIR	:=$(BUILD_TOP_DIR)

PACKAGE := $(PKG_CONFIG_DIR)/glib-2.0.pc

include ../autotools.mk

ifeq (arm, $(findstring arm,$(TARGET)))
cache_file := $(CURDIR)/$(dir $(MAKEFILE))glib.cache
cache_opt := --cache-file=$(cache_file)
$(cache_file): arm.cache
	mkdir -p $(dir $@)
	cp $< $@
endif

ifeq (aarch64, $(findstring aarch64,$(TARGET)))
cache_file := $(CURDIR)/$(dir $(MAKEFILE))glib.cache
cache_opt := --cache-file=$(cache_file)
$(cache_file): aarch64.cache
	mkdir -p $(dir $@)
	cp $< $@
endif


LIBFFI_CFLAGS := -I$(OUT)/lib/libffi-3.2.1/include
LIBFFI_LIBS := "-L$(OUT)/lib -lffi"

ZLIB_CFLAGS := "-I$(TOP_DIR)/linkplay_sdk/opensource/a98/include/"
ZLIB_LIBS := "-L$(TOP_DIR)/linkplay_sdk/opensource/a98/lib -lz"

$(warning CONFIGURE_OPTS=$(CONFIGURE_OPTS))
$(MAKEFILE): src/Makefile.in $(cache_file)
	cd $(dir $@) && $(CURDIR)/src/configure $(CONFIGURE_OPTS) LIBFFI_CFLAGS=$(LIBFFI_CFLAGS) LIBFFI_LIBS=$(LIBFFI_LIBS) ZLIB_CFLAGS=$(ZLIB_CFLAGS) ZLIB_LIBS=$(ZLIB_LIBS) \
        $(cache_opt) \
        --disable-libmount --with-pcre=internal \
        LDFLAGS=-Wl,-rpath-link=$(OUT)/lib:$(CURDIR)/$(dir $@)gmodule/.libs:$(TOP_DIR)/linkplay_sdk/opensource/a98/lib/

src/Makefile.in: src/Makefile.am
	cd $(dir $@) && NOCONFIGURE=1 ./autogen.sh

## fetch

GIT_REPO := https://gitlab.gnome.org/GNOME/glib.git
GIT_COMMIT := 2.54.2
include ../fetch.mk



