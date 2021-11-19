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

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <linux/input.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "bootloader.h"
#include "common.h"
#include "cutils/properties.h"
#include "cutils/android_reboot.h"
#include "install.h"
#include "minui/minui.h"
#include "minzip/DirUtil.h"
#include "roots.h"
#include "ui.h"
#include "screen_ui.h"
#include "device.h"
#include "qb_function.h"
#include "recovery_errno.h"
#include "rsa.h"

#include "micomutils/micomutils.h"
#include "tar/untar.h"
#include "update_bootloader.h"
#include "cipher/cipher.h"

#include "./gpsutils/trimble/trimble_gps.h"

extern "C" {
#include "lgit9x28modemutils/include/lgit_9x28_api.h"
}

//#define UPDATE_FILE_SIGNING	1

struct selabel_handle *sehandle;

static const struct option OPTIONS[] = {
    { "send_intent", required_argument, NULL, 's' },
    { "update_package", required_argument, NULL, 'u' },
    { "wipe_data", no_argument, NULL, 'w' },
    { "wipe_cache", no_argument, NULL, 'c' },
    { "show_text", no_argument, NULL, 't' },
    { "just_exit", no_argument, NULL, 'x' },
    { "locale", required_argument, NULL, 'l' },
    { "wipe_snapshot", no_argument, NULL, 'n' },
    { "extract_qbimg", required_argument, NULL, 'q' },
    { "extract_qb_data_only", no_argument, NULL, 'y' },
    { "restore_qb_data", required_argument, NULL, 'r' },
    { NULL, 0, NULL, 0 },
};

#define LAST_LOG_FILE           "/cache/recovery/last_log"

enum {
    PRODUCT_MODEL = 0,
    PRODUCT_REGION,
    PRODUCT_OPTION,
    PRODUCT_VERSUIN,
    PRODUCT_DATE,
    PRODUCT_MAX,
};

static const char *CACHE_LOG_DIR 		= "/cache/recovery";
static const char *COMMAND_FILE 		= "/cache/recovery/command";
static const char *INTENT_FILE 			= "/cache/recovery/intent";
static const char *LOG_FILE 			= "/cache/recovery/log";
static const char *LAST_INSTALL_FILE 	= "/cache/recovery/last_install";
static const char *LOCALE_FILE 			= "/upgrade/last_locale";

const char *CACHE_ROOT 			= "/cache";
const char *USERDATA_ROOT 		= "/data";
const char *SNAPSHOT_ROOT 		= "/snapshot";
const char *SDCARD_ROOT 		= "/sdcard";
const char *INTERNAL_ROOT 		= "/data/media/0";
const char *USB_ROOT 			= "/usb";
const char *QB_INTERNAL_ROOT 	= "/qb_data";
const char *SYSTEM_ROOT 		= "/system";
const char *TEMPORARY_DIR_ROOT 	= "/tmp";


// defined about update
const char *QUICKBOOT_UPDATED_FLAG = "/upgrade/update.cfg";


// defined about NAVI
const char *NAVI_ROOT 						= "/navi";
const char *NAVI_MAIN_PATH 					= "/navi/navi";
const char *NAVI_UPDATE_PATH 				= "/navi/navi_backup";

const char *NAVI_MIRROR_ROOT 				= "/navi2";
const char *NAVI_MIRROR_MAIN_PATH 			= "/navi2/navi";
const char *NAVI_MIRROR_UPDATE_PATH 		= "/navi2/navi_backup";

const char *NAVI_UPDATED_FLAG 				= "/upgrade/map_update_complete.cfg";
const char *NAVI_INDEX_INFO 				= "/oem_data/map_index.cfg";

const char *NAVI_FOREGROUND_UPDATE_WORKING 	= "/upgrade/map_updating.cfg";


// defined about VR
const char *VR1_ROOT 			= "/vr1";
const char *VR2_ROOT 			= "/vr2";
const char *VR1_MAIN_PATH 		= "/vr1/vr1";
const char *VR2_MAIN_PATH 		= "/vr2/vr2";
const char *VR2_UPDATE_PATH 	= "/vr2/vr2_backup";
const char *VR1_PACKAGE_NAME 	= "extra_package";


// OEM Partiton
const char *OEM_DATA_ROOT = "/oem_data";


// defined QucikBoot pakcage
const char *QUICK_PACKAGE_NAME = "quick_package";


const char *UPDATE_ROOT = "/upgrade";
const char *STORAGE_LOG_ROOT = "/log";

#if 0	//for DEBUG
static const char *TEMPORARY_LOG_FILE = "/dev/kmsg";
#else
static const char *TEMPORARY_LOG_FILE = "/tmp/recovery.log";
#endif
static const char *TEMPORARY_INSTALL_FILE = "/tmp/last_install";
static const char *SIDELOAD_TEMP_DIR = "/tmp/sideload";
char LAST_LOG_FILE_TO_STORAGE[PATH_MAX];


const char *TEMP1_ROOT = "/upgrade/temp1";
const char *USB_BACKUP = "/usb/backup_img";

static const char *GPS_FILE = "/upgrade/gps/gps_module.bin";
static const char *DAB_FILE = "/upgrade/micom/dab_module.bin";

static const char *LGIT_MODEM_FILE = "/upgrade/modem/partition.mbn";
static const char *LGIT_MODEM_DRIVER = "/dev/ttyUSB0";



static const char *UPDATE_COUNT_GPS 	= "/upgrade/countgps.txt";
static const char *UPDATE_COUNT_MODEM 	= "/upgrade/countmodem.txt";
static const char *UPDATE_COUNT_TOUCH 	= "/upgrade/counttouch.txt";
static const char *UPDATE_COUNT_HD 		= "/upgrade/counthd.txt";
static const char *UPDATE_COUNT_SYSTEM 	= "/upgrade/countsystem.txt";
static const char *UPDATE_COUNT_MICOM 	= "/upgrade/countmicom.txt";
static const char *UPDATE_COUNT_DAB 	= "/upgrade/countdab.txt";


static const char *SYSTEM_UPDATE_LK_PATH           = "/upgrade/system/lk.rom";
static const char *SYSTEM_UPDATE_BOOT_PATH         = "/upgrade/system/boot.img";
static const char *SYSTEM_UPDATE_DTB_PATH          = "/upgrade/system/device_tree.dtb";
static const char *SYSTEM_UPDATE_SPLASH_PATH       = "/upgrade/system/splash.img";
static const char *SYSTEM_UPDATE_RECOVERY_PATH     = "/upgrade/system/recovery.img";
static const char *SYSTEM_UPDATE_SYSTEM_PATH       = "/upgrade/system/system.ext4";

static char *ENC_SYSTEM_UPDATE_LK_PATH          = "/upgrade/enc_system/enc_lk.rom";
static char *ENC_SYSTEM_UPDATE_BOOT_PATH        = "/upgrade/enc_system/enc_boot.img";
static char *ENC_SYSTEM_UPDATE_DTB_PATH         = "/upgrade/enc_system/enc_device_tree.dtb";
static char *ENC_SYSTEM_UPDATE_SPLASH_PATH      = "/upgrade/enc_system/enc_splash.img";
static char *ENC_SYSTEM_UPDATE_RECOVERY_PATH    = "/upgrade/enc_system/enc_recovery.img";
static char *ENC_SYSTEM_UPDATE_SYSTEM_PATH      = "/upgrade/enc_system/enc_system.ext4";

static const char *SYSTEM_UPDATE_WORKING            = "/upgrade/system_updating.cfg";
static const char *SYSTEM_PACKAGE_INFO_PATH         = "/upgrade/update.info";
static const char *SYSTEM_PACKAGE_SEC_INFO_PATH     = "/upgrade/update_info";

static const char *FOREGROUND_UPDATE_FLAG 			= "/upgrade/forground_update.cfg";
static const char *SYSTEM_UPDATE_VR2_PATH       	= "/upgrade/vr/VR2_PACKAGE.tar";

RecoveryUI* ui = NULL;

char locale[3] = {0, };
char recovery_version[PROPERTY_VALUE_MAX+1];
char save_qbdata_path_usb[4096] ;
char save_qbdata_path_hu [4096] ;

bool QUICKBOOT_SNAPSHOT = false;

#if defined(LGIT9x28_MODEM_SUPPORT)
#define MODEM_UPDATE_FIRMWARE_NUM 	10
static const char *modem_firmware_list[MODEM_UPDATE_FIRMWARE_NUM] = {
																	"appsboot.mbn",
																	"ENPRG9x07.mbn",
																	"NON-HLOS.ubi",
																	"NPRG9x07.mbn",
																	"partition.mbn",
																	"rpm.mbn",
																	"sbl1.mbn",
																	"mdm9607-boot.img",
																	"mdm9607-sysfs.ubi",
																	"tz.mbn" };
#endif


typedef struct build_product2_info{
  char car_type[8];
  char region[8];
  char hw_ver[8];
  char sw_ver[8];
  char build_time[16];
  int point_cnt;
}PRODUCT2_INFO;

char extra_package_name_buf[64] = {0,};
char tar_package_name[64] = {0,};


//--- compare the NAVI Map Version ------------------------------------------
#define NAVI_VERSION_STRING_MAX 	32
#define NAVI_MAP_VERSION_INFO 		"/upgrade/map_version.cfg"

//--- NAVI force update defined ---------------------------------------------
#define NAVI_MAP_INFOFILE_SIZE	2048
#define STR_MAX_LENGTH    		128
#define NAVI_TAR_MAX 			16

static const char *NAVI_LIST_ENC_PATH = "/usb/TarList.txt_encrypted";
static const char *NAVI_LIST_DEC_PATH = "/upgrade/TarList.txt";

typedef struct map_lists_info_struct {
	char navi_tar_str[NAVI_TAR_MAX][STR_MAX_LENGTH];
	char navi_version[NAVI_VERSION_STRING_MAX];
	int tar_cnt;
	unsigned long long TotalFileSize;
} MAP_LISTS_STRUCT;

//--- Qucikboot update defined ---------------------------------------------
#define QUCIKBOOT_IMAGE_PATH	"/upgrade/snapshot.sparse.img"
#define QUCIKBOOT_QBDATA_PATH	"/upgrade/qb_data.sparse.img"


//--------------------------------------------------------------------------------------------


extern int wait_child_ps(pid_t waiting_pid);
int request_to_enter_sleep_mode(void);

/*
 * The recovery tool communicates with the main system through /cache files.
 *   /cache/recovery/command - INPUT - command line for tool, one arg per line
 *   /cache/recovery/log - OUTPUT - combined log file from recovery run(s)
 *   /cache/recovery/intent - OUTPUT - intent that was passed in
 *
 * The arguments which may be supplied in the recovery.command file:
 *   --send_intent=anystring - write the text out to recovery.intent
 *   --update_package=path - verify install an OTA package file
 *   --wipe_data - erase user data (and cache), then reboot
 *   --wipe_cache - wipe cache (but not user data), then reboot
 *   --set_encrypted_filesystem=on|off - enables / diasables encrypted fs
 *   --just_exit - do nothing; exit and reboot
 *
 * After completing, we remove /cache/recovery/command and reboot.
 * Arguments may also be supplied in the bootloader control block (BCB).
 * These important scenarios must be safely restartable at any point:
 *
 * FACTORY RESET
 * 1. user selects "factory reset"
 * 2. main system writes "--wipe_data" to /cache/recovery/command
 * 3. main system reboots into recovery
 * 4. get_args() writes BCB with "boot-recovery" and "--wipe_data"
 *    -- after this, rebooting will restart the erase --
 * 5. erase_volume() reformats /data
 * 6. erase_volume() reformats /cache
 * 7. finish_recovery() erases BCB
 *    -- after this, rebooting will restart the main system --
 * 8. main() calls reboot() to boot main system
 *
 * OTA INSTALL
 * 1. main system downloads OTA package to /cache/some-filename.zip
 * 2. main system writes "--update_package=/cache/some-filename.zip"
 * 3. main system reboots into recovery
 * 4. get_args() writes BCB with "boot-recovery" and "--update_package=..."
 *    -- after this, rebooting will attempt to reinstall the update --
 * 5. install_package() attempts to install the update
 *    NOTE: the package install must itself be restartable from any point
 * 6. finish_recovery() erases BCB
 *    -- after this, rebooting will (try to) restart the main system --
 * 7. ** if install failed **
 *    7a. prompt_and_wait() shows an error icon and waits for the user
 *    7b; the user reboots (pulling the battery, etc) into the main system
 * 8. main() calls maybe_install_firmware_update()
 *    ** if the update contained radio/hboot firmware **:
 *    8a. m_i_f_u() writes BCB with "boot-recovery" and "--wipe_cache"
 *        -- after this, rebooting will reformat cache & restart main system --
 *    8b. m_i_f_u() writes firmware image into raw cache partition
 *    8c. m_i_f_u() writes BCB with "update-radio/hboot" and "--wipe_cache"
 *        -- after this, rebooting will attempt to reinstall firmware --
 *    8d. bootloader tries to flash firmware
 *    8e. bootloader writes BCB with "boot-recovery" (keeping "--wipe_cache")
 *        -- after this, rebooting will reformat cache & restart main system --
 *    8f. erase_volume() reformats /cache
 *    8g. finish_recovery() erases BCB
 *        -- after this, rebooting will (try to) restart the main system --
 * 9. main() calls reboot() to boot main system
 */

static const int MAX_ARG_LENGTH = 4096;
static const int MAX_ARGS = 100;

// open a given path, mounting partitions as necessary
FILE*
fopen_path(const char *path, const char *mode) {
    if (ensure_path_mounted(path) != 0) {
        LOGE("Can't mount %s\n", path);
        return NULL;
    }

    // When writing, try to create the containing directory, if necessary.
    // Use generous permissions, the system (init.rc) will reset them.
    if (strchr("wa", mode[0])) dirCreateHierarchy(path, 0777, NULL, 1, sehandle);

    FILE *fp = fopen(path, mode);
    return fp;
}

// close a file, log an error if the error indicator is set
static void
check_and_fclose(FILE *fp, const char *name) {
    fflush(fp);
    if (ferror(fp)) LOGE("Error in %s\n(%s)\n", name, strerror(errno));
    fclose(fp);
}

// command line args come from, in decreasing precedence:
//   - the actual command line
//   - the bootloader control block (one per line, after "recovery")
//   - the contents of COMMAND_FILE (one per line)
static void
get_args(int *argc, char ***argv) {
    struct bootloader_message boot;
    memset(&boot, 0, sizeof(boot));
    get_bootloader_message(&boot);  // this may fail, leaving a zeroed structure

    if (boot.command[0] != 0 && boot.command[0] != 255) {
        LOGI("Boot command: %.*s\n", sizeof(boot.command), boot.command);
    }

    if (boot.status[0] != 0 && boot.status[0] != 255) {
        LOGI("Boot status: %.*s\n", sizeof(boot.status), boot.status);
    }

    // --- if arguments weren't supplied, look in the bootloader control block
    if (*argc <= 1) {
        boot.recovery[sizeof(boot.recovery) - 1] = '\0';  // Ensure termination
        const char *arg = strtok(boot.recovery, "\n");
        if (arg != NULL && !strcmp(arg, "recovery")) {
            *argv = (char **) malloc(sizeof(char *) * MAX_ARGS);
            (*argv)[0] = strdup(arg);
            for (*argc = 1; *argc < MAX_ARGS; ++*argc) {
                if ((arg = strtok(NULL, "\n")) == NULL) break;
                (*argv)[*argc] = strdup(arg);
            }
            LOGI("Got arguments from boot message\n");
        } else if (boot.recovery[0] != 0 && boot.recovery[0] != 255) {
            LOGE("Bad boot message\n\"%.20s\"\n", boot.recovery);
        }
    }

    // --- if that doesn't work, try the command file
    if (*argc <= 1) {
        FILE *fp = fopen_path(COMMAND_FILE, "r");
        if (fp != NULL) {
            char *token;
            char *argv0 = (*argv)[0];
            *argv = (char **) malloc(sizeof(char *) * MAX_ARGS);
            (*argv)[0] = argv0;  // use the same program name

            char buf[MAX_ARG_LENGTH];
            for (*argc = 1; *argc < MAX_ARGS; ++*argc) {
                if (!fgets(buf, sizeof(buf), fp)) break;
                token = strtok(buf, "\r\n");
                if (token != NULL) {
                    (*argv)[*argc] = strdup(token);  // Strip newline.
                } else {
                    --*argc;
                }
            }

            check_and_fclose(fp, COMMAND_FILE);
            LOGI("Got arguments from %s\n", COMMAND_FILE);
        }
    }

    // --> write the arguments we have back into the bootloader control block
    // always boot into recovery after this (until finish_recovery() is called)
    strlcpy(boot.command, "boot-recovery", sizeof(boot.command));
    strlcpy(boot.recovery, "recovery\n", sizeof(boot.recovery));
    int i;
    for (i = 1; i < *argc; ++i) {
        strlcat(boot.recovery, (*argv)[i], sizeof(boot.recovery));
        strlcat(boot.recovery, "\n", sizeof(boot.recovery));
    }
    set_bootloader_message(&boot);
}

