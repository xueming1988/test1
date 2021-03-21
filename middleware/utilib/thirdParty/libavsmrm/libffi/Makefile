include ../common.mk

## build

PACKAGE := $(PKG_CONFIG_DIR)/libffi.pc

include ../autotools.mk

$(MAKEFILE): src/Makefile.in
	cd $(dir $@) && $(CURDIR)/src/configure $(CONFIGURE_OPTS)

src/Makefile.in: src/Makefile.am
	cd $(dir $@) && ./autogen.sh

#fetch

GIT_REPO := https://github.com/libffi/libffi.git

# sony_2020 requires a different version of libffi.
ifeq (sony_2020, $(findstring sony_2020,$(TARGET_PRODUCT)))
GIT_COMMIT := v3.3-rc0
else
GIT_COMMIT := v3.2.1
endif

include ../fetch.mk
