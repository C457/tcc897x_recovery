LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

ifeq ($(HAVE_FSL_EPDC_FB),true)
LOCAL_CFLAGS += -DFSL_EPDC_FB
endif

LOCAL_SRC_FILES := micomutils.cpp
LOCAL_MODULE := libmicomutils

LOCAL_LDLIBS += -lpthread

LOCAL_MODULE_TAGS := optional
include $(BUILD_STATIC_LIBRARY)
#LOCAL_MODULE:= enermysong
#include $(BUILD_EXECUTABLE)
