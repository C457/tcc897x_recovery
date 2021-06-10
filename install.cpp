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
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dirent.h>

#include "common.h"
#include "install.h"
#include "mincrypt/rsa.h"
#include "minui/minui.h"
#include "minzip/SysUtil.h"
#include "minzip/Zip.h"
#include "mtdutils/mounts.h"
#include "mtdutils/mtdutils.h"
#include "roots.h"
#include "verifier.h"
#include "ui.h"

extern RecoveryUI* ui;

#define MAX_BUFF_SIZE               4096
#define BLK_DEV_NAME_PATH           "/dev/block/platform/bdm/by-name"

#define ASSUMED_UPDATE_BINARY_NAME  "META-INF/com/google/android/update-binary"
#define PUBLIC_KEYS_FILE "/res/keys"

// Default allocation of progress bar segments to operations
static const int VERIFICATION_PROGRESS_TIME = 60;
static const float VERIFICATION_PROGRESS_FRACTION = 0.25;
static const float DEFAULT_FILES_PROGRESS_FRACTION = 0.4;
static const float DEFAULT_IMAGE_PROGRESS_FRACTION = 0.1;

#define LOCALE_DATA_LOAD_PATH           "/log/backup_data"
#define LOCALE_DATA_SAVE_PATH           "/data/property"

#define COPY_BUFFER_SIZE                4096
#define PATH_MAX_LENGTH                 256

int copy_localedata_to_datapart(const char* source_file, const char* dest_file) {
    FILE *source = NULL;
    FILE *dest = NULL;
    int size=0;
    int ret = 0;
    int org_len, new_len = 0;
    unsigned char buffer[COPY_BUFFER_SIZE] = {};

    memset(buffer, '\0', sizeof(buffer));

    dest = fopen(dest_file, "w");
    if(dest == NULL) {
        LOGE("Can't create %s\n", dest_file);
        return -1;
    }

    source = fopen(source_file, "rb");
    if (source == NULL) {
        LOGE("Can't open %s\n", source_file);
        return -1;
    }

    while ((size = fread(buffer, 1, COPY_BUFFER_SIZE, source)) != 0)
    {
        if(ferror(source)) {
            LOGE("read error (%s)\n",source_file);
            return -1;
        }

        int r = fwrite(buffer, 1, size, dest);
        if (r < 0) {
            LOGE(" output file write failed\n");
            return -1;
        }

        memset(buffer, '\0', sizeof(buffer));
    }

    fclose(source);
    fclose(dest);
    sync();

    return 0;
}

int locale_data_restore() {
    int result = 0;

    char src_path[PATH_MAX_LENGTH];
    char dst_path[PATH_MAX_LENGTH];

    DIR *dir;
    struct dirent *ent;

    dir = opendir(LOCALE_DATA_LOAD_PATH);
    if(dir == NULL) {
        fprintf(stdout, "directory(%s) open failed\n", LOCALE_DATA_LOAD_PATH);
        return -1;
    }

    mkdir(LOCALE_DATA_SAVE_PATH,0700);

    while((ent = readdir(dir)) != NULL)
    {
        // pass "." & ".."
        if(strcmp(ent->d_name, ".") == 0)
            continue;

        if(strcmp(ent->d_name, "..") == 0)
            continue;

        // pass not property file
        if(strncmp(ent->d_name, "persist.", 8) != 0)
            continue;

        memset(src_path, 0, sizeof(src_path));
        memset(dst_path, 0, sizeof(dst_path));

        // set path src & dst
        sprintf(src_path, "%s/%s", LOCALE_DATA_LOAD_PATH, ent->d_name);
        sprintf(dst_path, "%s/%s", LOCALE_DATA_SAVE_PATH, ent->d_name);

        // copy backup file
        result = copy_localedata_to_datapart(src_path, dst_path);
        if(result < 0) {
            fprintf(stdout, "%s file copy failed\n", dst_path);
            return -1;
        }
    }

    return result;
}