static void
set_sdcard_update_bootloader_message() {
    struct bootloader_message boot;
    memset(&boot, 0, sizeof(boot));
    strlcpy(boot.command, "boot-recovery", sizeof(boot.command));
    strlcpy(boot.recovery, "recovery\n", sizeof(boot.recovery));
    set_bootloader_message(&boot);
}

// How much of the temp log we have copied to the copy in cache.
static long tmplog_offset = 0;

static void
copy_log_file(const char* source, const char* destination, int append) {
    FILE *log = fopen_path(destination, append ? "a" : "w");
    if(log == NULL) {
        LOGE("Can't open %s\n", destination);
    } else {
        FILE *tmplog = fopen(source, "r");
        if (tmplog != NULL) {
            if(append) {
                fseek(tmplog, tmplog_offset, SEEK_SET);  // Since last write
            }

            char buf[4096];
            while(fgets(buf, sizeof(buf), tmplog)) fputs(buf, log);
            if(append) {
                tmplog_offset = ftell(tmplog);
            }
            check_and_fclose(tmplog, source);
        }
        check_and_fclose(log, destination);
    }
}

// Rename last_log -> last_log.1 -> last_log.2 -> ... -> last_log.$max
// Overwrites any existing last_log.$max.
static void
rotate_last_logs(int max) {
    char oldfn[256];
    char newfn[256];

    int i;
    for (i = max-1; i >= 0; --i) {
        snprintf(oldfn, sizeof(oldfn), (i==0) ? LAST_LOG_FILE : (LAST_LOG_FILE ".%d"), i);
        snprintf(newfn, sizeof(newfn), LAST_LOG_FILE ".%d", i+1);
        // ignore errors
        rename(oldfn, newfn);
    }
}

static void
copy_logs() {
    // Copy logs to cache so the system can find out what happened.
    copy_log_file(TEMPORARY_LOG_FILE, LOG_FILE, true);
    copy_log_file(TEMPORARY_LOG_FILE, LAST_LOG_FILE, false);
    copy_log_file(TEMPORARY_INSTALL_FILE, LAST_INSTALL_FILE, false);
    chmod(LOG_FILE, 0600);
    chown(LOG_FILE, 1000, 1000);   // system user
    chmod(LAST_LOG_FILE, 0640);
    chmod(LAST_INSTALL_FILE, 0644);

    copy_log_file(TEMPORARY_LOG_FILE, LAST_LOG_FILE_TO_STORAGE, false);
    chmod(LAST_LOG_FILE_TO_STORAGE, 0640);

    sync();
}

// clear the recovery command and prepare to boot a (hopefully working) system,
// copy our log file to cache as well (for the system to read), and
// record any intent we were asked to communicate back to the system.
// this function is idempotent: call it as many times as you like.
static void
finish_recovery(const char *send_intent, const char *update_package) {
    // By this point, we're ready to return to the main system...
    if (send_intent != NULL) {
        FILE *fp = fopen_path(INTENT_FILE, "w");
        if (fp == NULL) {
            LOGE("Can't open %s\n", INTENT_FILE);
        } else {
            fputs(send_intent, fp);
            check_and_fclose(fp, INTENT_FILE);
        }
    }

    copy_logs();

    // Reset to normal system boot so recovery won't cycle indefinitely.
    struct bootloader_message boot;
    memset(&boot, 0, sizeof(boot));
    if (update_package != NULL) {
	    sprintf(boot.command, "%s", "boot-force_normal");
    }
    set_bootloader_message(&boot);

    // Remove the command file, so recovery won't repeat indefinitely.
    if (ensure_path_mounted(COMMAND_FILE) != 0 ||
        (unlink(COMMAND_FILE) && errno != ENOENT)) {
        LOGW("Can't unlink %s\n", COMMAND_FILE);
    }

    ensure_path_unmounted(CACHE_ROOT);
    sync();  // For good measure.
}

typedef struct _saved_log_file {
    char* name;
    struct stat st;
    unsigned char* data;
    struct _saved_log_file* next;
} saved_log_file;

static int
erase_volume(const char *volume) {
    //bool is_cache = (strcmp(volume, CACHE_ROOT) == 0);
    bool is_cache = false;  //TODO : cache recovery log restore
    saved_log_file* head = NULL;

    if (is_cache) {
        // If we're reformatting /cache, we load any
        // "/cache/recovery/last*" files into memory, so we can restore
        // them after the reformat.

        ensure_path_mounted(volume);

        DIR* d;
        struct dirent* de;
        d = opendir(CACHE_LOG_DIR);
        if (d) {
            char path[PATH_MAX];
            strcpy(path, CACHE_LOG_DIR);
            strcat(path, "/");
            int path_len = strlen(path);
            while ((de = readdir(d)) != NULL) {
                if (strncmp(de->d_name, "last", 4) == 0) {
                    saved_log_file* p = (saved_log_file*) malloc(sizeof(saved_log_file));
                    strcpy(path+path_len, de->d_name);
                    p->name = strdup(path);
                    if (stat(path, &(p->st)) == 0) {
                        // truncate files to 512kb
                        if (p->st.st_size > (1 << 19)) {
                            p->st.st_size = 1 << 19;
                        }
                        p->data = (unsigned char*) malloc(p->st.st_size);
                        FILE* f = fopen(path, "rb");
                        fread(p->data, 1, p->st.st_size, f);
                        fclose(f);
                        p->next = head;
                        head = p;
                    } else {
                        free(p);
                    }
                }
            }
            closedir(d);
        } else {
            if (errno != ENOENT) {
                printf("opendir failed: %s\n", strerror(errno));
            }
        }
    }
    ui->Print("Formatting %s...\n", volume);

    ensure_path_unmounted(volume);
    int result = format_volume(volume);

    if (is_cache) {
        while (head) {
            FILE* f = fopen_path(head->name, "wb");
            if (f) {
                fwrite(head->data, 1, head->st.st_size, f);
                fclose(f);
                chmod(head->name, head->st.st_mode);
                chown(head->name, head->st.st_uid, head->st.st_gid);
            }
            free(head->name);
            free(head->data);
            saved_log_file* temp = head->next;
            free(head);
            head = temp;
        }

        // Any part of the log we'd copied to cache is now gone.
        // Reset the pointer so we copy from the beginning of the temp
        // log.
        tmplog_offset = 0;
        copy_logs();
    }

    return result;
}

static char*
copy_sideloaded_package(const char* original_path) {
  if (ensure_path_mounted(original_path) != 0) {
    LOGE("Can't mount %s\n", original_path);
    return NULL;
  }

  if (ensure_path_mounted(SIDELOAD_TEMP_DIR) != 0) {
    LOGE("Can't mount %s\n", SIDELOAD_TEMP_DIR);
    return NULL;
  }

  if (mkdir(SIDELOAD_TEMP_DIR, 0700) != 0) {
    if (errno != EEXIST) {
      LOGE("Can't mkdir %s (%s)\n", SIDELOAD_TEMP_DIR, strerror(errno));
      return NULL;
    }
  }

  // verify that SIDELOAD_TEMP_DIR is exactly what we expect: a
  // directory, owned by root, readable and writable only by root.
  struct stat st;
  if (stat(SIDELOAD_TEMP_DIR, &st) != 0) {
    LOGE("failed to stat %s (%s)\n", SIDELOAD_TEMP_DIR, strerror(errno));
    return NULL;
  }
  if (!S_ISDIR(st.st_mode)) {
    LOGE("%s isn't a directory\n", SIDELOAD_TEMP_DIR);
    return NULL;
  }
  if ((st.st_mode & 0777) != 0700) {
    LOGE("%s has perms %o\n", SIDELOAD_TEMP_DIR, st.st_mode);
    return NULL;
  }
  if (st.st_uid != 0) {
    LOGE("%s owned by %lu; not root\n", SIDELOAD_TEMP_DIR, st.st_uid);
    return NULL;
  }

  char copy_path[PATH_MAX];
  strcpy(copy_path, SIDELOAD_TEMP_DIR);
  strcat(copy_path, "/package.zip");

  char* buffer = (char*)malloc(BUFSIZ);
  if (buffer == NULL) {
    LOGE("Failed to allocate buffer\n");
    return NULL;
  }

  size_t read;
  FILE* fin = fopen(original_path, "rb");
  if (fin == NULL) {
    LOGE("Failed to open %s (%s)\n", original_path, strerror(errno));
    return NULL;
  }
  FILE* fout = fopen(copy_path, "wb");
  if (fout == NULL) {
    LOGE("Failed to open %s (%s)\n", copy_path, strerror(errno));
    return NULL;
  }

  while ((read = fread(buffer, 1, BUFSIZ, fin)) > 0) {
    if (fwrite(buffer, 1, read, fout) != read) {
      LOGE("Short write of %s (%s)\n", copy_path, strerror(errno));
      return NULL;
    }
  }

  free(buffer);

  if (fclose(fout) != 0) {
    LOGE("Failed to close %s (%s)\n", copy_path, strerror(errno));
    return NULL;
  }

  if (fclose(fin) != 0) {
    LOGE("Failed to close %s (%s)\n", original_path, strerror(errno));
    return NULL;
  }

  // "adb push" is happy to overwrite read-only files when it's
  // running as root, but we'll try anyway.
  if (chmod(copy_path, 0400) != 0) {
    LOGE("Failed to chmod %s (%s)\n", copy_path, strerror(errno));
    return NULL;
  }

  return strdup(copy_path);
}

const char**
prepend_title(const char* const* headers) {
    // count the number of lines in our title, plus the
    // caller-provided headers.
    int count = 3;   // our title has 3 lines
    const char* const* p;
    for (p = headers; *p; ++p, ++count);

    const char** new_headers = (const char**)malloc((count+1) * sizeof(char*));
    const char** h = new_headers;
    *(h++) = "Android system recovery <" EXPAND(RECOVERY_API_VERSION) "e>";
    *(h++) = recovery_version;
    *(h++) = "";
    for (p = headers; *p; ++p, ++h) *h = *p;
    *h = NULL;

    return new_headers;
}

int
get_menu_selection(const char* const * headers, const char* const * items,
                   int menu_only, int initial_selection, Device* device) {
    // throw away keys pressed previously, so user doesn't
    // accidentally trigger menu items.
    ui->FlushKeys();

    // Count items to detect valid values for absolute selection
    int item_count = 0;
    while (items[item_count] != NULL)
        ++item_count;

    ui->StartMenu(headers, items, initial_selection);
    int selected = initial_selection;
    int chosen_item = -1;

    while (chosen_item < 0) {
        int key = ui->WaitKey();
        int visible = ui->IsTextVisible();

        if (key == -1) {   // ui_wait_key() timed out
#if 0
            if (ui->WasTextEverVisible()) {
                continue;
            } else {
                LOGI("timed out waiting for key input; rebooting.\n");
                ui->EndMenu();
                return 0; // XXX fixme
            }
#endif
        }

        int action = device->HandleMenuKey(key, visible);

        if (action >= 0) {
            if ((action & ~KEY_FLAG_ABS) >= item_count) {
                action = Device::kNoAction;
            }
            else {
                // Absolute selection.  Update selected item and give some
                // feedback in the UI by selecting the item for a short time.
                selected = action & ~KEY_FLAG_ABS;
                action = Device::kInvokeItem;
                selected = ui->SelectMenu(selected, true);
                usleep(50*1000);
            }
        }

        if (action < 0) {
            switch (action) {
                case Device::kHighlightUp:
                    --selected;
                    selected = ui->SelectMenu(selected);
                    break;
                case Device::kHighlightDown:
                    ++selected;
                    selected = ui->SelectMenu(selected);
                    break;
                case Device::kInvokeItem:
                    chosen_item = selected;
                    break;
                case Device::kNoAction:
                    break;
            }
        } else if (!menu_only) {
            chosen_item = action;
        }
    }

    ui->EndMenu();
    return chosen_item;
}

static int compare_string(const void* a, const void* b) {
    return strcmp(*(const char**)a, *(const char**)b);
}

static int
update_directory(const char* path, const char* unmount_when_done,
                 int* wipe_cache, Device* device) {
    ensure_path_mounted(path);

    const char* MENU_HEADERS[] = { "Choose a package to install:",
                                   path,
                                   "",
                                   NULL };
    DIR* d;
    struct dirent* de;
    d = opendir(path);
    if (d == NULL) {
        LOGE("error opening %s: %s\n", path, strerror(errno));
        if (unmount_when_done != NULL) {
            ensure_path_unmounted(unmount_when_done);
        }
        return 0;
    }

    const char** headers = prepend_title(MENU_HEADERS);

    int d_size = 0;
    int d_alloc = 10;
    char** dirs = (char**)malloc(d_alloc * sizeof(char*));
    int z_size = 1;
    int z_alloc = 10;
    char** zips = (char**)malloc(z_alloc * sizeof(char*));
    zips[0] = strdup("../");

    while ((de = readdir(d)) != NULL) {
        int name_len = strlen(de->d_name);

        if (de->d_type == DT_DIR) {
            // skip "." and ".." entries
            if (name_len == 1 && de->d_name[0] == '.') continue;
            if (name_len == 2 && de->d_name[0] == '.' &&
                de->d_name[1] == '.') continue;

            if (d_size >= d_alloc) {
                d_alloc *= 2;
                dirs = (char**)realloc(dirs, d_alloc * sizeof(char*));
            }
            dirs[d_size] = (char*)malloc(name_len + 2);
            strcpy(dirs[d_size], de->d_name);
            dirs[d_size][name_len] = '/';
            dirs[d_size][name_len+1] = '\0';
            ++d_size;
        } else if (de->d_type == DT_REG &&
                   name_len >= 4 &&
                   strncasecmp(de->d_name + (name_len-4), ".zip", 4) == 0) {
            if (z_size >= z_alloc) {
                z_alloc *= 2;
                zips = (char**)realloc(zips, z_alloc * sizeof(char*));
            }
            zips[z_size++] = strdup(de->d_name);
        }
    }
    closedir(d);

    qsort(dirs, d_size, sizeof(char*), compare_string);
    qsort(zips, z_size, sizeof(char*), compare_string);

    // append dirs to the zips list
    if (d_size + z_size + 1 > z_alloc) {
        z_alloc = d_size + z_size + 1;
        zips = (char**)realloc(zips, z_alloc * sizeof(char*));
    }
    memcpy(zips + z_size, dirs, d_size * sizeof(char*));
    free(dirs);
    z_size += d_size;
    zips[z_size] = NULL;

    int result;
    int chosen_item = 0;
    do {
        chosen_item = get_menu_selection(headers, zips, 1, chosen_item, device);

        char* item = zips[chosen_item];
        int item_len = strlen(item);
        if (chosen_item == 0) {          // item 0 is always "../"
            // go up but continue browsing (if the caller is update_directory)
            result = -1;
            break;
        } else if (item[item_len-1] == '/') {
            // recurse down into a subdirectory
            char new_path[PATH_MAX];
            strlcpy(new_path, path, PATH_MAX);
            strlcat(new_path, "/", PATH_MAX);
            strlcat(new_path, item, PATH_MAX);
            new_path[strlen(new_path)-1] = '\0';  // truncate the trailing '/'
            result = update_directory(new_path, unmount_when_done, wipe_cache, device);
            if (result >= 0) break;
        } else {
            // selected a zip file:  attempt to install it, and return
            // the status to the caller.
            char new_path[PATH_MAX];
            strlcpy(new_path, path, PATH_MAX);
            strlcat(new_path, "/", PATH_MAX);
            strlcat(new_path, item, PATH_MAX);

            ui->Print("\n-- Install %s ...\n", path);
            set_sdcard_update_bootloader_message();
            char* copy = copy_sideloaded_package(new_path);
            if (unmount_when_done != NULL) {
                ensure_path_unmounted(unmount_when_done);
            }
            if (copy) {
                result = install_package(copy, wipe_cache, TEMPORARY_INSTALL_FILE);
                free(copy);
            } else {
                result = INSTALL_ERROR;
            }
            break;
        }
    } while (true);

    int i;
    for (i = 0; i < z_size; ++i) free(zips[i]);
    free(zips);
    free(headers);

    if (unmount_when_done != NULL) {
        ensure_path_unmounted(unmount_when_done);
    }
    return result;
}

