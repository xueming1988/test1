#
# Makefile to build
#

#Directory definition
TOP_DIR         :=$(BUILD_TOP_DIR)
CUR_DIR = .

include $(TOP_DIR)/Rule.make
include $(TOP_DIR)/Makefile.Inc

CFLAGS += $(subst -Werror, ,$(CFLAGS))
export CC
export CFLAGS

SUBDIR_AIRPLAYD= $(CUR_DIR)/PlatformPOSIX
AIRPLAY_PTP_TARGET_SRC_FILE:=AirTunesPTPClock.c

#APTPD_DIR :=Aptpd

#ifeq ($(APTPD_DIR), Aptpd)
#SUBDIR_APTPD= $(CUR_DIR)/Aptpd
#AIRPLAY_PTP_SRC_FILE:=AirTunesPTPClock.c.gptp
#endif

all: airplayd 


airplayd:
	make -C $(SUBDIR_AIRPLAYD) openssl=1 sdkptp=1 mfi=1

airplayd_install: airplayd	
	cp build/Release-linux/airplaydemo $(RELEASE_DIR)/bin/airplayd -f
	
airplayd_clean:	
	make -C $(SUBDIR_AIRPLAYD) clean

#apptpd_prepare:
#	@rm ./Platform/$(AIRPLAY_PTP_TARGET_SRC_FILE) 
#	ln -s ../$(APTPD_DIR)/$(AIRPLAY_PTP_SRC_FILE) ./Platform/$(AIRPLAY_PTP_TARGET_SRC_FILE) 
	
#aptpd:
#	make -C $(SUBDIR_APTPD)  

#aptpd_clean:
#	make -C $(SUBDIR_APTPD) clean 
	
#aptpd_install: aptpd
#	make -C $(SUBDIR_APTPD) install 
	
install: airplayd_install 
	@echo "make install done."

clean:  airplayd_clean
	@echo "make clean done."
	
distclean: clean
