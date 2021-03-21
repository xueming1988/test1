include ../common.mk

CUR_DIR		:=$(shell pwd)
TOP_DIR		:=$(BUILD_TOP_DIR)

include $(TOP_DIR)/Makefile.Inc
include $(TOP_DIR)/Rule.make

OPENSSL_FLAGS = -I$(RELEASE_DIR)/include/ -L$(RELEASE_DIR)/lib/ -lssl  
HEADERS := $(OUT)/include/CivetServer.h $(OUT)/include/civetweb.h

all: $(OUT)/lib/libcivetweb.so $(HEADERS)

$(OUT)/lib/libcivetweb.so: src/libcivetweb.so
	install -D $< $@

src/libcivetweb.so: src/Makefile
	$(MAKE) -C src WITH_CPP=1 CFLAGS='-DNO_SSL_DL=1 $(OPENSSL_FLAGS) -W -Wall -DLINUX -Iinclude -DNDEBUG -fPIC -O2' slib

$(OUT)/include/%: src/include/%
	install -D $< $@

## Remove previously applied patches
fetch::
	[ ! -d src ] || (cd src && git checkout -- .)

GIT_REPO := https://github.com/civetweb/civetweb.git
GIT_COMMIT := v1.6
include ../fetch.mk

# Introduce a patch to insert head callback from upstream
fetch::
	echo "Applying patches"
	cd src && patch -p1 < ../request-handler.patch

clean:
	[ ! -f src/Makefile ] || $(MAKE) -C src clean
	rm -f src/libcivetweb.so
	rm -f $(OUT)/lib/libcivetweb.so
	rm -f $(HEADERS)

