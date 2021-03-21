include ../common.mk

## build

PACKAGE := $(PKG_CONFIG_DIR)/liburiparser.pc

include ../autotools.mk

$(MAKEFILE): src/Makefile.in
	cd $(dir $@) && $(CURDIR)/src/configure --disable-doc --disable-test $(CONFIGURE_OPTS)

src/Makefile.in: src/Makefile.am
	cd $(dir $@) && AUTOMAKE=automake ACLOCAL=aclocal ./autogen.sh

## fetch

GIT_REPO := https://github.com/uriparser/uriparser.git
GIT_COMMIT := uriparser-0.7.5
include ../fetch.mk

