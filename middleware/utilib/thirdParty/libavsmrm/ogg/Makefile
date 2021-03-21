include ../common.mk

## build

PACKAGE := $(PKG_CONFIG_DIR)/ogg.pc

include ../autotools.mk

$(MAKEFILE): src/Makefile.in
	cd $(dir $@) && $(CURDIR)/src/configure $(CONFIGURE_OPTS)

src/Makefile.in: src/Makefile.am
	cd $(dir $@) && ./autogen.sh

## fetch

GIT_REPO := https://github.com/xiph/ogg.git
GIT_COMMIT := v1.3.3
include ../fetch.mk