static void
wipe_data(int confirm, Device* device) {
    if (confirm) {
        static const char** title_headers = NULL;

        if (title_headers == NULL) {
            const char* headers[] = { "Confirm wipe of all user data?",
                                      "  THIS CAN NOT BE UNDONE.",
                                      "",
                                      NULL };
            title_headers = prepend_title((const char**)headers);
        }

        const char* items[] = { " No",
                                " No",
                                " No",
                                " No",
                                " No",
                                " No",
                                " No",
                                " Yes -- delete all user data",   // [7]
                                " No",
                                " No",
                                " No",
                                NULL };

        int chosen_item = get_menu_selection(title_headers, items, 1, 0, device);
        if (chosen_item != 7) {
            return;
        }
    }

    ui->Print("\n-- Wiping data...\n");
    device->WipeData();
    erase_volume(USERDATA_ROOT);
    erase_volume(CACHE_ROOT);
    if (QUICKBOOT_SNAPSHOT)
      erase_volume(SNAPSHOT_ROOT);
    ui->Print("Data wipe complete.\n");
}

static void
print_property(const char *key, const char *name, void *cookie) {
    printf("%s=%s\n", key, name);
}

#if 1 	// TODO : need to modify
static void
load_locale_from_cache() {
	LOGI("load_locale_from_cache\n");
    FILE* fp = fopen_path(LOCALE_FILE, "r");
    char buffer[80];
    if (fp != NULL) {
        fgets(buffer, sizeof(buffer), fp);
        int j = 0;
        unsigned int i;
        for (i = 0; i < sizeof(buffer) && buffer[i]; ++i) {
            if (!isspace(buffer[i])) {
                buffer[j++] = buffer[i];
            }
        }
        buffer[j] = 0;
		memset(locale,0x00,sizeof(char)*3);
		memcpy(locale, buffer, 2);
        check_and_fclose(fp, LOCALE_FILE);
    }else{
		memset(locale,0x00,sizeof(char)*3);
		memcpy(locale, "en", 2);
	}
}
#endif

static RecoveryUI* gCurrentUI = NULL;

void
ui_print(const char* format, ...) {
    char buffer[256];

    va_list ap;
    va_start(ap, format);
    vsnprintf(buffer, sizeof(buffer), format, ap);
    va_end(ap);

    if (gCurrentUI != NULL) {
        gCurrentUI->Print("%s", buffer);
    } else {
        fputs(buffer, stdout);
    }
}

/**
 * Description : Update counting function
 * @param no : file path
 * @return : None
 * @see    : None
 */
int UpdateCountFunc(const char *filepath)
{
    FILE *fp;
    int readCnt, writeCnt;
    char buf[4] = {0,};

    printf("File is [%s]\n", filepath);
    fp = fopen(filepath, "r+");
    if(fp == NULL) {
        fp = fopen(filepath, "w+");
    }

    if(fp != NULL) {
        readCnt = fread(buf, 1, 4, fp);
        printf("Count is [%d]\n", readCnt);
        if(readCnt >= 2) {
            fprintf(stderr, "Count over. Retry end\n");
            fclose(fp);
            return -1;
        }

        writeCnt = fwrite("1", 1, 1, fp);
        if(writeCnt < 0) {
            fprintf(stderr, "Count file write failed\n");
            fclose(fp);
            return -1;
        }
        fclose(fp);
    } else {
        printf("Count file open error\n");
        return -1;
    }

    sync();
    return 0;
}

void getBuildProduct(char *dest, const char *src, char mChar)
{
    unsigned int j = 0, i = 0, k = 0, max = PROPERTY_VALUE_MAX;

    for(i = 0;i < strlen(src);i++) {
        if(src[i] == mChar) {
            j++;
            k = 0;
        } else {
            if('A' <= src[i] && src[i] <= 'Z') {
                dest[j * max + k] = src[i] + PROPERTY_VALUE_MAX;
            } else {
                dest[j * max + k] = src[i];
            }
            k++;
        }
    }
}

void makeLogFiles()
{
    int addNumber = 1;
    char tmpText[PATH_MAX];

    memset(tmpText, 0, PATH_MAX);
    sprintf(tmpText, "/log/recovery/logRecovery");

    memset(LAST_LOG_FILE_TO_STORAGE, 0, sizeof(LAST_LOG_FILE_TO_STORAGE));
    sprintf(LAST_LOG_FILE_TO_STORAGE, "%s%d", tmpText, addNumber);

    do {
        if(access(LAST_LOG_FILE_TO_STORAGE, F_OK) == 0) {
            addNumber++;
            snprintf(LAST_LOG_FILE_TO_STORAGE, sizeof(LAST_LOG_FILE_TO_STORAGE) - 1, "%s%d", tmpText, addNumber);
            LAST_LOG_FILE_TO_STORAGE[sizeof(LAST_LOG_FILE_TO_STORAGE) - 1] = '\0';
        } else {
            break;
        }
    } while(1);
}

static void wipe_data_in_update(Device *device)
{
    LOGI("Wiping data start\n");
    device->WipeData();
    erase_volume("/data");
    erase_volume("/cache");
    LOGI("Wiping data completez\n");
}

static bool isFactoryResetCommand(int wipedata, int wipecache)
{
    if(wipedata == 1 && wipecache == 1) {
        return true;
    }

    return false;
}

static unsigned int
quickboot_FactoryReset() {
	unsigned int result = 0;

	LOGI("\n-- Quick boot img make...\n");

	if(0 > ensure_path_mounted(QB_INTERNAL_ROOT)) {
        //error_print("[ERROR] '/qb_data' partition mount failed\n");
		    //result += 0x20;
	}

	if(0 > quickboot_factory_reset_opt(QB_OPT_DATA | QB_OPT_CACHE)) {
		//error_print("[ERROR] quick boot image update failed\n");
		result += 0x80;
	}

	if(0 > ensure_path_unmounted(QB_INTERNAL_ROOT)) {
        //error_print("[ERROR] '/qb_data' partition unmount failed\n");
		    //result += 0x200;
	}

	LOGI("Quick boot img make compleate\n");

	return result;
}

static unsigned int
wipe_data_in_FactoryReset(Device* device) {
    unsigned int result = 0;
    LOGI("\n-- Wiping data...\n");
    device->WipeData();

    if(0 > erase_volume("/data")) {
        //error_print("[ERROR] '/data' partition erase failed\n");
        printf("CJUNG : /data erase failed\n");
        result += 0x01;
    }

    if(0 > erase_volume("/cache")) {
        //error_print("[ERROR] '/cache' partition erase failed\n");
        printf("CJUNG : /cache erase failed\n");
        result += 0x02;
    }

	if(0 > erase_volume("/log")) {
        //error_print("[ERROR] '/cache' partition erase failed\n");
        printf("CJUNG : /log erase failed\n");
        result += 0x02;
    }
	
#if 0 //+[TCCQB]
    if(0 > erase_volume("/mymusic")) {
        //error_print("[ERROR] '/mymusic' partition erase failed\n");
        printf("CJUNG : /mymusic erase failed\n");
        result += 0x04;
    }
#endif //-[TCCQB]
    LOGI("Data wipe complete. result [%ud]\n", result);

    return result;
}

void
start_Factory_Reset(void)
{
	int result = 0;
	unsigned int status = 0;
	struct bootloader_message boot_msg;
	Device* device = make_device();

	/* pre-mountd list --------------
	   /system,
	   /cache,
	 -------------------------------- */

	ui = device->GetUI();
	ui->SetLocale(locale);
	ui->Init();
	ui->countTotal = 0;
	ui->countPresent = 0;
	ui->ShowText(false);
	ui->ShowSystemIcon(false);
	ui->SetBackground(RecoveryUI::FACTORY_RESET);

	device->StartRecovery();

	printf("startFactoryReset sec....\n");

    result = ensure_path_unmounted(CACHE_ROOT);
	if (result == -1) {
		printf("/cache umount fail (%d)....\n",result);
		sleep(1);
	    ensure_path_unmounted(CACHE_ROOT);
	}

	status = wipe_data_in_FactoryReset(device);
	if (status != 0) {
		printf("wipe_data_in_FactoryReset is fail (%d)\n",result);

		if ((status & 0x01) != 0) {
			ensure_path_unmounted(USERDATA_ROOT);
		}
		if ((status & 0x02) != 0) {
			ensure_path_unmounted(CACHE_ROOT);
		}
#if 0 //+[TCCQB]
		if ((status & 0x04) != 0) {
			ensure_path_unmounted(MYMUSIC_ROOT);
		}
#endif //-[TCCQB]

		status = wipe_data_in_FactoryReset(device);
	}

	if (status == 0) {
		status = quickboot_FactoryReset();

		if(status)
		{
			ui->SetBackground(RecoveryUI::ERROR);
			ui->Print("QB data is written failed. reason[%08X]!!\n", result);
			LOGE("QB data is written failed. reason[%08X]!!\n", result);
			sleep(10);
			while (1) { }
		}
	}

	printf("QB data is written successfully!!\n");

	result = ensure_path_mounted(USERDATA_ROOT);
	sleep(2);
	if (result >= 0) {
		property_set("persist.sys.time.user.offset", "0");
	} else {
		printf("/data mount fail... error (%d) \r\n",result);
	}

	memset(&boot_msg,0x00,sizeof(struct bootloader_message));
	set_bootloader_message(&boot_msg);

	sync();
	sync();
	ensure_path_unmounted(USERDATA_ROOT);

#if 1 //TODO : check micom
	hardReset();
#else
	android_reboot(ANDROID_RB_RESTART2, 0, (char *)"force_normal");
#endif
	sleep(5);
	while (1) { }
}

static int __check_cmp(const char *filename)
{
    int ret = 0;
    char src1[4096] = {0, };
    char src2[4096] = {0, };

    sprintf(src1, "%s/fwdn/%s", save_qbdata_path_usb, filename);
    sprintf(src2, "%s/fwdn/%s", save_qbdata_path_hu, filename);

    ret = execl("/system/bin/cmp","cmp", src1, src2, NULL);
    if(ret != 0) {
        printf("[%s] : cmp error [%s]\n", __func__, src1);
    } else {
        printf("[%s] : cmp success [%s]\n", __func__, src1);
    }

    return ret;
}

static int __check_cmp_path(const char *filename, const char *src, const char *dest)
{
    int ret = 0;
    char src1[4096] = {0, };
    char src2[4096] = {0, };

    sprintf(src1, "%s/fwdn/%s", src, filename);
    sprintf(src2, "%s/%s", dest, filename);

    ret = execl("/system/bin/cmp", "cmp", src1, src2, NULL);
    if(ret != 0) {
        printf("[%s] : cmp error [%s]\n", __func__, src1);
    } else {
        printf("[%s] : cmp success [%s]\n", __func__, src1);
    }

    return ret;
}

int tcc_cp(char *source, const char *dest)
{
    int childExitStatus;
    pid_t pid;
    int status;

    LOGI("[%s] : file copy [%s] -> [%s]\n", __func__, source, dest);

    if(source == NULL || dest == NULL) {
        LOGE("[%s] : check file src[%s] & dst[%s]\n", __func__, source, dest);
        return -1;
    }

    pid = fork();

    if(pid == 0) {
        execl("/system/bin/cp", "/system/bin/cp", source, dest, (char *)0);
    }

    status = wait_child_ps(pid);
    if(status < 0) {
        LOGE("[%s] : file copy failed [%d]\n", __func__, status);
        return -1;
    }

    return 0;
}

void setPermissionBackupDir(char *wd)
{
    struct dirent *dentry;
    struct stat fstat;
    DIR *dirp;
    int indent = 0;
    int i;

    if(chdir(wd) < 0) {
        printf("error: chdir..\n");
        return ;
    }

    if((dirp = opendir(".")) == NULL) {
        printf("error: opendir..\n");
        return;
    }

    while((dentry = readdir(dirp))!=NULL) {
        if(dentry->d_ino != 0) {
            if((!strcmp(dentry->d_name, ".")) || (!strcmp(dentry->d_name, ".."))) {
                continue;
            }
            stat(dentry->d_name, &fstat);

            for(i = 0; i < indent; i++) {
                printf("\t");
            }

            if(S_ISDIR(fstat.st_mode)) {
                printf("%s\n", dentry->d_name);
                chmod(dentry->d_name, 0777);
                indent++;
                setPermissionBackupDir(dentry->d_name);
            } else {
                printf("\r");
                chmod(dentry->d_name, 0777);
            }
        }
    }

    closedir(dirp);
    indent--;
    chdir("..");
}

#if 1 //jason.ku 2017.11.01
static int __make_extra_package_tar(const char *tar_file_name)
{
    int ret = 0;
    char src1[4096] = {0, };

    sprintf(src1, "%s/%s.tar", USB_ROOT,tar_file_name);

	char tmp[256];
	sprintf(tmp, "busybox tar -cf %s snapshot.sparse.img qb_data.sparse.img", src1);
	ret = system(tmp);
    return ret;
}

PRODUCT2_INFO get_build_product2_str(char *buf)
{
	char *VerStr;
	int str_length = 0;
	PRODUCT2_INFO get_ver_info;

	memset(&get_ver_info, 0x00, sizeof(get_ver_info));
	get_ver_info.point_cnt = 0;
	VerStr = strtok(buf, ".");
	
	while(VerStr != NULL) {
		str_length = strlen(VerStr);
		if(get_ver_info.point_cnt == 0)
			strncpy(get_ver_info.car_type, VerStr, str_length);
		else if(get_ver_info.point_cnt == 1)
			strncpy(get_ver_info.region, VerStr, str_length);
		else if(get_ver_info.point_cnt == 2)
			strncpy(get_ver_info.hw_ver, VerStr, str_length);
		else if(get_ver_info.point_cnt == 3)
			strncpy(get_ver_info.sw_ver, VerStr, str_length);
		else if(get_ver_info.point_cnt == 4)
			strncpy(get_ver_info.build_time, VerStr, str_length);
		else 
			ui->Print(" Not Used \n");
		get_ver_info.point_cnt++;
		VerStr = strtok(NULL, ".");
	}
	return get_ver_info;
}

void delete_copied_file(void)
{
	char tmp[256];
	sprintf(tmp, "rm -rf qb_data.sparse.img snapshot.sparse.img");
	system(tmp);
}

void make_extra_package_func(void)
{
	PRODUCT2_INFO product2_info;

	char build_release_buf[PROPERTY_VALUE_MAX] = {0,};
	char build_product2_buf[PROPERTY_VALUE_MAX] = {0,};

    int pid = 0;
    int status = 0;
    int result = 0;

    memset(build_release_buf, 0, PROPERTY_VALUE_MAX);
	property_get("ro.build.date.releaze", build_release_buf, "");

    memset(build_product2_buf, 0, PROPERTY_VALUE_MAX);
    property_get("ro.build.product2", build_product2_buf, "");

	memset(&product2_info, 0x00, sizeof(product2_info));
	product2_info = get_build_product2_str(build_product2_buf);

    sprintf(extra_package_name_buf, "%s%s.%s.%s.%s.%s","quick_package_", product2_info.car_type, product2_info.region, \
		    product2_info.hw_ver, product2_info.sw_ver, build_release_buf);

	if(0 > tcc_cp("/usb/backup_img/snapshot.sparse.img", "/")) 
    {
		ui->Print("[%s] : [%s] copy failed!\n", __func__, "/usb/backup_img/snapshot.sparse.img");
	}
	
	if(0 > tcc_cp("/usb/backup_img/qb_data.sparse.img", "/")) 
    {
		ui->Print("[%s] : [%s] copy failed!\n", __func__, "/usb/backup_img/qb_data.sparse.img");
	}

	__make_extra_package_tar(extra_package_name_buf);
    sync();
	delete_copied_file();
}
#endif



