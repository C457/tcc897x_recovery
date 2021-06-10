LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

ifeq ($(HAVE_FSL_EPDC_FB),true)
LOCAL_CFLAGS += -DFSL_EPDC_FB
endif

LOCAL_CFLAGS += -DTRIMBLE_GPS_SUPPORT
LOCAL_CFLAGS += -DTRIMBLE_GPS_BISON3

LOCAL_SRC_FILES := trimble/trimble_gps_msgreceiver.cpp
LOCAL_SRC_FILES += trimble/trimble_gps_msgqueue.cpp
LOCAL_SRC_FILES += trimble/trimble_gps.cpp
LOCAL_SRC_FILES += trimble/crc32.cpp

LOCAL_C_INCLUDES += kernel/arch/arm/mach-tcc897x/include/mach
LOCAL_MODULE := libgpsutils

LOCAL_LDLIBS += -lpthread

LOCAL_MODULE_TAGS := optional
include $(BUILD_STATIC_LIBRARY)

#include $(BUILD_EXECUTABLE)
