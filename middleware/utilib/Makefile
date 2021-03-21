CUR_DIR 	:= .
TOP_DIR		:= $(CUR_DIR)/../..

include $(TOP_DIR)/Rule.make
include $(TOP_DIR)/Makefile.Inc

SUBDIRS := thirdParty libbin

all release:
	for dir in $(SUBDIRS); do ( make -C $$dir) ||exit 1; done
	
debug:
	for dir in $(SUBDIRS); do ( make debug -C $$dir) ||exit 1; done
	
clean: 
	for dir in $(SUBDIRS); do ( make -C $$dir clean) ||exit 1; done

distclean: 
	for dir in $(SUBDIRS); do ( make -C $$dir distclean) ||exit 1; done