void extract_Qbimg_Data(const char *extract_qbimg_path, int extract_qb_data_only)
{
    int pid=0;
    int result = 0;
    int status = 0;

    struct bootloader_message boot_msg;
    Device* device = make_device();

    /* pre-mountd list --------------
       /system,
       /cache,
     -------------------------------- */

    ui = device->GetUI();
    ui->SetLocale(locale);
    ui->Init();
    ui->SetBackground(RecoveryUI::INSTALLING_UPDATE);
    ui->countTotal = 0;
    ui->countPresent = 0;
    ui->ShowText(true);

    ui->ui_set_status_message(RecoveryUI::INSTALLING_MESSAGE);
    device->StartRecovery();

    ui->Print("extract_Qbimg_Data\n");

    result = ensure_path_mounted(QB_INTERNAL_ROOT);

    if(result == -1 && errno == 22) {
        erase_volume(QB_INTERNAL_ROOT);
    }

    if(result != 0) {
        sleep(1);
        ensure_path_mounted(QB_INTERNAL_ROOT);
    }

    result = ensure_path_unmounted(CACHE_ROOT);
    if(result == -1) {
        sleep(1);
        ensure_path_unmounted(CACHE_ROOT);
    }

    if(extract_qb_data_only == 0) {
        result = ensure_path_mounted(USB_ROOT);
        if(result != 0) {
            sleep(5);
            result = ensure_path_mounted(USB_ROOT);
        }
    }

    memset(save_qbdata_path_usb, 0, 4096);
    memset(save_qbdata_path_hu, 0, 4096);

    extract_qb_data_auto(extract_qbimg_path, extract_qb_data_only);

    if(extract_qb_data_only == 0) {
        ui->Print("save_qbdata_path_1 = [%s]\n", save_qbdata_path_usb);
        ui->Print("save_qbdata_path_2 = [%s]\n", save_qbdata_path_hu);

        /* compare 1*/
        pid = fork();
        if(0 == pid) {
            __check_cmp("qb_data.sparse.img");
        }
        status = wait_child_ps(pid);

        ui->Print("[%s] : qb_data.sparse child return (%d)\n", __func__, status);

        pid = fork();
        if(0 == pid) {
            __check_cmp("snapshot.sparse.img");
        }
        status = wait_child_ps(pid);

        /*HU data backup*/
        ui->Print("[%s] : snapshot.sparse child return (%d)\n", __func__, status);

        char tmp_path_hu[4096];
        char tmp_path_usb[4096];

        result = mkdir(USB_BACKUP, 0666);
        if(0 != result) {
            ui->Print("[%s] : mkdir failed!\n", __func__);
            remove("/usb/backup_img/qb_data.sparse.img");
            remove("/usb/backup_img/snapshot.sparse.img");
            remove("/usb/backup_img/cache.sparse.img.gz");
            remove("/usb/backup_img/userdata.sparse.img.gz");
        }

        sync();
        sprintf(tmp_path_hu, "%s/fwdn/qb_data.sparse.img", save_qbdata_path_hu);
        if(0 > tcc_cp(tmp_path_hu, USB_BACKUP)) {
            ui->Print("[%s] : [%s] copy failed!\n", __func__, tmp_path_hu);
        }

        sprintf(tmp_path_hu, "%s/fwdn/snapshot.sparse.img", save_qbdata_path_hu);
        if(0 > tcc_cp(tmp_path_hu, USB_BACKUP)) {
            ui->Print("[%s] : [%s] copy failed!\n", __func__, tmp_path_hu);
        }

        sprintf(tmp_path_hu, "%s/qb_data/cache.sparse.img.gz", save_qbdata_path_hu);
        if(0 > tcc_cp(tmp_path_hu, USB_BACKUP)) {
            ui->Print("[%s] : [%s] copy failed!\n", __func__, tmp_path_hu);
        }

        sprintf(tmp_path_hu, "%s/qb_data/userdata.sparse.img.gz", save_qbdata_path_hu);
        if(0 > tcc_cp(tmp_path_hu, USB_BACKUP)) {
            ui->Print("[%s] : [%s] copy failed!\n", __func__, tmp_path_hu);
        }

        sync();
        sync();
        ui->Print("copy end..\n");

        /* qb_data.sparse.imgbackup file cmp */
        ui->Print("backup file cmp.. \n");
        pid = fork();
        if(pid ==0) {
            __check_cmp_path("qb_data.sparse.img" ,save_qbdata_path_hu , (char*)USB_BACKUP );
        }
        status = wait_child_ps(pid);

        ui->Print("[%s] : qb_data.sparse child return [%d]\n", __func__, status);

        char tmp[256];
        sprintf(tmp, "rm -rf %s/*", save_qbdata_path_hu);
        system(tmp);

        if(remove(save_qbdata_path_hu) != 0) {
            ui->Print("delete fail - errno = %d\n", errno);
        } else {
            ui->Print("delete success - errno = %d\n", errno);
        }

        ui->Print("[%s]save_qbdata_path_hu =(%s) deleted...\n", __func__, save_qbdata_path_hu);

        remove(TEMP1_ROOT);
        result = ensure_path_unmounted(TEMP1_ROOT);
        if (result != 0) {
            sleep(1);
            ensure_path_unmounted(TEMP1_ROOT);
        }
#if 1 //jason.ku 2017.11.01 :: make extra_package_SK.KOR.XXX....x.tar file
		sync();
		sync();
		make_extra_package_func();
#endif
    }
    ui->Print("QB data end\n");
    sync();
    sync();

    ui->SetProgressType(RecoveryUI::DETERMINATE);
    ui->ui_set_status_message(RecoveryUI::INSTALL_SUCCESS_MESSAGE);

    memset(&boot_msg, 0, sizeof(struct bootloader_message));
    set_bootloader_message(&boot_msg);

    result = ensure_path_unmounted(QB_INTERNAL_ROOT);
    if (result == -1) {
        ensure_path_unmounted(QB_INTERNAL_ROOT);
    }

    if (extract_qb_data_only == 0) {
        result = ensure_path_unmounted(USB_ROOT);
        if (result == -1) {
            ensure_path_unmounted(USB_ROOT);
        }
    }

    sleep(5);
#if 1 //TODO : check micom
    hardReset();
#else
    android_reboot(ANDROID_RB_RESTART2, 0, (char *)"force_normal");
#endif
    sleep(5);

    while (1) {};
}

void clear_Snapshot()
{
	struct bootloader_message boot_msg;
	Device* device = make_device();

	ui = device->GetUI();
	ui->SetLocale(locale);
	ui->Init();
	ui->SetBackground(RecoveryUI::INSTALLING_UPDATE);
	ui->countTotal = 0;
	ui->countPresent = 0;
	ui->ShowText(true);

	ui->ui_set_status_message(RecoveryUI::INSTALLING_MESSAGE);
	device->StartRecovery();

	ui->Print("[%s] start snapshot partition erase!!\n", __func__);

	if(0 > erase_volume("/snapshot")) {
		ui->Print("[%s] snapshot partition erase failed\n", __func__);
	}

	memset(&boot_msg,0x00,sizeof(struct bootloader_message));
	set_bootloader_message(&boot_msg);

	sync();

	hardReset();

	sleep(5);

	while(1);
}

// qb_data restore
int update_qbdata_system(Device *device)
{
    int ret = 0;

    ret = write_sparse_image_no_zip(QUCIKBOOT_QBDATA_PATH, SYSTEM_UPDATE_QBDATA_DEV_PATH);

    if(ret == 0) {
        ret = ensure_path_unmounted(CACHE_ROOT);
        if(ret != 0) {
			ui->Print("[%d] cache umount error (%d)\n", __LINE__, ret);
			sleep(1);
			ensure_path_unmounted(CACHE_ROOT);
        }

        ret = ensure_path_unmounted(USERDATA_ROOT);
        if(ret != 0) {
			ui->Print("[%d] cache umount error (%d)\n", __LINE__, ret);
			sleep(1);
			ensure_path_unmounted(USERDATA_ROOT);
        }

        wipe_data_in_update(device);

        ret = ensure_path_mounted(QB_INTERNAL_ROOT);
        if(ret != 0) {
			ui->Print("[%d] cache umount error (%d)\n", __LINE__, ret);
			sleep(1);
			ensure_path_mounted(QB_INTERNAL_ROOT);
        }

        quickboot_factory_reset_opt(QB_OPT_DATA | QB_OPT_CACHE);

        ret = ensure_path_unmounted(QB_INTERNAL_ROOT);
        if(ret != 0) {
			ui->Print("[%d] cache umount error (%d)\n", __LINE__, ret);
			sleep(1);
			ensure_path_unmounted(QB_INTERNAL_ROOT);
        }

        ret = ensure_path_mounted(CACHE_ROOT);
        if(ret != 0) {
            ui->Print("[%d] cache umount error (%d)\n", __LINE__, ret);
            sleep(1);
            ensure_path_mounted(CACHE_ROOT);
        }

        ret = ensure_path_mounted(USERDATA_ROOT);
        if(ret != 0) {
			ui->Print("[%d] cache umount error (%d)\n", __LINE__, ret);
			sleep(1);
			ensure_path_mounted(USERDATA_ROOT);
        }
    }

	remove(QUCIKBOOT_QBDATA_PATH);

    return 0;
}

#if defined(TRIMBLE_GPS_SUPPORT)
int update_trimble_gps_module(void)
{
    int ret = 0;

    printf("[DEBUG] GPS TRIMBLE Mode \r\n");

    // 1. checking update file
    if (access(GPS_FILE,F_OK) != 0) {
        printf("[DEBUG] GPS file not found (%s) \r\n",GPS_FILE);
        return 1;
    } else {
        // 2. initiaize GUI
        ui->countPresent++;
        ui->ui_set_status_message(RecoveryUI::INSTALLING_MESSAGE);
        ui->SetUpdateLableType(RecoveryUI::CUR_UPDATE_LABLE_TYPE_GPS);
        ui->SetProgressType(RecoveryUI::DETERMINATE);

        // 3. display progress
        ui->ShowProgress(1.0, 100);

        // 4. update GPS module
        printf("[DEBUG] GPS TRIMBLE BISON Mode \r\n");
        ret = requestUpdateGPS2(GPS_FILE);

        // 5. update completion (GUI, etc..)
        printf("[DEBUG] GPS ret (%d) \r\n",ret);
        if (ret == 0) {
            ui->SetProgress(1);
			DeleteFile(GPS_FILE);
			DeleteFolder("/upgrade/gps");
            sleep(4);
        } else {
            return -2;
        }
    }

    return 0;
}
#endif

#if defined(LGIT9x28_MODEM_SUPPORT)
int update_lg9x28modem_module(void)
{
	int ret = 0;
	int loop = 0;
	int found_modem_driver = 0;

	printf("[DEBUG] Modem lg9x28 161019-2 start... \r\n");

	// 1. checking update file
	if (access(LGIT_MODEM_FILE,F_OK) != 0) {
		printf("[DEBUG] Modem update file not found \r\n");
		return 1;
	} else {

		// 2. checking ttyUSB0~2 driver, wait for 5 sec
		for (loop = 0; loop < 10; loop++) {
			if (access(LGIT_MODEM_DRIVER,F_OK) != 0) {
				printf("[DEBUG] Modem driver is not found (%s), loop (%d) \r\n",LGIT_MODEM_DRIVER,loop);
				sleep(1);
			} else {
				found_modem_driver = 1;
				break;
			}
		}

		if (found_modem_driver == 1) {
			// 3. initiaize GUI
			ui->countPresent++;
//			ui->SetStatusMessageHeight(MESSAGE_HEIGHT_DEFAULT_DY);
			ui->ui_set_status_message(RecoveryUI::INSTALLING_MESSAGE);
			ui->SetUpdateLableType(RecoveryUI::CUR_UPDATE_LABLE_TYPE_MODEM);
			ui->SetProgressType(RecoveryUI::DETERMINATE);

			// 4. display progress
			ui->ShowProgress(1.0, 180);
			sleep(2);

			// 5. update modem module
			log_start("/upgrade");
			ret = requestUpdateModem("/upgrade/modem",400,0);
			log_end();

			// 6. update completion (GUI, etc..)
			if (ret == 0) {
				ui->SetProgress(1);
				sleep(4);
			} else {
				printf("[DEBUG] Modem update fail, ret (%d) \r\n",ret);
				return -1;
			}
		} else {
			printf("[DEBUG] Modem driver is not found \r\n");
			return 2;
		}

	}
	return 0;
}
#endif

#ifdef SUPPORT_OTA
static int __process_redband_update_agent()
{
    int pipe_rbua_fd[2];
    int ret = 0;

    printf("[%s] start 2\n", __func__);

    pipe(pipe_rbua_fd);

    char* temp = (char*)malloc(10);
    sprintf(temp, "%d", pipe_rbua_fd[1]);

    pid_t pid = fork();

    if(pid == 0) {
        close(pipe_rbua_fd[0]);

        ret = execl("/sbin/rec_rb_ua", "rec_rb_ua", temp, NULL);

        if(ret != 0) {
            printf("[int __process_redband_update_agent()] error : %d\n", ret);
            perror("[int __process_redband_update_agent()] execl failed");
            exit(-1);
        }

        exit(0);
    }
    close(pipe_rbua_fd[1]);

    char buffer[1024];
	float prev_fraction;
	float result_fraction;
    FILE* from_child = fdopen(pipe_rbua_fd[0], "r");
    while (fgets(buffer, sizeof(buffer), from_child) != NULL) {

        if(strncmp(buffer, "Failed", 6) == 0) {
            printf("!!!!!!!!!!!!!!!!!!!!!!!! RB UA ERROR!!!!!!!!!!!!!!!!!!\n");
            ret = -1;
        } else {
            printf("[OTA LOG] : %s", buffer);
#if 1 //jason.ku 2018.01.05
			if(strncmp(buffer, "RB_Progress", 11) == 0) 
			{
				char *temp_str[8] = {NULL, };
				int i = 0;

				char *ptr = strtok(buffer, ": ");

				while(ptr != NULL)
				{
					temp_str[i] = ptr;
					i++;
					ptr = strtok(NULL, ": ");
				}
				float fraction = strtof(temp_str[2], NULL);
				//printf("==> jason : fraction = %f\n",fraction);

				if(prev_fraction < fraction)
				{
					if((fraction - prev_fraction) == 1)
						result_fraction = 1;
					else if((fraction - prev_fraction) > 1)
						result_fraction = fraction - prev_fraction;
					prev_fraction = fraction;
					ui->RB_UI_Progress(((float)(result_fraction)/100));
				}
			}
#endif
        }
    }
    fclose(from_child);
    close(pipe_rbua_fd[0]);

    int status;
    waitpid(pid, &status, 0);

    free(temp);

    printf("[%s] end\n", __func__);

    return ret;
}
#endif //SUPPORT_OTA

void display_error_and_copy_log(int err, RecoveryUI::TitleText text, const char *count_file)
{
    copy_log_file(TEMPORARY_LOG_FILE, LAST_LOG_FILE, false);
    copy_log_file(TEMPORARY_LOG_FILE, LAST_LOG_FILE_TO_STORAGE, false);

    if(UpdateCountFunc(count_file) == 0) {
        ui->SetUpdateLableType(text);
        ui->ui_set_status_message(RecoveryUI::INSATLL_ERROR_MESSAGE);
        ui->ui_set_error_number(err);

        sleep(10);
        hardReset();
        while (1) { };
    } else {
        ui->SetUpdateLableType(RecoveryUI::CUR_UPDATE_LABLE_TYPE_CONTACT_CUSTOMER);
        ui->ui_set_status_message(RecoveryUI::INSTALL_ERROR_CONTACN_CUSTOMER);
        ui->ui_set_error_number(err);

        while (1) { };
    }
}

int rmdirs(const char *path, int is_error_stop)
{
	DIR *dir_ptr = NULL;
	struct dirent *file = NULL;
	struct stat buf;
	char filename[1024];

	if((dir_ptr = opendir(path)) == NULL) {
		return unlink(path);
	}

	while((file = readdir(dir_ptr)) != NULL) {
		if(strcmp(file->d_name, ".") == 0 || strcmp(file->d_name, "..") == 0) {
			continue;
		}

		sprintf(filename, "%s/%s", path, file->d_name);

		if(lstat(filename, &buf) == -1) {
			continue;
		}

		if(S_ISDIR(buf.st_mode)) {
			if(rmdirs(filename, is_error_stop) == -1 && is_error_stop) {
				return -1;
			}
		} else if(S_ISREG(buf.st_mode) || S_ISLNK(buf.st_mode)) {
			if(unlink(filename) == -1 && is_error_stop) {
				return -1;
			}
		}
	}

    closedir(dir_ptr);
    return rmdir(path);
}

time_t print_time(time_t start)
{
	time_t current;

	time(&current);
	current = current - start;

	return current;
}

void display_force_reboot_error(int pError_num)
{
	copy_log_file(TEMPORARY_LOG_FILE, LAST_LOG_FILE, false);
	copy_log_file(TEMPORARY_LOG_FILE, LAST_LOG_FILE_TO_STORAGE, false);

	ui->ShowSpinProgress(false);
	ui->SetUpdateLableType(RecoveryUI::CUR_UPDATE_LABLE_TYPE_SYSTEM_ERROR);
	ui->ui_set_status_message(RecoveryUI::INSATLL_ERROR_MESSAGE);
	ui->ui_set_error_number(pError_num);

	request_to_enter_sleep_mode();

	sync();
	sleep(30);
	android_reboot(ANDROID_RB_RESTART2, 0, (char *)"");
	while (1) {};
}

