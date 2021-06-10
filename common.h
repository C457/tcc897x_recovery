/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef RECOVERY_COMMON_H
#define RECOVERY_COMMON_H

#include <stdio.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LOGE(...) ui_print("E:" __VA_ARGS__)
#define LOGW(...) fprintf(stdout, "W:" __VA_ARGS__)
#define LOGI(...) fprintf(stdout, "I:" __VA_ARGS__)

#if 0
#define LOGV(...) fprintf(stdout, "V:" __VA_ARGS__)
#define LOGD(...) fprintf(stdout, "D:" __VA_ARGS__)
#else
#define LOGV(...) do {} while (0)
#define LOGD(...) do {} while (0)
#endif

#define STRINGIFY(x) #x
#define EXPAND(x) STRINGIFY(x)

static const char *SYSTEM_UPDATE_BOOT_DEV_PATH               = "/dev/block/platform/bdm/by-name/boot";
static const char *SYSTEM_UPDATE_SYSTEM_DEV_PATH             = "/dev/block/platform/bdm/by-name/system";
static const char *SYSTEM_UPDATE_DATA_DEV_PATH               = "/dev/block/platform/bdm/by-name/data";
static const char *SYSTEM_UPDATE_CACHE_DEV_PATH              = "/dev/block/platform/bdm/by-name/cache";
static const char *SYSTEM_UPDATE_RECOVERY_DEV_PATH           = "/dev/block/platform/bdm/by-name/recovery";
static const char *SYSTEM_UPDATE_SPLASH_DEV_PATH             = "/dev/block/platform/bdm/by-name/splash";
static const char *SYSTEM_UPDATE_DTB_DEV_PATH                = "/dev/block/platform/bdm/by-name/dtb";

static const char *SYSTEM_UPDATE_QUICKBOOT_DEV_PATH          = "/dev/block/platform/bdm/by-name/snapshot";
static const char *SYSTEM_UPDATE_QBDATA_DEV_PATH             = "/dev/block/platform/bdm/by-name/qb_data";
static const char *SYSTEM_UPDATE_UPGRADE_DEV_PATH            = "/dev/block/platform/bdm/by-name/upgrade";
static const char *SYSTEM_UPDATE_VR1_DEV_PATH                = "/dev/block/platform/bdm/by-name/vr1";
static const char *SYSTEM_UPDATE_VR2_DEV_PATH                = "/dev/block/platform/bdm/by-name/vr2";

static const char *SYSTEM_UPDATE_RECOVERY_MIRROR_DEV_PATH    = "/dev/block/platform/bdm/by-name/recovery_mirror";
static const char *SYSTEM_UPDATE_SPLASH_MIRROR_DEV_PATH      = "/dev/block/platform/bdm/by-name/splash_mirror";
static const char *SYSTEM_UPDATE_BOOT_MIRROR_DEV_PATH        = "/dev/block/platform/bdm/by-name/boot_mirror";

static const char *SYSTEM_UPDATE_BOOT_OEMDATA_PATH           = "/dev/block/platform/bdm/by-name/oemdata";
static const char *SYSTEM_UPDATE_LOG_DEV_PATH                = "/dev/block/platform/bdm/by-name/log";
static const char *SYSTEM_UPDATE_NAVI_DEV_PATH               = "/dev/block/platform/bdm/by-name/navi";
static const char *SYSTEM_UPDATE_NAVI2_DEV_PATH              = "/dev/block/platform/bdm/by-name/navi2";

typedef struct fstab_rec Volume;

// fopen a file, mounting volumes and making parent dirs as necessary.
FILE* fopen_path(const char *path, const char *mode);

void ui_print(const char* format, ...);

#ifdef __cplusplus
}
#endif

#endif  // RECOVERY_COMMON_H
