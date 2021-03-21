#
# Makefile to build 
#

#Directory definition
TOP_DIR         :=$(BUILD_TOP_DIR)

include $(TOP_DIR)/Rule.make
include $(TOP_DIR)/Makefile.Inc
export CC LD STRIP


all: WACServer 


WACServer: 
	make platform_makefile=Platform/Platform_OSX_Stubs/Platform.include.mk debug=0

install: WACServer
	cp WACServer $(RELEASE_DIR)/bin -f

clean:
	make platform_makefile=Platform/Platform_OSX_Stubs/Platform.include.mk clean