void display_system_update_error(int pLabel,int pError_num)
{
	copy_log_file(TEMPORARY_LOG_FILE, LAST_LOG_FILE, false);
	copy_log_file(TEMPORARY_LOG_FILE, LAST_LOG_FILE_TO_STORAGE, false);

	request_to_enter_sleep_mode();

	if (UpdateCountFunc(UPDATE_COUNT_SYSTEM) != -1) {
		ui->ShowSpinProgress(false);
		ui->SetUpdateLableType(pLabel);
		ui->ui_set_status_message(RecoveryUI::INSATLL_ERROR_MESSAGE);
		ui->ui_set_error_number(pError_num);

		sleep(30);
		android_reboot(ANDROID_RB_RESTART2, 0, "recovery");
	} else {
		ui->ShowSpinProgress(false);
		ui->SetUpdateLableType(RecoveryUI::CUR_UPDATE_LABLE_TYPE_CONTACT_CUSTOMER);
		ui->ui_set_status_message(RecoveryUI::INSTALL_ERROR_CONTACN_CUSTOMER);
		ui->ui_set_error_number(pError_num);
	}

	sleep(30);
	while (1) {};
}

//---------------------------------------------------------
// mount the partition for USB updating
//---------------------------------------------------------
int mount_partition_for_USB_updating(void)
{
	int ret = 0;

	ret = ensure_path_mounted(UPDATE_ROOT);
	if(ret != 0) {
		printf("[ERROR] mount fail update partition, %d \r\n",ret);
		return -1;
	}

	ret = ensure_path_mounted(USB_ROOT);
	if (ret != 0) {
		printf("USB mount is not success, %d \r\n",ret);
		sleep(1);
		ensure_path_mounted(USB_ROOT);
	}

	ret = ensure_path_mounted(SDCARD_ROOT);
	if (ret != 0) {
		printf("SDCARD_ROOT mount is not success, %d \r\n",ret);
		sleep(1);
		ensure_path_mounted(SDCARD_ROOT);
	}

	ret = ensure_path_mounted(USERDATA_ROOT);
	if (ret != 0) {
		printf("data folder mount failed\n");
		sleep(1);
		ensure_path_mounted(USERDATA_ROOT);
	}

	// Mount NAVI
	ret = ensure_path_mounted(NAVI_ROOT);
	if (ret != 0) {
		printf("Navi partition  mount failed\n");
		sleep(1);
		ensure_path_mounted(NAVI_ROOT);
	}

	ret = ensure_path_mounted(NAVI_MIRROR_ROOT);
	if (ret != 0) {
		printf("Navi partition  mount failed\n");
		sleep(1);
		ensure_path_mounted(NAVI_MIRROR_ROOT);
	}

	// Mount VR 
	ret = ensure_path_mounted(VR1_ROOT);
	if (ret != 0) {
		printf("VR2 partition  mount failed\n");
		sleep(1);
		ensure_path_mounted(VR1_ROOT);
	}

	ret = ensure_path_mounted(VR2_ROOT);
	if (ret != 0) {
		printf("VR2 partition  mount failed\n");
		sleep(1);
		ensure_path_mounted(VR2_ROOT);
	}


	// mount OEM_DATA
	ret = ensure_path_mounted(OEM_DATA_ROOT);
	if (ret != 0) {
		printf("Navi partition  mount failed\n");
		sleep(1);
		ensure_path_mounted(OEM_DATA_ROOT);
	}

	// mount LOG 
	ret = ensure_path_mounted(STORAGE_LOG_ROOT);
	if (ret != 0) {
		printf("log partition  mount failed\n");
		sleep(1);
		ensure_path_mounted(STORAGE_LOG_ROOT);
	}

	return 0;
}

//---------------------------------------------------------
// check the validation about system package 
//---------------------------------------------------------
int check_validation_system_package(int enc)
{
	int ret = 0;
	int loop = 0;
	char file_path[256] = {0x00,};

	printf("[DEBUG] %s, %d \r\n",__func__,__LINE__);

	if (enc == 1) {
		/* check CPU SW *********************************************/
		ret = rec_info_file_parsing(SYSTEM_PACKAGE_INFO_PATH);
		if(ret != 0) {
			printf("[ERROR] ==> update.info file open error \n");
			return -1;
		}

		ret = check_validity_sha224("/navi/lk.rom", "lk.rom");
		if(ret != 0) {
			printf("[ERROR] ==> lk.rom sha224 check : ret = %d \n", ret);
			return -2;
		}

		ret = check_validity_sha224("/navi/boot.img", "boot.img");
		if(ret != 0) {
			printf("[ERROR] ==> boot.img sha224 check : ret = %d \n", ret);
			return -3;
		}

		ret = check_validity_sha224("/navi/device_tree.dtb", "device_tree.dtb");
		if(ret != 0) {
			printf("[ERROR] ==> dtb sha224 check : ret = %d \n", ret);
			return -4;
		}

		ret = check_validity_sha224("/navi/splash.img", "splash.img");
		if(ret != 0) {
			printf("[ERROR] ==> splash.img sha224 check : ret = %d \n", ret);
			return -5;
		}

		ret = check_validity_sha224("/navi/recovery.img", "recovery.img");
		if(ret != 0) {
			printf("[ERROR] ==> recovery.img sha224 check : ret = %d \n", ret);
			return -6;
		}

		ret = check_validity_sha224("/navi/system.ext4", "system.ext4");
		if(ret != 0) {
			printf("[ERROR] ==> system.img sha224 check : ret = %d \n", ret);
			return -7;
		}

	} else {

		/* check CPU SW *********************************************/
		ret = rec_info_file_parsing(SYSTEM_PACKAGE_INFO_PATH);
		if(ret != 0) {
			printf("[ERROR] ==> update.info file open error \n");
			return -1;
		}

		ret = check_validity_sha224(SYSTEM_UPDATE_LK_PATH, "lk.rom");
		if(ret != 0) {
			printf("[ERROR] ==> lk.rom sha224 check : ret = %d \n", ret);
			return -2;
		}

		ret = check_validity_sha224(SYSTEM_UPDATE_BOOT_PATH, "boot.img");
		if(ret != 0) {
			printf("[ERROR] ==> boot.img sha224 check : ret = %d \n", ret);
			return -3;
		}

		ret = check_validity_sha224(SYSTEM_UPDATE_DTB_PATH, "device_tree.dtb");
		if(ret != 0) {
			printf("[ERROR] ==> dtb sha224 check : ret = %d \n", ret);
			return -4;
		}

		ret = check_validity_sha224(SYSTEM_UPDATE_SPLASH_PATH, "splash.img");
		if(ret != 0) {
			printf("[ERROR] ==> splash.img sha224 check : ret = %d \n", ret);
			return -5;
		}

		ret = check_validity_sha224(SYSTEM_UPDATE_RECOVERY_PATH, "recovery.img");
		if(ret != 0) {
			printf("[ERROR] ==> recovery.img sha224 check : ret = %d \n", ret);
			return -6;
		}

		ret = check_validity_sha224(SYSTEM_UPDATE_SYSTEM_PATH, "system.ext4");
		if(ret != 0) {
			printf("[ERROR] ==> system.img sha224 check : ret = %d \n", ret);
			return -7;
		}
	}

	/* check MICOM firmware *********************************************/
	if (access(MICOM_FILE,F_OK) == 0) {
		ret = check_validity_sha224(MICOM_FILE, "micom_sw.bin");
		if(ret != 0) {
			printf("[ERROR] ==> micom_sw.bin sha224 check : ret = %d \n", ret);
			return -8;
		}
	}

	/* check GPS firmware *********************************************/
	if (access(GPS_FILE,F_OK) == 0) {
		ret = check_validity_sha224(GPS_FILE, "gps_module.bin");
		if(ret != 0) {
			printf("[ERROR] ==> micom_sw.bin sha224 check : ret = %d \n", ret);
			return -9;
		}
	}

	/* check MODEM firmware *********************************************/

#if defined(LGIT9x28_MODEM_SUPPORT)

	/*
	for (loop = 0; loop < MODEM_UPDATE_FIRMWARE_NUM; loop++) {
		memset(&file_path,0x00,sizeof(char)*256);
		sprintf(file_path,"/upgrade/modem/%s",modem_firmware_list[loop]);
		if (access(file_path,F_OK) == 0) {
			ret = check_validity_sha224(file_path, modem_firmware_list[loop]);
			if(ret != 0) {
				printf("[ERROR] ==> MODEM firmware (%d,%s), file path(%s) sha224 check : ret = %d \n",loop,modem_firmware_list[loop],file_path, ret);
				return -10;
			}
		}
	}
	*/
#endif

	return ret;
}

//---------------------------------------------------------
// Update the System package (bootloader, kernel, F/W, App)
//---------------------------------------------------------
int update_system_package(int enc_path)
{
	int ret = 0;

	printf("[DEBUG] %s, %d \r\n",__func__,__LINE__);

	if (enc_path == 1) {

#if 1
		printf("Don't actually do a damn thing");
#else
		ret = update_bootloader_function("/navi/lk.rom");
		if (ret != 0) {
			printf("update failure about bootloader, ret (%d) \r\n",ret);
			return -1;
		}

		ret = daudio_partition_write("/navi/boot.img",SYSTEM_UPDATE_BOOT_DEV_PATH);
		if (ret != 0) {
			printf("update failure about boot.img, ret (%d) \r\n",ret);
			return -2;
		}

		ret = daudio_partition_write("/navi/device_tree.dtb",SYSTEM_UPDATE_DTB_DEV_PATH);
		if (ret != 0) {
			printf("update failure about dtb, ret (%d) \r\n",ret);
			return -3;
		}

		ret = daudio_partition_write("/navi/splash.img",SYSTEM_UPDATE_SPLASH_DEV_PATH);
		if (ret != 0) {
			printf("update failure about splash, ret (%d) \r\n",ret);
			return -4;
		}

		ret = daudio_partition_write("/navi/recovery.img",SYSTEM_UPDATE_RECOVERY_DEV_PATH);
		if (ret != 0) {
			printf("update failure about recovery, ret (%d) \r\n",ret);
			return -5;
		}

		erase_volume("/system");
		sleep(1);
		ret = daudio_partition_write("/navi/system.ext4",SYSTEM_UPDATE_SYSTEM_DEV_PATH);
		if (ret != 0) {
			printf("update failure about system, ret (%d) \r\n",ret);
			return -6;
		}
#endif

	} else {

		ret = update_bootloader_function(SYSTEM_UPDATE_LK_PATH);
		if (ret != 0) {
			printf("update failure about bootloader, ret (%d) \r\n",ret);
			return -1;
		}

		ret = daudio_partition_write(SYSTEM_UPDATE_BOOT_PATH,SYSTEM_UPDATE_BOOT_DEV_PATH);
		if (ret != 0) {
			printf("update failure about boot.img, ret (%d) \r\n",ret);
			return -2;
		}

		ret = daudio_partition_write(SYSTEM_UPDATE_DTB_PATH,SYSTEM_UPDATE_DTB_DEV_PATH);
		if (ret != 0) {
			printf("update failure about dtb, ret (%d) \r\n",ret);
			return -3;
		}

		ret = daudio_partition_write(SYSTEM_UPDATE_SPLASH_PATH,SYSTEM_UPDATE_SPLASH_DEV_PATH);
		if (ret != 0) {
			printf("update failure about splash, ret (%d) \r\n",ret);
			return -4;
		}

		ret = daudio_partition_write(SYSTEM_UPDATE_RECOVERY_PATH,SYSTEM_UPDATE_RECOVERY_DEV_PATH);
		if (ret != 0) {
			printf("update failure about recovery, ret (%d) \r\n",ret);
			return -5;
		}

		erase_volume("/system");
		sleep(1);
		ret = daudio_partition_write(SYSTEM_UPDATE_SYSTEM_PATH,SYSTEM_UPDATE_SYSTEM_DEV_PATH);
		if (ret != 0) {
			printf("update failure about system, ret (%d) \r\n",ret);
			return -6;
		}
	}

	return ret;
}

//---------------------------------------------------------
// Update the quickboot package
//---------------------------------------------------------
int update_quickboot_package(char *update_tar_pkg_file,Device *device)
{
	int ret = 0;
	int file = 0;
	char quick_pkg_file[256] = {0,};
	char *quick_package_name = NULL;
	char buf[2] = {0x00,};

	erase_volume("/cache");
	erase_volume("/data");
	erase_volume("/snapshot");

	quick_package_name = strchr(update_tar_pkg_file,'_');
	quick_package_name = strchr(quick_package_name + 1, '_');

	memset(quick_pkg_file,0x00,sizeof(char)*256);
#if defined(SDCARD_UPDATE)
	sprintf(quick_pkg_file,"%s/%s%s",SDCARD_ROOT,QUICK_PACKAGE_NAME,quick_package_name);
#else
	sprintf(quick_pkg_file,"%s/%s%s",USB_ROOT,QUICK_PACKAGE_NAME,quick_package_name);
#endif

	printf("check the quickboot package : %s \n",quick_pkg_file);

	if (access(quick_pkg_file,F_OK) == 0) {
		sync();
		ret = lib_untar_f(quick_pkg_file, "/upgrade");
		if (ret != 0) {
			printf("[ERROR] untar error code : %d \r\n",ret);
			return -1;
		}
		sync();

		ret = update_qbdata_system(device);
		if (ret != 0) {
			printf("[ERROR] qb data update error code : %d \r\n",ret);
			return -2;
		}

		ret = write_sparse_image_no_zip(QUCIKBOOT_IMAGE_PATH,SYSTEM_UPDATE_QUICKBOOT_DEV_PATH);
		if (ret != 0) {
			printf("[ERROR] snapshot update error code : %d \r\n",ret);
			return -3;
		}
		remove(QUCIKBOOT_IMAGE_PATH);

		//make restoring file
		file = open(QUICKBOOT_UPDATED_FLAG,O_WRONLY|O_CREAT|O_TRUNC,0766);
		if (file < 0) {
			printf("[ERROR] %s, %d, error (%d) \r\n",__func__,__LINE__,file);
			return -4;
		} else {
			memset(buf,0x00,2);
			buf[0] = '1';
			lseek(file,0,SEEK_SET);
			write(file,buf,1);
			close(file);
			chmod(QUICKBOOT_UPDATED_FLAG,0766);
		}
	}

	return 0;
}

//---------------------------------------------------------
// Descrypt the navi information file
//---------------------------------------------------------
#define NAVI_DEC_KEY 0x11  //need for tarlist decryption
int navi_list_decrypt_func(const char *encryped_file_path, const char *decrypt_file_path)
{
	FILE *rfp = NULL;
	FILE *wfp = NULL;
	char ch = 0x00;

	rfp = fopen(encryped_file_path, "r");
	if(rfp == NULL)
	{
		fclose(rfp);
		return -1;
	}

	wfp = fopen(decrypt_file_path, "w");
	if(wfp == NULL)
	{
		fclose(rfp);
		fclose(wfp);
		return -2;
	}

	while(!feof(rfp))
	{
		ch = fgetc(rfp);
		fputc((ch^NAVI_DEC_KEY), wfp);
	}

	fclose(rfp);
	fclose(wfp);

	return 0;
}

/*
 * compare map version 
 *
 * return : '1' - upper,  '0' - same, '-1' - under, '-2' - can't compare, '-10' : unmatch REGION
 */
int compare_map_version(MAP_LISTS_STRUCT map_lists)
{
	int file = 0;
	int num = 0;
	char map_version[NAVI_VERSION_STRING_MAX] = {0x00,};

	printf("[%s][%d] \n",__func__,__LINE__);

	// read map info file
	file = open(NAVI_MAP_VERSION_INFO, O_RDONLY);
	if (file < 0) {
		return -2;
	}

	memset(map_version,0x00,sizeof(char)*NAVI_VERSION_STRING_MAX);
	num = read(file,map_version,NAVI_VERSION_STRING_MAX);

	if (num < 0) {
		close(file);
		return -2;
	}
	close(file);

	// compare map version
	printf("[%s][%d] current (%s), usb (%s)  \n",__func__,__LINE__,map_version,map_lists.navi_version);

	if (strncmp(map_version,map_lists.navi_version,3) != 0) {
		return -10;
	}

	num = strlen(map_version);
	if (strncmp(map_version,map_lists.navi_version,num) == 0) {
		return 0;
	} else if (strncmp(map_version,map_lists.navi_version,num) < 0) {
		return 1;
	} else if (strncmp(map_version,map_lists.navi_version,num) == 0) {
		return -1;
	}

	return -2;
}

