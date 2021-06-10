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

#include <errno.h>
#include <stdlib.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <ctype.h>

#include <fs_mgr.h>
#include "mtdutils/mtdutils.h"
#include "mtdutils/mounts.h"
#include "roots.h"
#include "common.h"
#include "make_ext4fs.h"
//#include "ui.h"
#include "cutils/properties.h"
#include <fcntl.h>

//#include <platform.h>

#define false 0
#define true 1


#define PARTITION_CHECK_CNT     17

//extern RecoveryUI* ui;

static struct fstab *m_fstab = NULL;

//extern struct selabel_handle *sehandle;
struct selabel_handle *sehandle;
unsigned char boot_mode_is_sdmmc = false;
extern unsigned boot_active_partition;  // 0 : boot normal partition, 1 : boot mirror partition

static char fstab_mirror[32];

static void import_kernel_nv(char *name, int in_qemu)
{
	if (*name != '\0') {
		char *value = strchr(name, '=');
		if (value != NULL) {
			*value++ = 0;
			if (!strcmp(name,"active_partition"))
			{
				strlcpy(fstab_mirror, value, sizeof(fstab_mirror));
			}
		}
	}
}

void import_kernel_cmdline(int in_qemu, void (*import_kernel_nv)(char *name, int in_qemu))
{
	char cmdline[1024];
	char *ptr;
	int fd;

	fd = open("/proc/cmdline", O_RDONLY);
	if (fd >= 0) {
		int n = read(fd, cmdline, 1023);
		if (n < 0) n = 0;

		/* get rid of trailing newline, it happens */
		if (n > 0 && cmdline[n-1] == '\n') n--;

		cmdline[n] = 0;
		close(fd);
	} else {
		cmdline[0] = 0;
	}

	ptr = cmdline;
	while (ptr && *ptr) {
		char *x = strchr(ptr, ' ');
		if (x != 0) *x++ = 0;
		import_kernel_nv(ptr, in_qemu);
		ptr = x;
	}
}


void load_update_volume_table(void)
{
    int i;
    int ret;
    
    char fstab_name[512];
    
    memset(fstab_name, 0x0, 512);

    if(boot_active_partition == 0)
      sprintf(fstab_name, "%s", "/etc/recovery.fstab_mirror" );
    else
      sprintf(fstab_name, "%s", "/etc/recovery.fstab" );

    printf("==> fstab_name = %s\n", fstab_name);
      
    m_fstab = fs_mgr_read_fstab(fstab_name);
    if (!m_fstab) {
      printf("failed to read /etc/recovery.fstab\n");
      return;
    }

    ret = fs_mgr_add_entry(m_fstab, "/tmp", "ramdisk", "ramdisk", 0);
    if (ret < 0) {
      printf("failed to add /tmp entry to fstab\n");
      fs_mgr_free_fstab(m_fstab);
      m_fstab = NULL;
      return;
    }

    printf("recovery filesystem table\n");
    printf("=========================\n");
    for (i=0; i<m_fstab->num_entries; ++i) {
      Volume* v = &m_fstab->recs[i];
      printf("  %d %s %s %s %lld\n", i, v->mount_point, v->fs_type,
	  v->blk_device, v->length);
    }
    printf("\n");
}


#include <dirent.h>
extern const char *SDCARD_ROOT;
int update_mmcblk_dev_name(Volume* v)
{
	DIR *dir_info = NULL;
	struct dirent *dir_entry;
	char temp[4096];

	dir_info = opendir(v->blk_device);
	if (dir_info != NULL)
	{
		while ((dir_entry = readdir(dir_info)) != NULL)  {
			printf("%s : dir->entry(%s)\n", __func__, dir_entry->d_name);
			if (!strncmp(dir_entry->d_name, "mmcblk", 6)
					&& strlen(dir_entry->d_name) == 7 ) {
				memset(temp, 0x0, sizeof(temp));
				sprintf(temp, "%s/%s", v->blk_device, dir_entry->d_name);
				v->blk_device2[0] = strdup(temp);
				printf("%s : v->blk_device(%s)\n", __func__, v->blk_device);
				printf("%s : v->blk_device2[0](%s)\n", __func__, v->blk_device2[0]);
				printf("%s : v->mount_point(%s)\n", __func__, v->mount_point);
				printf("%s : v->fs_type(%s)\n", __func__, v->fs_type);
			} 
            else if (!strncmp(dir_entry->d_name, "mmcblk", 6)
					&& strlen(dir_entry->d_name) > 7 ) {
				memset(temp, 0x0, sizeof(temp));
				sprintf(temp, "%s/%s", v->blk_device, dir_entry->d_name);
				v->blk_device2[1] = strdup(temp);
				printf("%s : v->blk_device(%s)\n", __func__, v->blk_device);
				printf("%s : v->blk_device2[1](%s)\n", __func__, v->blk_device2[1]);
				printf("%s : v->mount_point(%s)\n", __func__, v->mount_point);
				printf("%s : v->fs_type(%s)\n", __func__, v->fs_type);
			}
		}
	}

	return 0;
}

