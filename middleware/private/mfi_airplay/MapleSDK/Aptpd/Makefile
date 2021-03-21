TOP_DIR         :=$(BUILD_TOP_DIR)

include $(TOP_DIR)/Rule.make
include $(TOP_DIR)/Makefile.Inc

APTPD_LINUX_SRC_DIR := ./linux
APTPD_LINUX_BUILD_DIR := ./linux/build

APTPD_CFG_FILE := aptp_cfg.ini

.PHONY: clean all install 

all:
	@make -C $(APTPD_LINUX_BUILD_DIR) 

install: all
	@echo "Installing aptpd"
	cp -f $(APTPD_LINUX_BUILD_DIR)/obj/aptpd $(RELEASE_DIR)/bin/aptpd
	@mkdir -p $(RELEASE_DIR)/etc/
	cp -f $(APTPD_CFG_FILE) $(RELEASE_DIR)/etc/
	
clean:
	@make -C $(APTPD_LINUX_BUILD_DIR) clean 
	