//---------------------------------------------------------
// parsing the navi information file
//---------------------------------------------------------
int parse_navi_map_file(const char *path_name, MAP_LISTS_STRUCT *map_lists)
{
	int file = 0;
	int num = 0;
	int loop = 0;
	int sub_loop = 0;
	char *read_buf;
	char *line_token = NULL;
	char *line_separator = "@";
	char *separator = ",";

	char tmp_buf[STR_MAX_LENGTH] = {0x00,};
	char navi_map_list[NAVI_TAR_MAX][STR_MAX_LENGTH] = {0x00,};

	struct stat sourcefstat;

	printf("[DEBUG] %s,%d \r\n",__func__,__LINE__);

	read_buf = (char *)malloc(sizeof(char)*NAVI_MAP_INFOFILE_SIZE);

	/* Open File **********************************************************************************/
	file = open(path_name, O_RDONLY);
	if (file < 0) {
		printf("[%s][%d] File(%s) Open ierror : %d  %\n",__func__,__LINE__,path_name,file);
		free(read_buf);
		return -1;
	}

	/* Read File **********************************************************************************/
	memset(read_buf,0x00,sizeof(char)*NAVI_MAP_INFOFILE_SIZE);
	num = read(file,read_buf,NAVI_MAP_INFOFILE_SIZE);
	if (num < 0) {
		free(read_buf);
		close(file);
		return -2;
	}

	/* Close File **********************************************************************************/
	close(file);


	/* Parsing Data ********************************************************************************/

	line_token = strtok(read_buf,line_separator);
	if (line_token == NULL) {
		printf("[%s][%d] token(parsing) error  %\n",__func__,__LINE__);
		free(read_buf);
		close(file);
		return -3;
	}

	strncpy(navi_map_list[0],line_token,STR_MAX_LENGTH);

	num = 1;
	while (line_token = strtok(NULL,line_separator)) {
		if (num < NAVI_TAR_MAX) {
			strncpy(navi_map_list[num],line_token,STR_MAX_LENGTH);
		}
		num++;
	}

	if (read_buf != NULL)
		free(read_buf);

	memset(map_lists,0x00,sizeof(MAP_LISTS_STRUCT));

	/* set map list struct *************************************************************/
	for (loop = 0; loop < num; loop++) {

		memset(tmp_buf,0x00,sizeof(char)*STR_MAX_LENGTH);
		strcpy(tmp_buf,navi_map_list[loop]);
		line_token = strtok(tmp_buf,separator);

		if (strncmp(line_token,"FILE:",5) == 0) {
			for (sub_loop = 5; sub_loop < strlen(line_token); sub_loop++) {
				map_lists->navi_tar_str[loop][sub_loop-5] = line_token[sub_loop];
			}
			map_lists->tar_cnt++;
		} else if (strncmp(line_token,"VERSION:",8) == 0) {
			for (sub_loop = 8; sub_loop < strlen(line_token); sub_loop++) {
				map_lists->navi_version[sub_loop-8] = line_token[sub_loop];
			}
		}
	}

	for (loop = 0; loop < map_lists->tar_cnt; loop++) {
		printf("[%s][%d] Map File(%s)  %\n",__func__,__LINE__,map_lists->navi_tar_str[loop]);
	}
	printf("[%s][%d] version : %s \r\n",__func__,__LINE__,map_lists->navi_version);

	/* check map file size **********************************************************************/
	for(loop=0; loop < map_lists->tar_cnt; loop++) {
		memset(tmp_buf,0x00,sizeof(char)*STR_MAX_LENGTH);
		sprintf(tmp_buf, "%s/%s", USB_ROOT, map_lists->navi_tar_str[loop]);

		num = stat(tmp_buf, &sourcefstat);
		if (num != 0) {
			LOGE("[%s][%d] Fil(%s) stat error (%d)   %\n",__func__,__LINE__,map_lists->navi_tar_str[loop],num);
			return -4;
		}
		map_lists->TotalFileSize += sourcefstat.st_size;
	}

	return 0;
}

//---------------------------------------------------------
// Force to Update the Navi Map 
//---------------------------------------------------------
int update_foreground_map(void)
{
	int ret = 0;
	int file = 0;
	int loop = 0;
	char map_file_path[128] = {0x00,};
	char map_target_path[8] = {0x00,};
	char read_buf[4] = {0x00,};
	MAP_LISTS_STRUCT map_lists;

	if (access(NAVI_LIST_ENC_PATH,F_OK) == 0) {

		ret = navi_list_decrypt_func(NAVI_LIST_ENC_PATH, NAVI_LIST_DEC_PATH);
		if (ret != 0) {
			printf("[ERROR][%s:%d] navi info file decrypted error : %d \r\n",__func__,__LINE__,ret);
			return -1;
		}

		if (access(NAVI_LIST_DEC_PATH,F_OK) == 0) {

			// parsing the map info file
			ret = parse_navi_map_file(NAVI_LIST_DEC_PATH,&map_lists);
			if (ret != 0) {
				printf("[ERROR][%s:%d] parsing error : %d \r\n",__func__,__LINE__,ret);
				return -2;
			} 

			// compare the map version : 
			// return : '1' - upper,  '0' - same, '-1' - under, '-2' - can't compare, '-10' : unmatch REGION
			ret = compare_map_version(map_lists);
			printf("[DEBUG][%s:%d] MAP version ret : %d \r\n",__func__,__LINE__,ret);
			if (ret == -10) {
				return -11;
			} else if (ret == 0 || ret == -1) {
				return 1;
			}

			remove(NAVI_LIST_DEC_PATH);

			// TODO : check navi map index 
			if (access(NAVI_INDEX_INFO,F_OK) == 0) {
				file = open(NAVI_INDEX_INFO,O_RDONLY);
				if (file < 0) {
					printf("[ERROR] %s, %d, error (%d) \r\n",__func__,__LINE__,file);
					return -4;
				}

				ret = read(file,read_buf,1);
				if (ret < 0) {
					close(file);
					printf("[ERROR] %s, %d, read error (%d) \r\n",__func__,__LINE__,file);
					return -5;
				}
				ret = 0;
				close(file);
				
				memset(map_target_path,0x00,sizeof(char)*8);
				if (read_buf[0] == '0') {
					strcpy(map_target_path,NAVI_MIRROR_ROOT);
				} else if (read_buf[0] == '1') {
					strcpy(map_target_path,NAVI_ROOT);
				} else {
					printf("[ERROR] %s, %d, read data error (%d) \r\n",__func__,__LINE__,file);
					return -6;
				}

			} else {
				memset(map_target_path,0x00,sizeof(char)*8);
				strcpy(map_target_path,NAVI_MIRROR_ROOT);
			}

			// untar the navi map files
			for (loop = 0; loop < map_lists.tar_cnt; loop++) {
				memset(map_file_path,0x00,sizeof(char)*128);

			#if defined(SDCARD_UPDATE)
				sprintf(map_file_path,"%s/%s",SDCARD_ROOT,map_lists.navi_tar_str[loop]);
			#else
				sprintf(map_file_path,"%s/%s",USB_ROOT,map_lists.navi_tar_str[loop]);
			#endif

				printf("[DEBUG] [%d] [%s] [%s] \r\n",loop,map_lists.navi_tar_str[loop],map_file_path);
				ret = lib_untar_f(map_file_path, map_target_path);
				{
					char mbuf[256] = {0x00,};
					sprintf(mbuf, "%s", "echo 1 > /proc/sys/vm/drop_caches");
					system(mbuf);
				}
				if (ret != 0) {
					return -10;
				}
			}
		}
	} else {	// if (access(NAVI_LIST_ENC_PATH,F_OK) == 0)
		return 1;
	}

	return 0;
}


//---------------------------------------------------------
// Update the Navi Map 
//---------------------------------------------------------
int update_navi_map(void)
{
	int ret = 0;
	int file = 0;
	int num = 0;
	char read_buf[2] = {0x00,};

	printf("[DEBUG] %s, %d \r\n",__func__,__LINE__);

	// check the navi updated
	if (access(NAVI_UPDATED_FLAG,F_OK) == 0) {		//NAVI_UPDATED_FLAG = "/upgrade/map_update_complete.cfg";
		printf("[DEBUG] %s, %d \r\n",__func__,__LINE__);

		// check the navi index file
		if (access(NAVI_INDEX_INFO,F_OK) == 0) {	//NAVI_INDEX_INFO = "/oem_data/map_index.cfg" 
			printf("[DEBUG] %s, %d \r\n",__func__,__LINE__);

			// read the current navi index file
			file = open(NAVI_INDEX_INFO,O_RDONLY);
			if (file < 0) {
				printf("[ERROR] %s, %d, error (%d) \r\n",__func__,__LINE__,file);
				return -1;
			} else {

				num = read(file,read_buf,1);
				if (num < 0) {
					close(file);
					return -2;
				}
				close(file);
			}

			// rename & format navi map
			if (read_buf[0] == '0') {

				if (access(NAVI_MIRROR_UPDATE_PATH,F_OK) == 0) {

					erase_volume(NAVI_ROOT);

					if (access(NAVI_MIRROR_MAIN_PATH,F_OK) == 0) {
						ret = rmdirs(NAVI_MIRROR_MAIN_PATH,1);
						printf("[DEBUG:%d] rmdirs ret : %d\r\n",__LINE__,ret);
					}

					unlink(NAVI_MIRROR_MAIN_PATH);
					sync();

					ret = rename(NAVI_MIRROR_UPDATE_PATH,NAVI_MIRROR_MAIN_PATH);
					printf("[DEBUG:%d] rename ret : %d\r\n",__LINE__,ret);

					// update navi index file
					file = open(NAVI_INDEX_INFO,O_WRONLY|O_TRUNC);
					if (file < 0) {
						printf("[ERROR] %s, %d, error (%d) \r\n",__func__,__LINE__,file);
						return -5;
					} else {
						memset(read_buf,0x00,2);
						read_buf[0] = '1';
						lseek(file,0,SEEK_SET);
						write(file,read_buf,1);
						close(file);
						chmod(NAVI_INDEX_INFO,0766);
					}

					sleep(1);
					ret = ensure_path_mounted(NAVI_ROOT);
					printf("[DEBUG] mount /navi ret : %d \r\n",ret);
					ret = symlink("/storage/navi2/navi",NAVI_MAIN_PATH);
					printf("[DEBUG] symlink ret : %d \r\n",ret);
				}
			} else if (read_buf[0] == '1') {

				if (access(NAVI_UPDATE_PATH,F_OK) == 0) {

					erase_volume(NAVI_MIRROR_ROOT);

					if (access(NAVI_MAIN_PATH,F_OK) == 0) {
						ret = rmdirs(NAVI_MAIN_PATH,1);
						printf("[DEBUG:%d] rmdirs ret : %d\r\n",__LINE__,ret);
					}
					unlink(NAVI_MAIN_PATH);
					sync();

					ret = rename(NAVI_UPDATE_PATH,NAVI_MAIN_PATH);
					printf("[DEBUG:%d] rename ret : %d\r\n",__LINE__,ret);

					// update navi index file
					file = open(NAVI_INDEX_INFO,O_WRONLY|O_TRUNC);
					if (file < 0) {
						printf("[ERROR] %s, %d, error (%d) \r\n",__func__,__LINE__,file);
						return -6;
					} else {
						memset(read_buf,0x00,2);
						read_buf[0] = '0';
						lseek(file,0,SEEK_SET);
						write(file,read_buf,1);
						close(file);
						chmod(NAVI_INDEX_INFO,0766);
					}
				}
			} else {

				printf("[ERROR] %s, %d, read buf error (%x) \r\n",__func__,__LINE__,read_buf[0]);
				return -3;
			}

		} else {
			printf("[DEBUG] %s, %d \r\n",__func__,__LINE__);

			if (access(NAVI_MIRROR_UPDATE_PATH,F_OK) == 0) {

				erase_volume(NAVI_ROOT);

				if (access(NAVI_MIRROR_MAIN_PATH,F_OK) == 0) {
					ret = rmdirs(NAVI_MIRROR_MAIN_PATH,1);
					printf("[DEBUG:%d] rmdirs ret : %d\r\n",__LINE__,ret);
				}
				unlink(NAVI_MIRROR_MAIN_PATH);
				sync();

				ret = rename(NAVI_MIRROR_UPDATE_PATH,NAVI_MIRROR_MAIN_PATH);
				printf("[DEBUG:%d] rename ret : %d\r\n",__LINE__,ret);

				// update navi index file
				file = open(NAVI_INDEX_INFO,O_WRONLY|O_CREAT|O_TRUNC,0766);
				if (file < 0) {
					printf("[ERROR] %s, %d, error (%d) \r\n",__func__,__LINE__,file);
					return -6;
				} else {
					memset(read_buf,0x00,2);
					read_buf[0] = '1';
					lseek(file,0,SEEK_SET);
					write(file,read_buf,1);
					close(file);
					chmod(NAVI_INDEX_INFO,0766);
				}

				sleep(1);
				ret = ensure_path_mounted(NAVI_ROOT);
				printf("[DEBUG] mount /navi ret : %d \r\n",ret);
				ret = symlink("/storage/navi2/navi",NAVI_MAIN_PATH);
				printf("[DEBUG] symlink ret : %d \r\n",ret);
			}
		}
	}

	remove(NAVI_UPDATED_FLAG);
	sync();

	/*
	if (access(NAVI_UPDATE_PATH,F_OK) == 0) {
		printf("exist navi update  path\n");
		if (access(NAVI_MAIN_PATH,F_OK) == 0) {
			printf("delete navi main dir\n");
			if (rmdirs(NAVI_MAIN_PATH,1) == 0) {
				sync();
				rename(NAVI_UPDATE_PATH,NAVI_MAIN_PATH);
			}
		} else {
			rename(NAVI_UPDATE_PATH,NAVI_MAIN_PATH);
		}
		sync();
	}
	*/

	return ret;
}

//---------------------------------------------------------
// Update the VR2 db 
//---------------------------------------------------------
int update_vr2_db(void)
{
	int ret = 0;

	printf("[DEBUG] %s, %d \r\n",__func__,__LINE__);

	if (access(VR2_UPDATE_PATH,F_OK) == 0) {
		printf("exist vr2 update  path\n");
		if (access(VR2_MAIN_PATH,F_OK) == 0) {
			printf("delete vr2 main dir\n");
			if (rmdirs(VR2_MAIN_PATH,1) == 0) {
				sync();
				rename(VR2_UPDATE_PATH,VR2_MAIN_PATH);
			}
		} else {
			rename(VR2_UPDATE_PATH,VR2_MAIN_PATH);
		}
		sync();
	}

	return ret;
}

//---------------------------------------------------------
// request to enter  sleep mode when scheduling updating
//---------------------------------------------------------
int request_to_enter_sleep_mode(void)
{
	if (access(FOREGROUND_UPDATE_FLAG,F_OK) == 0) {
		cancelReservationUpdate();
		requestShutdown();
	}

	return 0;
}

int print_enable = 0;