// If the package contains an update binary, extract it and run it.
static int
try_update_binary(const char *path, ZipArchive *zip, int* wipe_cache) {
    const ZipEntry* binary_entry =
            mzFindZipEntry(zip, ASSUMED_UPDATE_BINARY_NAME);
    if (binary_entry == NULL) {
        mzCloseZipArchive(zip);
        return INSTALL_CORRUPT;
    }

    const char* binary = "/tmp/update_binary";
    unlink(binary);
    int fd = creat(binary, 0755);
    if (fd < 0) {
        mzCloseZipArchive(zip);
        LOGE("Can't make %s\n", binary);
        return INSTALL_ERROR;
    }
    bool ok = mzExtractZipEntryToFile(zip, binary_entry, fd);
    close(fd);
    mzCloseZipArchive(zip);

    if (!ok) {
        LOGE("Can't copy %s\n", ASSUMED_UPDATE_BINARY_NAME);
        return INSTALL_ERROR;
    }

    int pipefd[2];
    pipe(pipefd);

    // When executing the update binary contained in the package, the
    // arguments passed are:
    //
    //   - the version number for this interface
    //
    //   - an fd to which the program can write in order to update the
    //     progress bar.  The program can write single-line commands:
    //
    //        progress <frac> <secs>
    //            fill up the next <frac> part of of the progress bar
    //            over <secs> seconds.  If <secs> is zero, use
    //            set_progress commands to manually control the
    //            progress of this segment of the bar
    //
    //        set_progress <frac>
    //            <frac> should be between 0.0 and 1.0; sets the
    //            progress bar within the segment defined by the most
    //            recent progress command.
    //
    //        firmware <"hboot"|"radio"> <filename>
    //            arrange to install the contents of <filename> in the
    //            given partition on reboot.
    //
    //            (API v2: <filename> may start with "PACKAGE:" to
    //            indicate taking a file from the OTA package.)
    //
    //            (API v3: this command no longer exists.)
    //
    //        ui_print <string>
    //            display <string> on the screen.
    //
    //   - the name of the package zip file.
    //

    const char** args = (const char**)malloc(sizeof(char*) * 5);
    args[0] = binary;
    args[1] = EXPAND(RECOVERY_API_VERSION);   // defined in Android.mk
    char* temp = (char*)malloc(10);
    sprintf(temp, "%d", pipefd[1]);
    args[2] = temp;
    args[3] = (char*)path;
    args[4] = NULL;

    pid_t pid = fork();
    if (pid == 0) {
        close(pipefd[0]);
        execv(binary, (char* const*)args);
        fprintf(stdout, "E:Can't run %s (%s)\n", binary, strerror(errno));
        _exit(-1);
    }
    close(pipefd[1]);

    *wipe_cache = 0;

    char buffer[1024];
    FILE* from_child = fdopen(pipefd[0], "r");
    while (fgets(buffer, sizeof(buffer), from_child) != NULL) {
        char* command = strtok(buffer, " \n");
        if (command == NULL) {
            continue;
        } else if (strcmp(command, "progress") == 0) {
            char* fraction_s = strtok(NULL, " \n");
            char* seconds_s = strtok(NULL, " \n");

            float fraction = strtof(fraction_s, NULL);
            int seconds = strtol(seconds_s, NULL, 10);

            ui->ShowProgress(fraction * (1-VERIFICATION_PROGRESS_FRACTION), seconds);
        } else if (strcmp(command, "set_progress") == 0) {
            char* fraction_s = strtok(NULL, " \n");
            float fraction = strtof(fraction_s, NULL);
            ui->SetProgress(fraction);
        } else if (strcmp(command, "ui_print") == 0) {
            char* str = strtok(NULL, "\n");
            if (str) {
                ui->Print("%s", str);
            } else {
                ui->Print("\n");
            }
            fflush(stdout);
        } else if (strcmp(command, "wipe_cache") == 0) {
            *wipe_cache = 1;
        } else if (strcmp(command, "clear_display") == 0) {
            ui->SetBackground(RecoveryUI::NONE);
        } else {
            LOGE("unknown command [%s]\n", command);
        }
    }
    fclose(from_child);

    int status;
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        LOGE("Error in %s\n(Status %d)\n", path, WEXITSTATUS(status));
        return INSTALL_ERROR;
    }

    return INSTALL_SUCCESS;
}

int daudio_partition_write(const char *inputfile, const char *outputfile)
{
    int ret = 0;
    int readCnt, writeCnt;
    FILE *fin = NULL, *fout = NULL;
    unsigned char buffer[MAX_BUFF_SIZE];

    fprintf(stdout, "daudio_partition_write function entered\n");

    fin = fopen(inputfile, "rb");
    if(fin == NULL) {
        fprintf(stderr, "input file(%s) open failed\n", inputfile);
        return -1;
    }

    fout = fopen(outputfile, "wb");
    if(fout == NULL) {
        fprintf(stdout, "output file(%s) open failed\n", outputfile);
        goto part_write_error_in;
    }

    ret = fseek(fin, 0L, SEEK_SET);
    if(ret < 0) {
        fprintf(stderr, "input file seek failed\n");
        goto part_write_error;
    }

    ret = fseek(fout, 0L, SEEK_SET);
    if(ret < 0) {
        fprintf(stderr, "output file seek failed\n");
        goto part_write_error;
    }

    fprintf(stdout, "---------------%s Device Write START-----------------\n", outputfile);
    fprintf(stdout, "start current outputfile pointer = %u\n", (unsigned)ftell(fout));
    while(!feof(fin)) {
        readCnt = fread(buffer, 1, MAX_BUFF_SIZE, fin);
        if(ferror(fin)) {
            fprintf(stderr, "input file read error (%s)\n",inputfile);
            goto part_write_error;
        }

        if(readCnt < 0) {
            fprintf(stderr, "input file failed\n");
            goto part_write_error;
        }

        if(readCnt != 0) {
            writeCnt = fwrite(buffer, 1, readCnt, fout);
            if(writeCnt < 0) {
                fprintf(stderr, " output file write failed\n");
                goto part_write_error;
            }
        }
    }
    fprintf(stdout, "---------------%s Device Write END-----------------\n", outputfile);
    fprintf(stdout, "end current outputfile pointer = %lu \n", ftell(fout));

    if(NULL != fin)
        fclose(fin);
    if(NULL != fout)
        fclose(fout);

    sync();

    return 0;

part_write_error:
    if(NULL != fout)
        fclose(fout);
part_write_error_in:
    if(NULL != fin)
        fclose(fin);

    return -1;
}

