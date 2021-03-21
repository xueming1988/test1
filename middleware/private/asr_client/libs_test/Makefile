TOP_DIR         :=$(BUILD_TOP_DIR)

include $(TOP_DIR)/Rule.make
include $(TOP_DIR)/Makefile.Inc

SUBDIRS :=

ifeq ($(SOFT_PREP_SDK_REL), YES)

ifeq ($(SOFT_PREP_DSPC_MODULE), YES)
SUBDIRS += test_soft_prep_d_rel
endif

ifneq (, $(findstring YES, $(SOFT_PREP_DSPC_V2_MODULE) $(SOFT_PREP_DSPC_V3_MODULE)))
SUBDIRS += test_soft_prep_d_rel
endif

ifeq ($(SOFT_PREP_HOBOT_MODULE), YES)
SUBDIRS += test_soft_prep_h_rel
endif

endif


all release install:
	for dir in $(SUBDIRS); do (( make -C $$dir -f Makefile.make) && ( make install -C $$dir -f Makefile.make)) ||exit 1; done

debug:
	for dir in $(SUBDIRS); do (( make debug -C $$dir -f Makefile.make) && ( make debug install -C $$dir -f Makefile.make)) ||exit 1; done

clean:
	for dir in $(SUBDIRS); do make -C $$dir clean -f Makefile.make ; done

distclean:
	for dir in $(SUBDIRS); do make -C $$dir distclean -f Makefile.make ; done