int main(int argc, char **argv) 
{
	int ret;
	int arg;
	int previous_runs = 0;

	const char *send_intent = NULL;
	const char *update_package = NULL;
	const char *extract_qbimg_path = NULL;
	int wipe_data = 0, wipe_cache = 0, show_text = 0;
	int wipe_snapshot = 0, extract_qbimg_auto = 0, extract_qb_data_only = 0;

//	int status = INSTALL_SUCCESS;

	char buildProduct[PRODUCT_MAX][PROPERTY_VALUE_MAX];

	bool just_exit = false;
	time_t start = time(NULL);

	char update_tar_path[256] = {0,};
	char update_untar_cmd[256] = {0,};
	char *update_tar_pkg_file = NULL;

	char update_vr1_pkg_file[256] = {0,};
	char *vr1_parsing_name = NULL;

	int untar_status;

	time_t start_time_sec;
	time_t module_time_sec;

	FILE *fd = NULL;

	time(&start_time_sec);

	// If these fail, there's not really anywhere to complain...
	freopen(TEMPORARY_LOG_FILE, "a", stdout); setbuf(stdout, NULL);
	freopen(TEMPORARY_LOG_FILE, "a", stderr); setbuf(stderr, NULL);

	// If this binary is started with the single argument "--adbd",
	// instead of being the normal recovery binary, it turns into kind
	// of a stripped-down version of adbd that only supports the
	// 'sideload' command.  Note this must be a real argument, not
	// anything in the command file or bootloader control block; the
	// only way recovery should be run with this argument is when it
	// starts a copy of itself from the apply_from_adb() function.
	/*
	if (argc == 2 && strcmp(argv[1], "--adbd") == 0) {
		adb_main();
		return 0;
	}
	*/

	char buf[PROPERTY_VALUE_MAX];
	memset(buf, 0, PROPERTY_VALUE_MAX);
	property_get(QBMANAGER_QB_ENABLED, buf, "");
	if(!strncmp(buf, "1", 1))
		QUICKBOOT_SNAPSHOT = true;

	printf("Starting recovery on %s", ctime(&start));

    memset(buf, 0, PROPERTY_VALUE_MAX);
//	property_get("ro.product.locale.language", buf, "");
//	printf("ro.product.locale.language : %s\n", buf); 
//	memcpy(locale, "en", 2);    /* TODO : get locale... it's test code */

	memset(buf, 0, PROPERTY_VALUE_MAX);
	property_get("ro.build.product", buf, "");
	printf("ro.build.product : %s\n", buf);

	memset(buildProduct, 0, sizeof(buildProduct));
	getBuildProduct((char *)buildProduct, buf, '.');
	printf("mBuildProduct [%s][%s]\n", buildProduct[PRODUCT_MODEL], buildProduct[PRODUCT_REGION]);

	load_volume_table();
	ensure_path_mounted(LAST_LOG_FILE);
	rotate_last_logs(10);
	get_args(&argc, &argv);

#ifdef TCC_FACTORY_RESET_SUPPORT
	if(argv[1] == 0)
	{
		wipe_data = wipe_cache = 1;
		if (QUICKBOOT_SNAPSHOT)
	  wipe_snapshot = 1;
	}
	else
#endif
	{
		while ((arg = getopt_long(argc, argv, "", OPTIONS, NULL)) != -1) {
			switch (arg) {
				case 'p': previous_runs = atoi(optarg); break;
				case 's': send_intent = optarg; break;
				case 'u': update_package = optarg; break;
				case 'w':
					wipe_data = wipe_cache = 1;
					if(QUICKBOOT_SNAPSHOT)
						wipe_snapshot = 1;
					break;
				case 'c': wipe_cache = 1; break;
				case 't': show_text = 1; break;
				case 'x': just_exit = true; break;
				case 'l': memcpy(optarg, locale, 2); break;
				case 'y': extract_qbimg_auto = 1; extract_qb_data_only = 1; break;
				case 'q': extract_qbimg_auto = 1; extract_qbimg_path = optarg; break;
				case 'r': wipe_data = wipe_cache = 1; break;
				case 'n': wipe_snapshot = 1; break;
				case '?':
					LOGE("Invalid command argument\n");
					continue;
		   }
		}
	}

    if (locale[0] == 0) {
        load_locale_from_cache();
    }

	printf("locale is [%s]\n", locale);
	printf("update_package is [%s]\n", update_package);
	printf("wipe_data: [%d] wipe_snapshot:[%d] wipe_cache:[%d]\n", wipe_data,wipe_snapshot,wipe_cache);
	printf("show_text: [%d] just_exit:[%d] \n", show_text,just_exit);
	printf("extract_qbimg_auto: [%d] extract_qb_data_only: [%d]\n", extract_qbimg_auto, extract_qb_data_only);
	printf("extract_qbimg_path: [%s]\n", extract_qbimg_path);

	if(isFactoryResetCommand(wipe_data, wipe_cache)) {
		start_Factory_Reset();
	} else if(extract_qbimg_auto) {
		print_enable = 1;
		extract_Qbimg_Data(extract_qbimg_path, extract_qb_data_only);
		print_enable = 0;
	} else if(wipe_snapshot) {
		clear_Snapshot();
	}

//	------------------------------------------------------------------------------------------------------------

	ret = mount_partition_for_USB_updating();
	if (ret != 0) {
		goto exit;
	}

	Device* device;
	device = make_device();
	ui = device->GetUI();
	gCurrentUI = ui;
	ui->SetLocale(locale);
	ui->Init();
	ui->SetBackground(RecoveryUI::INSTALLING_UPDATE);
	ui->countTotal = 4;
	ui->countPresent = 0;
	ui->ui_set_status_message(RecoveryUI::INSTALLING_MESSAGE);

	makeLogFiles();

#ifdef HAVE_SELINUX
	struct selinux_opt seopts[] = {
	  { SELABEL_OPT_PATH, "/file_contexts" }
	};

	sehandle = selabel_open(SELABEL_CTX_FILE, seopts, 1);

	if (!sehandle) {
		printf("Warning: No file_contexts\n");
	}
#endif

	if (update_package) {
		// For backwards compatibility on the cache partition only, if
		// we're given an old 'root' path "CACHE:foo", change it to
		// "/cache/foo".
		if (strncmp(update_package, "CACHE:", 6) == 0) {
			int len = strlen(update_package) + 10;
			char* modified_path = (char*)malloc(len);
			strlcpy(modified_path, "/cache/", len);
			strlcat(modified_path, update_package+6, len);
			printf("(replacing path \"%s\" with \"%s\")\n", update_package, modified_path);
			update_package = modified_path;
		}
	}
	printf("\n");

	if ((update_package != NULL) && (strncmp(update_package,"/upgrade/system_",16) == 0)) {

		update_Start_C_Func();

		printf("Current time : %d \n",print_time(start_time_sec));

		// check the package name for system update
		update_tar_pkg_file = strrchr(update_package, '/');
		printf("update_tar_pkg_file = %s\n",update_tar_pkg_file);

		if(update_tar_pkg_file != NULL) {
		#if defined(SDCARD_UPDATE)
			sprintf(update_tar_path, "%s%s",SDCARD_ROOT, update_tar_pkg_file);
		#else
			sprintf(update_tar_path, "%s%s",USB_ROOT, update_tar_pkg_file);
		#endif
			
			printf("update_tar_path = %s\n",update_tar_path);

			//----------------------------------------------
			// Check the force map updte 
			//----------------------------------------------
			if (access(FOREGROUND_UPDATE_FLAG,F_OK) == 0) {
				if (access(NAVI_FOREGROUND_UPDATE_WORKING,F_OK) != 0) {
					time(&module_time_sec);

					ui->countTotal = 5;
					ui->countPresent++;
					ui->ui_set_status_message(RecoveryUI::INSTALLING_MESSAGE);
					ui->SetUpdateLableType(RecoveryUI::CUR_UPDATE_LABLE_TYPE_MAP);
					ui->SetProgressType(RecoveryUI::DETERMINATE);
					if ((locale[0] == 'k') && (locale[1] == 'o')) {
						ui->ShowProgress(1.0, 600);
					} else if ((locale[0] == 'e') && (locale[1] == 'n')) {
						ui->ShowProgress(1.0, 900);
					} else {
						ui->ShowProgress(1.0, 600);
					}
					ui->ShowSpinProgress(true);

					ret = update_foreground_map();
					printf("[DEBUG] forground map update (%d) \n", ret);
					if (ret < 0) { 
						remove(LOCALE_FILE);
						remove(FOREGROUND_UPDATE_FLAG);
						remove(NAVI_FOREGROUND_UPDATE_WORKING);

						if (ret == -10) {
							display_force_reboot_error(REC_NAVI_VR_UNTAR_ERROR);
						} else if (ret == -11) {
							display_force_reboot_error(REC_NAVI_VR_INCORRECT_REGION_ERROR);
						} else {
							display_force_reboot_error(REC_NAVI_VR_ERROR);
						}
					} else if (ret == 1) {
						printf("[DEBUG] No existing or Skip the Navi files \n");
					} else {
						fd = fopen(NAVI_FOREGROUND_UPDATE_WORKING, "a+");
						if (fd != 0) {
							printf("open error %s : %d \n",NAVI_FOREGROUND_UPDATE_WORKING,fd);
						}
						fclose(fd);

						fd = fopen(NAVI_UPDATED_FLAG, "a+");
						if (fd != 0) {
							printf("open error %s : %d \n",NAVI_UPDATED_FLAG,fd);
						}
						fclose(fd);
						sync();
					}

					sleep(3);
					ui->SetProgress(1);
					printf("navi map untar time : %d \n",print_time(module_time_sec));
				}
			}

			time(&module_time_sec);

			ui->countPresent++;
			ui->ui_set_status_message(RecoveryUI::INSTALLING_MESSAGE);
			ui->SetUpdateLableType(RecoveryUI::CUR_UPDATE_LABLE_TYPE_NORMAL);
			ui->SetProgressType(RecoveryUI::DETERMINATE);
			ui->ShowProgress(1.0, 300);
			ui->ShowSpinProgress(true);

			ret = access(SYSTEM_UPDATE_WORKING,F_OK);
			if (ret != 0) {

				printf("[DEBUG] %s not found : first step \n",SYSTEM_UPDATE_WORKING);

				if (access(update_tar_path,F_OK) == 0) {

					//----------------------------------------------
					// Untar system_package....
					//----------------------------------------------
					untar_status = lib_untar_f(update_tar_path, "/upgrade");

					if (untar_status != 0) {
						printf("untar untar_status = %d\n", untar_status);
						display_force_reboot_error(REC_SYSTEM_UNTAR_ERROR);
					}
					printf("untar time : %d \n",print_time(module_time_sec));

					// -----------------------------------------------------
					// cache restore
					// -----------------------------------------------------
					{
						char mbuf[256] = {0x00,};
						sprintf(mbuf, "%s", "echo 1 > /proc/sys/vm/drop_caches");
						system(mbuf);
					}

					// -----------------------------------------------------
					// untar vr2
					// -----------------------------------------------------
					if (access(FOREGROUND_UPDATE_FLAG,F_OK) == 0) {
						printf("[DEBUG] check vr2 untar ... \n");

						if (access("/upgrade/vr/VR2_PACKAGE.tar",F_OK) == 0) {
							printf("[DEBUG] untar %s ... \n","/upgrade/vr/VR2_PACKAGE.tar");
							untar_status = lib_untar_f("/upgrade/vr/VR2_PACKAGE.tar", "/vr2");
							if (untar_status != 0) {
								printf("ntar_status = %d\n", untar_status);
								display_force_reboot_error(REC_SYSTEM_UNTAR_ERROR);
							}

							// -----------------------------------------------------
							// cache restore
							// -----------------------------------------------------
							{
								char mbuf[256] = {0x00,};
								sprintf(mbuf, "%s", "echo 1 > /proc/sys/vm/drop_caches");
								system(mbuf);
							}
						}
					}

					//----------------------------------------------
					// Check the signing
					//----------------------------------------------
				#if defined(UPDATE_FILE_SIGNING)
					if ( (access("/upgrade/update_info",F_OK) == 0) && (access("/upgrade/update.info",F_OK) == 0) ) {
						printf("[DEBUG] Check the RSA ............ \n");
						ret = rsa_hash_compare("/upgrade/update_info", "/upgrade/update.info");
						if (ret < 0) {
							printf("[DEBUG] ret : %d \r\n",ret);
							display_force_reboot_error(REC_SYSTEM_VALIDATION_FILE_ERROR);
						}
					}
				#endif

					//----------------------------------------------
					// Descryption 
					//----------------------------------------------

					if (access("/upgrade/enc_micom/enc_micom_sw.bin",F_OK) == 0) {

						rename("/upgrade/enc_micom","/upgrade/micom");
						sync();
						sleep(1);

						ret = check_tcc_AES128("/upgrade/micom/enc_micom_sw.bin","/upgrade/micom/micom_sw.bin");
						if (ret != 0) {
							printf("[ERROR] lk.rom - Descryption error : %d \r\n",ret);
							display_force_reboot_error(REC_SYSTEM_VALIDATION_FILE_ERROR);
						}

						{
							//add the signature for MICOM
							int mFd = 0;
							unsigned char mBuf[4] = {0x41,0x41,0x41,0x41};

							printf("[DEBUG] write signature data in micom \r\n");

							mFd = open("/upgrade//micom/micom_sw.bin",O_WRONLY | O_APPEND);
							if (mFd < 0) {
								printf("[ERROR] open(upgrade//micom/micom_sw.bin) error : %d \r\n",ret);
							}
							ret = write(mFd,mBuf,sizeof(unsigned char)*4);
							close(mFd);
						}
					}

					if ( (access(ENC_SYSTEM_UPDATE_LK_PATH,F_OK) == 0) &&  
							(access(ENC_SYSTEM_UPDATE_BOOT_PATH,F_OK) == 0) &&
							(access(ENC_SYSTEM_UPDATE_DTB_PATH,F_OK) == 0) &&
							(access(ENC_SYSTEM_UPDATE_RECOVERY_PATH,F_OK) == 0) &&
							(access(ENC_SYSTEM_UPDATE_SYSTEM_PATH,F_OK) == 0) ) {

						ret = check_tcc_AES128(ENC_SYSTEM_UPDATE_LK_PATH,"/navi/lk.rom");
						if (ret != 0) {
							printf("[ERROR] lk.rom - Descryption error : %d \r\n",ret);
							display_force_reboot_error(REC_SYSTEM_VALIDATION_FILE_ERROR);
						}

						ret = check_tcc_AES128(ENC_SYSTEM_UPDATE_BOOT_PATH,"/navi/boot.img");
						if (ret != 0) {
							printf("[ERROR] boot.img - Descryption error : %d \r\n",ret);
							display_force_reboot_error(REC_SYSTEM_VALIDATION_FILE_ERROR);
						}

						ret = check_tcc_AES128(ENC_SYSTEM_UPDATE_DTB_PATH,"/navi/device_tree.dtb");
						if (ret != 0) {
							printf("[ERROR] device_tree.dtb - Descryption error : %d \r\n",ret);
							display_force_reboot_error(REC_SYSTEM_VALIDATION_FILE_ERROR);
						}

						ret = check_tcc_AES128(ENC_SYSTEM_UPDATE_SPLASH_PATH,"/navi/splash.img");
						if (ret != 0) {
							printf("[ERROR] splash.img - Descryption error : %d \r\n",ret);
							display_force_reboot_error(REC_SYSTEM_VALIDATION_FILE_ERROR);
						}

						ret = check_tcc_AES128(ENC_SYSTEM_UPDATE_RECOVERY_PATH,"/navi/recovery.img");
						if (ret != 0) {
							printf("[ERROR] recovery.img - Descryption error : %d \r\n",ret);
							display_force_reboot_error(REC_SYSTEM_VALIDATION_FILE_ERROR);
						}

						ret = check_tcc_AES128(ENC_SYSTEM_UPDATE_SYSTEM_PATH,"/navi/system.ext4");
						if (ret != 0) {
							printf("[ERROR] system.ext4 - Descryption error : %d \r\n",ret);
							display_force_reboot_error(REC_SYSTEM_VALIDATION_FILE_ERROR);
						}

						// TODO : need to hash checking.
						//----------------------------------------------
						// Check the validation
						//----------------------------------------------
						time(&module_time_sec);
						ret = check_validation_system_package(1);
						if (ret != 0) {
							display_force_reboot_error(REC_SYSTEM_VALIDATION_FILE_ERROR);
						}
						printf("validation time : %d \n",print_time(module_time_sec));

					} else {

						//----------------------------------------------
						// Check the validation
						//----------------------------------------------
						time(&module_time_sec);
						ret = check_validation_system_package(0);
						if (ret != 0) {
							display_force_reboot_error(REC_SYSTEM_VALIDATION_FILE_ERROR);
						}
						printf("validation time : %d \n",print_time(module_time_sec));
					}

					//----------------------------------------------
					// make update working file
					//----------------------------------------------
					remove(SYSTEM_UPDATE_WORKING);
					sync();
					fd = fopen(SYSTEM_UPDATE_WORKING, "a+");
					if (fd != 0) {
						printf("open error %s : %d \n",SYSTEM_UPDATE_WORKING,fd);
					}
					fclose(fd);
				} else {						// if (access(update_tar_path,F_OK) == 0) 
					display_force_reboot_error(REC_SYSTEM_UNTAR_ERROR);
				}
			} else {	// if (access(SYSTEM_UPDATE_WORKING,F_OK) != 0)
				printf("[DEBUG] %s is existing \r\n",SYSTEM_UPDATE_WORKING);
			}

			// --------------------------------------------------
			// VR1 Update 
			// --------------------------------------------------
			vr1_parsing_name = strchr(update_tar_pkg_file,'_');
			vr1_parsing_name = strchr(vr1_parsing_name + 1, '_');

			memset(update_vr1_pkg_file,0x00,sizeof(char)*256);
		#if defined(SDCARD_UPDATE)
			sprintf(update_vr1_pkg_file,"%s/%s%s",SDCARD_ROOT,VR1_PACKAGE_NAME,vr1_parsing_name);
		#else
			sprintf(update_vr1_pkg_file,"%s/%s%s",USB_ROOT,VR1_PACKAGE_NAME,vr1_parsing_name);
		#endif
			printf("check the vr1 package : %s \n",update_vr1_pkg_file);
			time(&module_time_sec);

			if (access(update_vr1_pkg_file,F_OK) == 0) {

				printf("check the vr1 update path\n");
				if (access(VR1_MAIN_PATH,F_OK) == 0) {
					printf("exist vr1 files\n");

					if (rmdirs(VR1_MAIN_PATH,1) == 0) {
						printf("[%s][%d]\n",__func__,__LINE__);
						sync();
					}

					printf("[%s][%d]\n",__func__,__LINE__);

					// TODO : delete this code, It support to delete the  old system files
					if (access("/vr1/LPTE",F_OK) == 0) {
						printf("[%s][%d]\n",__func__,__LINE__);
						if (rmdirs("/vr1/LPTE",1) == 0) {
							sync();
						}
					}
					// TODO : end
				}
				printf("[%s][%d]\n",__func__,__LINE__);
				untar_status = lib_untar_f(update_vr1_pkg_file, "/vr1");
				printf("untar_status is %d\n",untar_status);
				sync();

				// -----------------------------------------------------
				// cache restore
				// -----------------------------------------------------
				{
					char mbuf[256] = {0x00,};
					sprintf(mbuf, "%s", "echo 1 > /proc/sys/vm/drop_caches");
					system(mbuf);
				}
			}
			printf("VR1 time : %d \n",print_time(module_time_sec));
			// --------------------------------------------------


			// --------------------------------------------------
			// Quickboot Update 
			// --------------------------------------------------
			time(&module_time_sec);
			ret = update_quickboot_package(update_tar_pkg_file,device);
			if (ret != 0) {
				display_system_update_error(RecoveryUI::CUR_UPDATE_LABLE_TYPE_SYSTEM_ERROR,REC_SYSTEM_INSTALL_ERROR);
			}

			// -----------------------------------------------------
			// cache restore
			// -----------------------------------------------------
			{
				char mbuf[256] = {0x00,};
				sprintf(mbuf, "%s", "echo 1 > /proc/sys/vm/drop_caches");
				system(mbuf);
			}


			//----------------------------------------------
			// Update the System 
			//----------------------------------------------
			if ( (access(SYSTEM_UPDATE_LK_PATH,F_OK) ==0) &&  
					(access(SYSTEM_UPDATE_BOOT_PATH,F_OK) ==0) &&
					(access(SYSTEM_UPDATE_DTB_PATH,F_OK) ==0) &&
					(access(SYSTEM_UPDATE_SPLASH_PATH,F_OK) ==0) &&
					(access(SYSTEM_UPDATE_RECOVERY_PATH,F_OK) ==0) &&
					(access(SYSTEM_UPDATE_SYSTEM_PATH,F_OK) ==0) ) {

				time(&module_time_sec);

				ret = update_system_package(0);
				if (ret != 0) {
					display_system_update_error(RecoveryUI::CUR_UPDATE_LABLE_TYPE_SYSTEM_ERROR,REC_SYSTEM_INSTALL_ERROR);
				}

				if (rmdirs("/upgrade/system",1) != 0) {
					DeleteFile(SYSTEM_UPDATE_LK_PATH);
					DeleteFile(SYSTEM_UPDATE_BOOT_PATH);
					DeleteFile(SYSTEM_UPDATE_DTB_PATH);
					DeleteFile(SYSTEM_UPDATE_SPLASH_PATH);
					DeleteFile(SYSTEM_UPDATE_RECOVERY_PATH);
					DeleteFile(SYSTEM_UPDATE_SYSTEM_PATH);
					DeleteFolder("/upgrade/system");
					sync();
				}
				printf("system package update time : %d \n",print_time(module_time_sec));
			}

			//-------------------------------------------------------------
			// Update the System (decryption files)
			//-------------------------------------------------------------
			if ( (access("/navi/lk.rom",F_OK) ==0) &&  
					(access("/navi/boot.img",F_OK) ==0) &&
					(access("/navi/device_tree.dtb",F_OK) ==0) &&
					(access("/navi/splash.img",F_OK) ==0) &&
					(access("/navi/recovery.img",F_OK) ==0) &&
					(access("/navi/system.ext4",F_OK) ==0) ) {

				time(&module_time_sec);

				ret = update_system_package(1);
				if (ret != 0) {
					display_system_update_error(RecoveryUI::CUR_UPDATE_LABLE_TYPE_SYSTEM_ERROR,REC_SYSTEM_INSTALL_ERROR);
				}

				if (rmdirs("/upgrade/system",1) != 0) {
					DeleteFile(SYSTEM_UPDATE_LK_PATH);
					DeleteFile(SYSTEM_UPDATE_BOOT_PATH);
					DeleteFile(SYSTEM_UPDATE_DTB_PATH);
					DeleteFile(SYSTEM_UPDATE_SPLASH_PATH);
					DeleteFile(SYSTEM_UPDATE_RECOVERY_PATH);
					DeleteFile(SYSTEM_UPDATE_SYSTEM_PATH);
					DeleteFolder("/upgrade/system");
				}

				//DeleteFile("/navi/lk.rom");
				//DeleteFile("/navi/boot.img");
				//DeleteFile("/navi/device_tree.dtb");
				//DeleteFile("/navi/splash.img");
				//DeleteFile("/navi/recovery.img");
				//DeleteFile("/navi/system.ext4");
				sync();

				printf("system package update time : %d \n",print_time(module_time_sec));
			}

			// --------------------------------------------------
			//  restoring
			// --------------------------------------------------
			ret = locale_data_restore();
			if (ret != 0) {
				printf("data restore result  : %d \n",ret);
			}

			// --------------------------------------------------
			//  set permission
			// --------------------------------------------------
			setPermissionBackupDir("/log/backup_data");

			printf("Quickboot image update time : %d \n",print_time(module_time_sec));
			// --------------------------------------------------


			// --------------------------------------------------
			// Navi Map Update 
			// --------------------------------------------------
			time(&module_time_sec);

			ret = update_navi_map();
			if (ret != 0) {
				display_system_update_error(RecoveryUI::CUR_UPDATE_LABLE_TYPE_SYSTEM_ERROR,REC_SYSTEM_INSTALL_ERROR);
			}

			printf("Navi Map time : %d \n",print_time(module_time_sec));
			// --------------------------------------------------


			// --------------------------------------------------
			// VR2 Update 
			// --------------------------------------------------
			time(&module_time_sec);

			ret = update_vr2_db();
			if (ret != 0) {
				display_system_update_error(RecoveryUI::CUR_UPDATE_LABLE_TYPE_SYSTEM_ERROR,REC_SYSTEM_INSTALL_ERROR);
			}

			if (rmdirs("/upgrade/vr",1) != 0) {
				DeleteFile(SYSTEM_UPDATE_VR2_PATH);
			}
			sync();

			printf("VR2 time : %d \n",print_time(module_time_sec));
			// --------------------------------------------------

			ui->SetProgress(1);
		}
	}

#ifdef SUPPORT_OTA
	if ((access("/upgrade/ota/root_debug_delta.up",F_OK)==0) || (strcmp(update_package,"/upgrade/ota/root_debug_delta.up")== 0)) 
	{
		ui->countPresent++;
		ui->ui_set_status_message(RecoveryUI::INSTALLING_MESSAGE);
		ui->SetUpdateLableType(RecoveryUI::CUR_UPDATE_LABLE_TYPE_NORMAL);
		ui->SetProgressType(RecoveryUI::DETERMINATE);

		printf("Start uagent update\n");

		update_Start_C_Func();

		ret = ensure_path_mounted(SYSTEM_ROOT);
		if(ret != 0) {
			printf("[cjung] SYSTEM_ROOT mount failed, retry\n");
			ensure_path_mounted(SYSTEM_ROOT);
		}

		ret = ensure_path_mounted(VR1_ROOT);
		if(ret != 0) {
			printf("[cjung] VR1_ROOT mount failed, retry\n");
			ensure_path_mounted(VR1_ROOT);
		}

		// check package file
		ret = 0;
		//        if(0 == access(update_package, F_OK)) {
		if(0 == access("/upgrade/ota/root_debug_delta.up", F_OK)) {
			ret = __process_redband_update_agent();
		} else {
			printf("[%s] don't access [%s]\n", update_package);
			ret = -1;
		}

		//update package remove
		if(ret == 0) {
			//            DeleteFile(update_package);
			DeleteFile("/upgrade/ota/root_debug_delta.up");
		} else {
			ui->SetProgress(1);

			printf("[%d] rb ua udpate failed, ret : %d\n", __LINE__, ret);
			display_error_and_copy_log(REC_SYSTEM_INSTALL_ERROR,
					RecoveryUI::CUR_UPDATE_LABLE_TYPE_SYSTEM_ERROR, UPDATE_COUNT_SYSTEM);
			// update failed - don't return
		}

		//update bootloader
		if(0 == access("/upgrade/ota/lk.rom", F_OK)) {
			ret = update_bootloader_function("/upgrade/ota/lk.rom");
		} else {
			printf("[%d] don't access /upgrade/ota/lk.rom\n", __LINE__);
		}

		ui->SetProgress(1);

		if(ret != 0) {
			printf("[%d] rb ua udpate failed, ret : %d\n", __LINE__, ret);

			display_error_and_copy_log(REC_SYSTEM_INSTALL_ERROR,
					RecoveryUI::CUR_UPDATE_LABLE_TYPE_SYSTEM_ERROR, UPDATE_COUNT_SYSTEM);
			// update failed - don't return
		}

		erase_volume("/cache");
		erase_volume("/data");
		erase_volume("/snapshot");
		DeleteFile("/upgrade/ota/lk.rom");

		ensure_path_unmounted(SYSTEM_ROOT);
		ensure_path_unmounted(VR1_ROOT);
	}
#endif // SUPPORT_OTA

#if defined(TRIMBLE_GPS_SUPPORT)
	ret = update_trimble_gps_module();
	if (ret < 0) {
		ui->ShowSpinProgress(false);
		if(UpdateCountFunc(UPDATE_COUNT_GPS) != -1) {
			copy_log_file(TEMPORARY_LOG_FILE, LAST_LOG_FILE, false);
			copy_log_file(TEMPORARY_LOG_FILE,LAST_LOG_FILE_TO_STORAGE, false);

			ui->SetUpdateLableType(RecoveryUI::CUR_UPDATE_LABLE_TYPE_GPS_ERROR);
			ui->ui_set_status_message(RecoveryUI::INSATLL_ERROR_MESSAGE);
			ui->ui_set_error_number(REC_GPS_ERROR);

			sleep(10);
			hardReset();
			sleep(5);
		} else {
			copy_log_file(TEMPORARY_LOG_FILE, LAST_LOG_FILE, false);
			copy_log_file(TEMPORARY_LOG_FILE,LAST_LOG_FILE_TO_STORAGE, false);

			ui->SetUpdateLableType(RecoveryUI::CUR_UPDATE_LABLE_TYPE_CONTACT_CUSTOMER);
			ui->ui_set_status_message(RecoveryUI::INSTALL_ERROR_CONTACN_CUSTOMER);
			ui->ui_set_error_number(REC_GPS_ERROR);

			sleep(10);
			request_to_enter_sleep_mode();
			while (1) {}
		}
	}
#endif


#if defined(LGIT9x28_MODEM_SUPPORT)
	ret = update_lg9x28modem_module();
	if (ret == -1) {
		ui->ShowSpinProgress(false);
		if(UpdateCountFunc(UPDATE_COUNT_MODEM) != -1) {
			copy_log_file(TEMPORARY_LOG_FILE, LAST_LOG_FILE, false);
			copy_log_file(TEMPORARY_LOG_FILE,LAST_LOG_FILE_TO_STORAGE, false);

			ui->SetUpdateLableType(RecoveryUI::CUR_UPDATE_LABLE_TYPE_MODEM_ERROR);
			ui->ui_set_status_message(RecoveryUI::INSATLL_ERROR_MESSAGE);
			ui->ui_set_error_number(REC_MODEM_ERROR);

			sleep(10);
			hardReset();
			sleep(5);
		} else {
			copy_log_file(TEMPORARY_LOG_FILE, LAST_LOG_FILE, false);
			copy_log_file(TEMPORARY_LOG_FILE,LAST_LOG_FILE_TO_STORAGE, false);

			ui->SetUpdateLableType(RecoveryUI::CUR_UPDATE_LABLE_TYPE_CONTACT_CUSTOMER);
			ui->ui_set_status_message(RecoveryUI::INSTALL_ERROR_CONTACN_CUSTOMER);
			ui->ui_set_error_number(REC_MODEM_ERROR);

			sleep(10);
			request_to_enter_sleep_mode();
			while (1) {}
		}
	}
#endif

	// Micom update func start
	ret = uComUpdateFunc();
	if(ret != MICOM_STATUS_OK) {
		copy_log_file(TEMPORARY_LOG_FILE, LAST_LOG_FILE, false);
		copy_log_file(TEMPORARY_LOG_FILE,LAST_LOG_FILE_TO_STORAGE, false);
		sleep(5);

		ui->ShowSpinProgress(false);
		if(ret == MICOM_STATUS_ERROR_MICOM) {
			if(UpdateCountFunc(UPDATE_COUNT_MICOM) != -1) {
				ui->SetUpdateLableType(RecoveryUI::CUR_UPDATE_LABLE_TYPE_MICOM_ERROR);
				ui->ui_set_status_message(RecoveryUI::INSATLL_ERROR_MESSAGE);
				ui->ui_set_error_number(REC_MICOM_ERROR);

				sleep(10);
				android_reboot(ANDROID_RB_RESTART2, 0, "recovery");
				while (1) { }
			} else {
				ui->SetUpdateLableType(RecoveryUI::CUR_UPDATE_LABLE_TYPE_CONTACT_CUSTOMER);
				ui->ui_set_status_message(RecoveryUI::INSTALL_ERROR_CONTACN_CUSTOMER);
				ui->ui_set_error_number(REC_MICOM_ERROR);

				sleep(10);
				request_to_enter_sleep_mode();
				while (1) { }
			}
		} else if(UpdateCountFunc(UPDATE_COUNT_HD) != -1) {
			ui->SetUpdateLableType(RecoveryUI::CUR_UPDATE_LABLE_TYPE_MICOM_ERROR);
			ui->ui_set_status_message(RecoveryUI::INSATLL_ERROR_MESSAGE);
			ui->ui_set_error_number(REC_MICOM_HD_ERROR);

			sleep(10);
			hardReset();
			sleep(5);
			android_reboot(ANDROID_RB_RESTART2, 0, "recovery");
			while (1) { }
		} else {
			ui->SetUpdateLableType(RecoveryUI::CUR_UPDATE_LABLE_TYPE_CONTACT_CUSTOMER);
			ui->ui_set_status_message(RecoveryUI::INSTALL_ERROR_CONTACN_CUSTOMER);
			ui->ui_set_error_number(REC_MICOM_ERROR);

			sleep(10);
			request_to_enter_sleep_mode();
			while (1) { }
		}
	}

exit:
	ui->ShowSpinProgress(false);
	ui->SetProgress(0);
	printf("Update end\n");
	printf("Navi Map time : %d \n",print_time(start_time_sec));

	if (0) {
		set_security_key_to_otp();
	}

	ui->ui_set_status_message(RecoveryUI::INSTALL_SUCCESS_MESSAGE);

	remove(SYSTEM_UPDATE_WORKING);
	remove(SYSTEM_PACKAGE_INFO_PATH);
	remove(SYSTEM_PACKAGE_SEC_INFO_PATH);
	remove(LOCALE_FILE);
	remove(FOREGROUND_UPDATE_FLAG);
	remove(NAVI_FOREGROUND_UPDATE_WORKING);
	remove(NAVI_MAP_VERSION_INFO);
	remove(QUCIKBOOT_IMAGE_PATH);
	remove(QUCIKBOOT_QBDATA_PATH);

	// Otherwise, get ready to boot the main system...
	finish_recovery(send_intent, update_package);

	/* get ready to boot the main */
	sync();

	ensure_path_unmounted(UPDATE_ROOT);
	ensure_path_unmounted(USERDATA_ROOT);
	ensure_path_unmounted(NAVI_ROOT);
	ensure_path_unmounted(NAVI_MIRROR_ROOT);
	ensure_path_unmounted(VR1_ROOT);
	ensure_path_unmounted(VR2_ROOT);
	ensure_path_unmounted(USB_ROOT);
	ensure_path_unmounted(SDCARD_ROOT);
	ensure_path_unmounted(OEM_DATA_ROOT);
	ensure_path_unmounted(STORAGE_LOG_ROOT);

	sleep(10);

	update_Finish_C_Func();

	sleep(5);

	android_reboot(ANDROID_RB_RESTART2, 0, (char *)"force_normal");

	while (1) { }

	return EXIT_SUCCESS;
}
