## Template for autotools-like packages

## Input params:
##   PACKAGE - target to build
##   MAKEFILE (optional) - name of the makefile to call
## Output params:
##   CONFIGURE_OPTS - default configure options: -- prefix, --target etc.

ifndef PACKAGE
$(error PACKAGE undefined!)
endif

ifdef TARGET
options_target := --target=${TARGET} --host=${TARGET}
endif

export ACLOCAL_PATH := $(OUT)/share/aclocal

# Supposedly it should work for all targets
options_pkg_config := PKG_CONFIG_LIBDIR= PKG_CONFIG_PATH=$(PKG_CONFIG_DIR)

CONFIGURE_OPTS := --prefix=$(OUT) $(options_target) $(options_pkg_config)

# By default build outside of source tree.
# If package does not support it, it can define MAKEFILE variable
MAKEFILE ?= out/$(TARGET_PRODUCT)/Makefile
make_dir := $(dir $(MAKEFILE))
make_notdir := $(notdir $(MAKEFILE))

ifeq (,$(findstring $(TARGET_PRODUCT), $(MAKEFILE)))
$(warning Package $(PACKAGE) builds inside of source tree.)
endif

define autotool_rules
## build
all: $(PACKAGE)

$(PACKAGE): $(MAKEFILE)
	$$(MAKE) -C $(make_dir) -f $(make_notdir)
	$$(MAKE) -C $(make_dir) -f $(make_notdir) install

$(MAKEFILE): | $(make_dir)

$(make_dir):
	mkdir -p $(make_dir)

## clean
clean::
	-[ ! -f $(MAKEFILE) ] || $$(MAKE) -C $(make_dir) -f $(make_notdir) uninstall distclean
	rm -f $(MAKEFILE)
	rm -rf out/$(TARGET_PRODUCT)
endef # autotools_rules

$(eval $(autotool_rules))


