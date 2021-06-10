LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	      gnutar.c \
	      append.c \
	      block.c \
	      decode.c \
	      extract.c \
	      handle.c \
	      libtar.c \
	      libtar_hash.c \
	      libtar_list.c \
	      output.c \
	      util.c \
	      wrapper.c \
	      fnmatch.c \
	      dirname.c \
	      basename.c \
	      strmode.c

LOCAL_MODULE := libtar
	
LOCAL_STATIC_LIBRARIES := libselinux libc
LOCAL_C_INCLUDES := \
       external/zlib


LOCAL_CFLAGS += -Wall

include $(BUILD_STATIC_LIBRARY)