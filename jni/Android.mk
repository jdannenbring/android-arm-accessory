LOCAL_PATH := $(call my-dir)
 
include $(CLEAR_VARS)
 
# Here we give our module name and source file(s)
LOCAL_MODULE    := adk-service
LOCAL_SRC_FILES := adk-service.c buffer.c
 
LOCAL_LDLIBS += -L/home/jesse/Projects/Android_Accessory/android_accessory_backups/2012-8-11/Source/src_r13_2/out/target/product/imx6q_sabrelite/system/lib/
LOCAL_LDLIBS += -lusb -llog
LOCAL_C_INCLUDES += /home/jesse/Projects/Android_Accessory/android_accessory_backups/2012-8-11/Source/src_r13_2/external/libusb-android/libusb/
include $(BUILD_SHARED_LIBRARY)