Volume* volume_for_path(const char* path) {
    return fs_mgr_get_entry_for_mount_point(m_fstab, path);
}

int ensure_path_mounted(const char* path) {
  int loop;
    Volume* v = volume_for_path(path);
    if(v == NULL) {
        printf("unknown volume for path [%s]\n", path);
        return -1;
    }

    printf("[%s] : v->mount_point [%s], v->fs_type [%s]\n", __func__, v->mount_point, v->fs_type);
    if(strcmp(v->fs_type, "ramdisk") == 0) {
        // the ramdisk is always mounted.
        printf("[%s] : fs_type = ramdisk, return 0\n", __func__);
        return 0;
    }

    int result;
    result = scan_mounted_volumes();
    if(result < 0) {
        printf("failed to scan mounted volumes\n");
        return -1;
    }

    const MountedVolume* mv =
        find_mounted_volume_by_mount_point(v->mount_point);
    if (mv) {
        // volume is already mounted
        printf("[%s] : volume is already mounted\n", __func__);
        return 0;
    }
  	if(!strcmp(v->mount_point, "/sdcard"))
		update_mmcblk_dev_name(v);

    printf("[%s] : mkdir [%s]\n", __func__, v->mount_point);
    mkdir(v->mount_point, 0755);  // in case it doesn't already exist

    if(strcmp(v->fs_type, "yaffs2") == 0) {
        printf("[%s] : fs_type = yaffs2\n", __func__);
        // mount an MTD partition as a YAFFS2 filesystem.
        mtd_scan_partitions();
        const MtdPartition* partition;
        partition = mtd_find_partition_by_name(v->blk_device);
        if (partition == NULL) {
            printf("failed to find \"%s\" partition to mount at \"%s\"\n",
                 v->blk_device, v->mount_point);
            return -1;
        }
        return mtd_mount_partition(partition, v->mount_point, v->fs_type, 0);
    } else if(strcmp(v->fs_type, "ext4") == 0 ||
               strcmp(v->fs_type, "vfat") == 0) {
        printf("[%s] : fs_type = ext4 or vfat\n", __func__);

        result = mount(v->blk_device, v->mount_point, v->fs_type,
                       MS_NOATIME | MS_NODEV | MS_NODIRATIME, "");
        if(result && errno == EROFS) {
          result = mount(v->blk_device, v->mount_point, v->fs_type,
              MS_NOATIME | MS_NODEV | MS_NODIRATIME | MS_RDONLY, "");
        }
        if(result == 0)
            return 0;


        result = mount(v->blk_device, v->mount_point, "ufsd",
                       MS_NOATIME | MS_NODEV | MS_NODIRATIME, "force");
        if(result && errno == EROFS) {
          result = mount(v->blk_device, v->mount_point, "ufsd",
              MS_NOATIME | MS_NODEV | MS_NODIRATIME | MS_RDONLY, "force");
        }
        if(result == 0)
            return 0;

        if(v->blk_device2[0]) {
            printf("failed to mount %s (%s); trying v->blk_device2[0] (%s)\n",
                 v->blk_device, strerror(errno), v->blk_device2[0]);
            result = mount(v->blk_device2[0], v->mount_point, v->fs_type,
                           MS_NOATIME | MS_NODEV | MS_NODIRATIME, "");
            if(result && errno == EROFS) {
              result = mount(v->blk_device2[0], v->mount_point, v->fs_type,
              MS_NOATIME | MS_NODEV | MS_NODIRATIME | MS_RDONLY, "");
            }
            if(result == 0)
                return 0;

            result = mount(v->blk_device2[0], v->mount_point, "ufsd",
                               MS_NOATIME | MS_NODEV | MS_NODIRATIME, "");
            if(result && errno == EROFS) {
              result = mount(v->blk_device2[0], v->mount_point, "ufsd",
              MS_NOATIME | MS_NODEV | MS_NODIRATIME | MS_RDONLY, "");
            }
            if(result == 0)
                return 0;
	    }

        if(v->blk_device2[1]) {
            printf("failed to mount %s (%s); trying v->blk_device2[1] (%s)\n",
                 v->blk_device, strerror(errno), v->blk_device2[1]);
            result = mount(v->blk_device2[1], v->mount_point, v->fs_type,
                           MS_NOATIME | MS_NODEV | MS_NODIRATIME, "");
            if(result && errno == EROFS) {
              result = mount(v->blk_device2[1], v->mount_point, v->fs_type,
              MS_NOATIME | MS_NODEV | MS_NODIRATIME | MS_RDONLY, "");
            }
            if(result == 0)
                return 0;

            result = mount(v->blk_device2[1], v->mount_point, "ufsd",
                               MS_NOATIME | MS_NODEV | MS_NODIRATIME, "");
            if(result && errno == EROFS) {
              result = mount(v->blk_device2[1], v->mount_point, "ufsd",
              MS_NOATIME | MS_NODEV | MS_NODIRATIME | MS_RDONLY, "");
            }
            if(result == 0)
                return 0;
        }

        printf("failed to mount %s (%s)\n", v->mount_point, strerror(errno));
        return -1;
    } else if (strcmp(v->fs_type, "auto") == 0) {
        int wait_time = 15;
        printf("[%s] : fs_type = auto\n", __func__);

        while(fopen(v->blk_device, "rb") == NULL && wait_time > 0) {
            printf("Waiting for attaching device..%ds\n", wait_time);
            sleep(1);
            wait_time--;
        }

        result = mount(v->blk_device, v->mount_point, "vfat",
          MS_NOATIME | MS_NODEV | MS_NODIRATIME, "");
        if(result && errno == EROFS) {
            result = mount(v->blk_device, v->mount_point, "vfat",
                    MS_NOATIME | MS_NODEV | MS_NODIRATIME | MS_RDONLY, "");
        }

        if(result == 0)
            return 0;

        result = mount(v->blk_device, v->mount_point, "texfat",
                       MS_NOATIME | MS_NODEV | MS_NODIRATIME, "");
        if(result && errno == EROFS) {
            result = mount(v->blk_device, v->mount_point, "texfat",
                           MS_NOATIME | MS_NODEV | MS_NODIRATIME | MS_RDONLY, "");
        }

        if(result == 0)
            return 0;

        result = mount(v->blk_device, v->mount_point, "tntfs",
                       MS_NOATIME | MS_NODEV | MS_NODIRATIME, "");
        if(result && errno == EROFS) {
            result = mount(v->blk_device, v->mount_point, "tntfs",
                           MS_NOATIME | MS_NODEV | MS_NODIRATIME | MS_RDONLY, "");
        }
        if(result == 0)
            return 0;

        result = mount(v->blk_device, v->mount_point, "ufsd",
          MS_NOATIME | MS_NODEV | MS_NODIRATIME, "force");
        if(result && errno == EROFS) {
            result = mount(v->blk_device, v->mount_point, "ufsd",
                    MS_NOATIME | MS_NODEV | MS_NODIRATIME | MS_RDONLY, "force");
        }
        if(result == 0)
            return 0;

        if(v->blk_device2[0]) {
            printf("failed to mount %s (%s); trying v->blk_device2[0] (%s)\n",
                 v->blk_device, strerror(errno), v->blk_device2[0]);

            char mount_str[PATH_MAX];

            // check multi-partition
            for(loop = 0; loop < PARTITION_CHECK_CNT; loop++) {
                memset(mount_str, 0, PATH_MAX);

                if(strncmp(v->blk_device2[0], "mmcblk", 6) == 0) {
                    sprintf(mount_str, "%sp%d", v->blk_device, loop);
                } else {
                    sprintf(mount_str, "%s%d", v->blk_device, loop);
                }

                printf("try mount [%s]\n", mount_str);

                result = mount(mount_str, v->mount_point, "vfat",
                               MS_NOATIME | MS_NODEV | MS_NODIRATIME, "");
                if(result && errno == EROFS) {
                    result = mount(mount_str, v->mount_point, v->fs_type,
                                   MS_NOATIME | MS_NODEV | MS_NODIRATIME | MS_RDONLY, "");
                }

                if(result == 0)
                    return 0;

                result = mount(mount_str, v->mount_point, "texfat",
                               MS_NOATIME | MS_NODEV | MS_NODIRATIME, "");
                if(result && errno == EROFS) {
                    result = mount(v->blk_device2[0], v->mount_point, v->fs_type,
                                   MS_NOATIME | MS_NODEV | MS_NODIRATIME | MS_RDONLY, "");
                }

                if(result == 0)
                    return 0;

                result = mount(mount_str, v->mount_point, "tntfs",
                               MS_NOATIME | MS_NODEV | MS_NODIRATIME, NULL);
                if(result && errno == EROFS) {
                    result = mount(v->blk_device2[0], v->mount_point, "ufsd",
                                   MS_NOATIME | MS_NODEV | MS_NODIRATIME | MS_RDONLY, "force");
                }

                if(result == 0)
                    return 0;
            }
        }

        if(v->blk_device2[1])
        {
            printf("failed to mount %s (%s); trying v->blk_device2[1] (%s)\n",
                 v->blk_device, strerror(errno), v->blk_device2[1]);
            result = mount(v->blk_device2[1], v->mount_point, "vfat",
                           MS_NOATIME | MS_NODEV | MS_NODIRATIME, "");
            if(result && errno == EROFS) {
                result = mount(v->blk_device2[1], v->mount_point, v->fs_type,
                        MS_NOATIME | MS_NODEV | MS_NODIRATIME | MS_RDONLY, "");
            }

            if(result == 0)
                return 0;

            result = mount(v->blk_device2[1], v->mount_point, "texfat",
                           MS_NOATIME | MS_NODEV | MS_NODIRATIME, "");
            if(result && errno == EROFS) {
                result = mount(v->blk_device2[1], v->mount_point, v->fs_type,
                               MS_NOATIME | MS_NODEV | MS_NODIRATIME | MS_RDONLY, "");
            }

            if(result == 0)
                return 0;

            result = mount(v->blk_device2[1], v->mount_point, "tntfs",
                           MS_NOATIME | MS_NODEV | MS_NODIRATIME, "");
            if(result && errno == EROFS) {
                result = mount(v->blk_device2[1], v->mount_point, v->fs_type,
                               MS_NOATIME | MS_NODEV | MS_NODIRATIME | MS_RDONLY, "");
            }

            if(result == 0)
                return 0;

            result = mount(v->blk_device2[1], v->mount_point, "ufsd",
                    MS_NOATIME | MS_NODEV | MS_NODIRATIME, "force");
            if(result && errno == EROFS) {
                result = mount(v->blk_device2[1], v->mount_point, "ufsd",
                    MS_NOATIME | MS_NODEV | MS_NODIRATIME | MS_RDONLY, "force");
            }

            if(result == 0)
                return 0;
        }

        printf("failed to mount %s (%s)\n", v->mount_point, strerror(errno));
        return -1;
    }


    printf("unknown fs_type \"%s\" for %s\n", v->fs_type, v->mount_point);
    return -1;
}

