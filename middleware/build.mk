ifneq ($(BUILD_USE_PREBUILT), true)
LOCAL_DIR := $(call current-dir)

dirs := $(LOCAL_DIR)/utilib
dirs += $(LOCAL_DIR)/private

dep_dirs := $(RELEASE_DIR)/lib
dep_dirs := $(RELEASE_DIR)/bin

include $(BUILD_CLEAR_VARS)
LOCAL_MODULE := middleware
LOCAL_DEPENDENCIES := $(call depend-on-dirs,$(dep_dirs))
LOCAL_DEPENDENCIES += platform_fs prebuilt lib_wl_util lib_mvmsg lib_wmcommon libwmrm
LOCAL_DEPENDENCIES += $(call dirs-mk-to-call,$(dirs))
LOCAL_CLEANUP_SETPS := 'for dir in $(dirs); do cd $$$$dir; make clean distclean; done;'
LOCAL_CLEANUP_FILES := $(LOCAL_DIR)/install
include $(BUILD_DUMMY_TARGET)

include $(BUILD_CLEAR_VARS)
LOCAL_MODULE := libalsa
LOCAL_DEPENDENCIES := $(call depend-on-dirs,$(dep_dirs))
alsa_dir := $(LOCAL_DIR)/utilib/thirdParty/alsa
LOCAL_DEPENDENCIES += platform_fs 
LOCAL_DEPENDENCIES += $(call dirs-mk-to-call,$(alsa_dir))
LOCAL_CLEANUP_SETPS := 'for dir in $(alsa_dir); do cd $$$$dir; make clean distclean; done;'
LOCAL_CLEANUP_FILES := $(LOCAL_DIR)/install
include $(BUILD_DUMMY_TARGET)
include $(BUILD_CLEAR_VARS)

LOCAL_MODULE := prebuilt
LOCAL_DEPENDENCIES := $(call depend-on-dirs,$(dep_dirs))
prebuilt_c_dir := $(LOCAL_DIR)/utilib/libbin
LOCAL_DEPENDENCIES += platform_fs 
LOCAL_DEPENDENCIES += $(call dirs-mk-to-call,$(prebuilt_c_dir))
LOCAL_CLEANUP_SETPS := 'for dir in $(libbin); do cd $$$$dir; make clean distclean; done;'
LOCAL_CLEANUP_FILES := $(LOCAL_DIR)/install
include $(BUILD_DUMMY_TARGET)

endif
