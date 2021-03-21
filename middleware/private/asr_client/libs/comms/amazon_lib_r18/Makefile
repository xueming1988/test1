include $(TOPDIR)/rules.mk
include $(BUILD_DIR)/package.mk

PKG_NAME:=libAlexaCommsLib
PKG_VERSION:=1.0
PKG_RELEASE:=1.0


define Package/$(PKG_NAME)
  SECTION:=avs
  CATEGORY:=sgw
  DEPENDS:=+libstdcpp
  TITLE:=Alexa Comms
endef

define Package/$(PKG_NAME)/description
	It's comms for alexa.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) -r ./* $(PKG_BUILD_DIR)/
endef

define Build/Compile
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/include/AlexaCommsLib $(1)/usr/lib/AlexaCommsLib
	$(CP) $(PKG_BUILD_DIR)/include/*.h $(1)/usr/include/AlexaCommsLib
	$(CP) $(PKG_BUILD_DIR)/libs/lib*.so $(1)/usr/lib/AlexaCommsLib
	$(CP) $(PKG_BUILD_DIR)/libs/lib*.so.* $(1)/usr/lib/AlexaCommsLib
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/libs/lib*.so $(1)/usr/lib/
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/libs/lib*.so* $(1)/usr/lib/
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