int daudio_write(const char *partition, const char *filename)
{
    int ret = 0;
    char partitionByName[128];

    DIR *dir;
    struct dirent *ent;

    fprintf(stdout, "daudio_write function entered\n");
    fprintf(stdout, "partition : %s filename : %s\n", partition, filename);

    dir = opendir(BLK_DEV_NAME_PATH);
    if(dir == NULL) {
        fprintf(stderr, "block device directory open failed\n");
        return -1;
    }
    fprintf(stderr, "opendir success [%s]\n", BLK_DEV_NAME_PATH);

    while((ent = readdir(dir)) != NULL) {
        // find partition in the "by-name" path.
        if(!strcmp(partition, ent->d_name)) {
            sprintf(partitionByName, "%s/%s", BLK_DEV_NAME_PATH, partition);
            fprintf(stdout, "block device by name path  : %s\n", partitionByName);

            ret = daudio_partition_write(filename, partitionByName);

            if(0 > ret) {
                fprintf(stderr, "%s write failed\n", partition);
                goto error;
            }
            fprintf(stderr, "%s write success\n", partition);
            closedir(dir);
            return 0;
        }
    }
    fprintf(stderr, "partition check failed\n");

error:
    if(NULL != dir)
        closedir(dir);

    return -1;
}

int update_voice_recognizer(const char *path)
{
    int ret = 0;

    ui->Print("VR update Checking\n");
    if(access(path, F_OK) != 0) {
        ui->Print("Not exist VR file!!\n");
        fprintf(stderr, "not found %s file\n", path);
        return -1;
    }

    ui->Print("Start VR update\n");
    ret = format_volume("/vr");
    if(ret < 0) {
        fprintf(stderr, "\"/vr\" partition format failed\n");
        return -1;
    }

    ret = daudio_write("vr", path);
    if(ret < 0) {
        fprintf(stderr, "VR image update failed\n");
        return -1;
    }
    ui->Print("VR image Finished update\n");

    return 0;
}

static int
really_install_package(const char *path, int* wipe_cache)
{
    ui->Print("Finding update package...\n");
    // Give verification half the progress bar...
    LOGI("Update location: %s\n", path);

    if (ensure_path_mounted(path) != 0) {
        LOGE("Can't mount %s\n", path);
        return INSTALL_CORRUPT;
    }

    ui->Print("Opening update package...\n");

    int numKeys;
    Certificate* loadedKeys = load_keys(PUBLIC_KEYS_FILE, &numKeys);
    if (loadedKeys == NULL) {
        LOGE("Failed to load keys\n");
        return INSTALL_CORRUPT;
    }
    LOGI("%d key(s) loaded from %s\n", numKeys, PUBLIC_KEYS_FILE);

    ui->Print("Verifying update package...\n");

    int err;
    err = verify_file(path, loadedKeys, numKeys);
    free(loadedKeys);
    LOGI("verify_file returned %d\n", err);
    if (err != VERIFY_SUCCESS) {
        LOGE("signature verification failed\n");
        return INSTALL_CORRUPT;
    }

    /* Try to open the package.
     */
    ZipArchive zip;
    err = mzOpenZipArchive(path, &zip);
    if (err != 0) {
        LOGE("Can't open %s\n(%s)\n", path, err != -1 ? strerror(err) : "bad");
        return INSTALL_CORRUPT;
    }

    /* Verify and install the contents of the package.
     */
    ui->Print("Installing update...\n");
    return try_update_binary(path, &zip, wipe_cache);
}

int
install_package(const char* path, int* wipe_cache, const char* install_file)
{
    FILE* install_log = fopen_path(install_file, "w");
    if (install_log) {
        fputs(path, install_log);
        fputc('\n', install_log);
    } else {
        LOGE("failed to open last_install: %s\n", strerror(errno));
    }
    int result;
    if (setup_install_mounts() != 0) {
        LOGE("failed to set up expected mounts for install; aborting\n");
        result = INSTALL_ERROR;
    } else {
        result = really_install_package(path, wipe_cache);
    }
    if (install_log) {
        fputc(result == INSTALL_SUCCESS ? '1' : '0', install_log);
        fputc('\n', install_log);
        fclose(install_log);
    }
    return result;
}
