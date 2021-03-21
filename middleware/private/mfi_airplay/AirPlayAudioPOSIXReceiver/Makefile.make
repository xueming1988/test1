#
# Makefile to build 
#

#Directory definition
TOP_DIR         :=$(BUILD_TOP_DIR)
CUR_DIR = .

include $(TOP_DIR)/Rule.make
include $(TOP_DIR)/Makefile.Inc

CFLAGS := $(subst -Werror, ,$(CFLAGS))
export CC
export CFLAGS

SUBDIR_AIRPLAYD= $(CUR_DIR)/Platform/Platform_OSX_Stubs

all: airplayd


airplayd: 
	make -C $(SUBDIR_AIRPLAYD) debug=1 openssl=0 daemon 

install: airplayd
	cp $(SUBDIR_AIRPLAYD)/obj/airplayd $(RELEASE_DIR)/bin -f

clean:
	make -C $(SUBDIR_AIRPLAYD) clean
