TOP_DIR := $(BUILD_TOP_DIR)

include $(TOP_DIR)/Rule.make
include $(TOP_DIR)/Makefile.Inc

SUBDIRS := passthough

all release:
	for dir in $(SUBDIRS); do (( make -C $$dir -f Makefile.make) && ( make install -C $$dir -f Makefile.make)) ||exit 1; done

debug:
	for dir in $(SUBDIRS); do (( make debug -C $$dir -f Makefile.make) && ( make debug install -C $$dir -f Makefile.make)) ||exit 1; done

clean:
	for dir in $(SUBDIRS); do make -C $$dir clean -f Makefile.make ; done

distclean:
	for dir in $(SUBDIRS); do make -C $$dir distclean -f Makefile.make ; done

