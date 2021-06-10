LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := cipher.c
LOCAL_MODULE := libcipher
LOCAL_STATIC_LIBRARIES := libselinux libc
LOCAL_CFLAGS += -Wall

include $(BUILD_STATIC_LIBRARY)
