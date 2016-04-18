LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE       :=  jnitest
LOCAL_SRC_FILES     :=  main.c
LOCAL_LDLIBS        := -L$(SYSROOT)/usr/lib -llog

include $(BUILD_SHARED_LIBRARY)