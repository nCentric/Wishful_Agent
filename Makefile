include $(TOPDIR)/rules.mk

PKG_NAME:=wishful
PKG_RELEASE:=1
PKG_MAINTAINER:=Koen Vandeputte <koen.vandeputte@ncentric.com>
PKG_BUILD_PARALLEL:=1

include $(INCLUDE_DIR)/package.mk

define Package/wishful
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=A tool for controlling the NovaNode using Wishful
  DEPENDS:=+libcurl +libpthread +libzmq +libjson-c
endef

define Package/wishful/description
 This package allows the Nova Node to be controlled
 by a wishful controller.
endef

define Build/Prepare
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	$(INSTALL_DIR) $(PKG_BUILD_DIR)/Tools
	$(INSTALL_DIR) $(PKG_BUILD_DIR)/Tools/Timer

	$(INSTALL_DATA) ./src/*.c $(PKG_BUILD_DIR)/
	$(INSTALL_DATA) ./src/*.h $(PKG_BUILD_DIR)/

	$(INSTALL_DATA) ./src/Tools/Timer/*.c $(PKG_BUILD_DIR)/Tools/Timer
	$(INSTALL_DATA) ./src/Tools/Timer/*.h $(PKG_BUILD_DIR)/Tools/Timer

	$(INSTALL_DATA) ./src/Makefile $(PKG_BUILD_DIR)/
endef

define Package/wishful/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/wishagent $(1)/usr/bin/

endef

$(eval $(call BuildPackage,wishful))
