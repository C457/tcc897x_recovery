# *******************************************************************************
# Copyright (c) 1999-2016 Redbend. All Rights Reserved.
# *******************************************************************************

ifeq ($(PRODUCT_SUPPORT_OTA),redbend)

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := rec_rb_ua
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS := -DNO_RB_BBM -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -DANDROID_COMPATIBLE

LOCAL_STATIC_LIBRARIES := \
    libext4_utils_static \
    libsparse_static \
    libminzip \
    libz \
    libmtdutils \
    libmincrypt \
    libminadbd \
    libminui \
    libpixelflinger_static \
    libfs_mgr \
    libcutils \
    libselinux \
    libm \
    libc

#LOCAL_SHARED_LIBRARIES := liblog libcrypto

LOCAL_C_INCLUDES := $(LOCAL_PATH)/include \
	$(TOP)/bootable/recovery \
	$(TOP)/bionic/libc/include \
	$(TOP)/bootable/bootloader/lk/platform/tcc_shared/include \
	external/openssl/include \
	external/safe-iop/include \
	external/glib/glib \
	external/glib \
	external/zlib \
	system/core/fs_mgr/include/ \
	system/extras/ext4_utils

ifeq ($(TARGET_USERIMAGES_USE_EXT4), true)
    LOCAL_CFLAGS += -DUSE_EXT4
    LOCAL_C_INCLUDES += system/extras/ext4_utils
    LOCAL_STATIC_LIBRARIES += libext4_utils_static libz
endif

RB_LIBS := android_ndk49/rls/
LOCAL_SRC_FILES := \
	RB_Android_FSUpdate.c \
	FSImageUpdate.c \
	RB_ImageUpdate.c \
	roots.c \
	mounts.c

LOCAL_PREBUILT_OBJ_FILES= \
    ${RB_LIBS}/Mobis-AVC_arm-linux-androideabi-4.9_fw_fs.lib \
    ${NULL}

include $(BUILD_EXECUTABLE) 
endif