int ensure_path_unmounted(const char* path) {
    Volume* v = volume_for_path(path);
    if (v == NULL) {
        printf("unknown volume for path [%s]\n", path);
        return -1;
    }
    if (strcmp(v->fs_type, "ramdisk") == 0) {
        // the ramdisk is always mounted; you can't unmount it.
        return -1;
    }

    int result;
    result = scan_mounted_volumes();
    if (result < 0) {
        printf("failed to scan mounted volumes\n");
        return -1;
    }

    const MountedVolume* mv =
        find_mounted_volume_by_mount_point(v->mount_point);
    if (mv == NULL) {
        // volume is already unmounted
        return 0;
    }

    return unmount_mounted_volume(mv);
}

int format_volume(const char* volume) {
    Volume* v = volume_for_path(volume);
    if (v == NULL) {
        printf("unknown volume \"%s\"\n", volume);
        return -1;
    }

    if (strcmp(v->fs_type, "ext4") == 0) {
        int result = make_ext4fs(v->blk_device, v->length, volume, sehandle);
        if (result != 0) {
            printf("format_volume: make_extf4fs failed on %s\n", v->blk_device);
            return -1;
        }
        return 0;
    }

    printf("format_volume: fs_type \"%s\" unsupported\n", v->fs_type);
    return -1;
}
