//+[TCCQB]
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>

#include <fs_mgr.h>
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

extern "C" {
#include <sparse/sparse.h>
}

extern bool boot_mode_is_sdmmc;
extern RecoveryUI* ui;
extern int
get_menu_selection(const char* const * headers, const char* const * items,
                   int menu_only, int initial_selection, Device* device);
extern const char**
prepend_title(const char* const* headers);

static unsigned long long qb_data_time = 0;

extern const char *CACHE_ROOT;
extern const char *SDCARD_ROOT;
extern const char *INTERNAL_ROOT;
extern const char *USB_ROOT;
extern const char *QB_INTERNAL_ROOT;
extern const char *SYSTEM_ROOT;
extern const char *TEMPORARY_DIR_ROOT;
extern const char *TEMP1_ROOT;
extern char  save_qbdata_path_usb[];
extern char  save_qbdata_path_hu[];

struct tm *t;

#define LOW_STORAGE_FOOTPRINT

enum PART_NAME {
  USERDATA,
  CACHE,
  SNAPSHOT,
  QB_DATA, 
  PART_NUM
};

const char* file_list[PART_NUM][3] = {
  {"userdata.sparse.img", "userdata.sparse.img.gz", NULL}, 
  {"cache.sparse.img", "cache.sparse.img.gz", NULL}, 
  {"snapshot.sparse.img", "snapshot.sparse.img.gz", "snapshot.raw.img"},
  {"qb_data.sparse.img", NULL, NULL}
};

struct volume_list {
  char blk_label[4096];
  Volume* list; 
};
struct volume_list v_head[PART_NUM];

int get_build_time(char *build_time, int size)
{
    memset(build_time, 0, size);
    property_get("ro.build.date.releaze", build_time, "no_dev_name");

    if (!strcmp(build_time, "no_dev_name")) {
        memset(build_time, 0, size);
        property_get("ro.build.date.releaze", build_time, "no_dev_name");
    }

    return 0;
}

int get_product_name(char *product_name, int size)
{
    memset(product_name, 0, size);
    property_get("ro.product.device", product_name, "no_dev_name");

    if(!strcmp(product_name, "no_dev_name")) {
        memset(product_name, 0, size);
        property_get("ro.product.name", product_name, "no_dev_name");
    }

    return 0;
}

static void __do_convert_img2simg(const char *dev_mnt_point, int which)
{
  int in, out, ret;
  struct sparse_file *s;
  unsigned int block_size = 4096;
  char if_root[512], of_root[512];
  off64_t len;

#if 0
  sprintf(if_root, "%s/%s", dev_mnt_point, v_head[which].blk_label);
#else
  sprintf(if_root, "%s", v_head[which].list->blk_device);
#endif
  sprintf(of_root, "%s/%s", dev_mnt_point, file_list[which][1]);

  printf("%s: if_root(%s), of_root(%s)\n", __func__, if_root, of_root);

  in = open(if_root, O_RDONLY);
  if (in < 0) {
    printf("%s: cannot open input file %s\n", __func__, if_root);
    exit(-1);
  }

  out = open(of_root, O_WRONLY  | O_CREAT | O_TRUNC, 0664);
  if (out < 0) {
    printf("%s: cannot open output file %s\n", __func__, of_root);
    exit(-1);
  }

  len = lseek64(in, 0, SEEK_END);
  lseek64(in, 0, SEEK_SET);

  s = sparse_file_new(block_size, len);
  if (!s) {
    printf("%s: failed to create sparse file\n", __func__);
    exit(-1);
  }

  sparse_file_verbose(s);
  ret = sparse_file_read(s, in, false, false);
  if (ret) {
    printf("%s: failed to read file\n", __func__);
    exit(-1);
  }

  ret = sparse_file_write(s, out, true, true, false);
  if (ret) {
    printf("%s: failed to write sparse file\n", __func__);
    exit(-1);
  }

  close(in);
  close(out);
  
  exit(ret);
}

static void __do_convert_ext2simg(const char *dev_mnt_point, int which) 
{
  int ret = 0;
  char if_root[512], of_root[512];

#if 0
  sprintf(if_root, "%s/%s", dev_mnt_point, v_head[which].blk_label);
#else
  sprintf(if_root, "%s", v_head[which].list->blk_device);
#endif
  sprintf(of_root, "%s/%s", dev_mnt_point, file_list[which][1]);

  
  ret = execl("/system/bin/ext2simg", 
      "ext2simg", "-z", if_root, of_root, NULL);

  printf("%s: if_root(%s), of_root(%s), ret(%d)\n", __func__, if_root, of_root, ret);

  exit(ret);
}

static void __copy_sparse_raw_img(const char *dev_mnt_point, int which)
{
  int ret = 0;
  char src[512];

  sprintf(src, "%s/%s", dev_mnt_point, file_list[which][1]);

  
  ret = execl("/system/bin/cp", 
      "cp", src, QB_INTERNAL_ROOT, NULL);
  
  printf("%s: src(%s), QB_INTERNAL_ROOT(%s), ret(%d)\n", __func__, src, QB_INTERNAL_ROOT, ret);
  
  exit(ret);
}

static void __copy_sparse_ext4_img(const char *dev_mnt_point, int which)
{
  int ret = 0;
  char src[512];

  sprintf(src, "%s/%s", dev_mnt_point, file_list[which][1]);

  ret = execl("/system/bin/cp", 
      "cp", src, QB_INTERNAL_ROOT, NULL);

  printf("%s: src(%s), QB_INTERNAL_ROOT(%s), ret(%d)\n", __func__, src, QB_INTERNAL_ROOT, ret);
  
  exit(ret);
}

static void __copy_general_img(const char *src, const char *dest)
{
  int ret = 0;

  ret = execl("/system/bin/cp", 
      "cp", "-rf", src, dest, NULL);
  
  printf("%s: src(%s), dest(%s), ret(%d)\n", __func__, src, dest, ret);
 
  exit(ret);
}

static void __do_extract_binary_data(const char *dev_mnt_point, int which)
{
  int ret = 0;
  char if_root[512], of_root[512];

  sprintf(if_root, "if=%s", v_head[which].list->blk_device);
  sprintf(of_root, "of=%s/%s", dev_mnt_point, v_head[which].blk_label);

  ret = execl("/system/bin/dd", 
      "dd", if_root, of_root, NULL);
  
  printf("%s: if_root(%s), of_root(%s), ret(%d)\n", __func__, if_root, of_root, ret);
  
  exit(ret);
}

static void __make_qb_data(int which, const char* src, const char* dest)
{
  int fd = 0;
  int ret = 0;
  char size[512], out_file[4096];

  fd = open(v_head[which].list->blk_device, O_RDONLY);
  if (fd<0) {
    printf("%s: failed to open %s ...\n", __func__, v_head[which].list->blk_device);
    exit(-1);
  }
  sprintf(size, "%d", (unsigned int)lseek(fd, 0, SEEK_END));
  close(fd);

  sprintf(out_file, "%s/qb_data.sparse.img", dest);

  ret = execl("/system/bin/make_ext4fs", 
      "make_ext4fs", "-s", "-l", size, "-a", "qb_data", out_file, src, NULL);
  
  exit(ret);
}

void __do_simg2img(int which, const char* src, const char* dest)
{
    int ret = 0;
    char out_file[4096];

    sprintf(out_file, "%s/%s", dest, file_list[which][2]);

    ret = execl("/system/bin/simg2img", "simg2img", src, out_file, NULL);

    printf("%s: src(%s), out_file(%s), ret(%d)\n", __func__, src, out_file, ret);

    exit(ret);
}

void __do_gunzip(const char* path)
{
    int ret = 0;
    ret = execl("/system/bin/gzip", "gzip", "-d", path, NULL);
    exit(ret);
}

void __do_gzip(const char* path)
{
    int ret = 0;
    ret = execl("/system/bin/gzip", "gzip", path, NULL);
    exit(ret);
}

int wait_child_ps(pid_t waiting_pid)
{
    int status;
    pid_t terminated_pid;

    terminated_pid = wait(&status);

    return status;
}

int extract_qb_data_hu(const char *external_storage_root)
{
    struct timeval tv1, tv2, tv3, tv4;
    unsigned long long usec = 0;
    pid_t pid;
    int status = 0;
    int ret = 0;

    format_volume("/cache");
    ui->Print("\n HU backup [%s]\n", external_storage_root);

    gettimeofday(&tv1, NULL);
    ui->Print("[extract_qb_data]path = [%s] Start extracting qb data...\n", external_storage_root);

    char dir_root[4096], dir_path1[4096], dir_path2[4096], tmp_buf[4096];
    char product_name[PROP_VALUE_MAX];

    ret = mkdir(external_storage_root, 0666);
    if(0 != ret) {
        ui->Print("[%s]create fail error = (%d), error no = (%d)\n", external_storage_root, ret, errno);
    }

    get_product_name(product_name, PROP_VALUE_MAX);
    strcpy(dir_root, "");
    sprintf(dir_root, "%s/daudio_%s.%04d.%02d.%02d.%02d.%02d.%02d",
            external_storage_root, product_name, t->tm_year + 1900,
            t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);

    strcpy((char*)save_qbdata_path_hu , (char*)dir_root);

    ret = mkdir(dir_root, 0666);
	if(0 != ret) {
        ui->Print("dir_root create fail error = (%d), error no =(%d)\n", external_storage_root, ret, errno);
	}

    strcpy(dir_path1, "");
    strcpy(dir_path1, dir_root);
    strcat(dir_path1, "/qb_data");
    strcpy(dir_path2, "");
    strcpy(dir_path2, dir_root);
    strcat(dir_path2, "/fwdn");

    printf("external_storage_root(%s)\n", external_storage_root);
    printf("dir_root(%s)\n", dir_root);
    printf("dir_path1(%s)\n", dir_path1);
    printf("dir_path2(%s)\n", dir_path2);

    ret = mkdir(dir_path1, 0666);
    if(0 > ret) {
        ui->Print("dir_path1 [%s] create fail error = (%d)\n", dir_path1 ,ret);
    }

    ui->Print("copy /qb_data : dir_path1 = [%s]\n", dir_path1);
    sleep(1);

    pid = fork();
    if(pid == 0) {
        __copy_general_img("/qb_data/cache.sparse.img.gz",dir_path1 );
    }

    status = wait_child_ps(pid);
    if(status) {
        ui->Print("[%s] error(%d)\n", __func__, status);
        return -1;
    }

    pid = fork();
    if (pid == 0) {
        __copy_general_img("/qb_data/userdata.sparse.img.gz",dir_path1 );
    }

    status = wait_child_ps(pid);
    if (status) {
        ui->Print("[%s] error(%d)\n", __func__, status);
        return -1;
    }

    sleep(1);

    pid = fork();
    if (pid == 0) {
        __do_convert_img2simg(dir_path1, SNAPSHOT);
    }

    status = wait_child_ps(pid);
    if (status) {
        ui->Print("[%s] error(%d)\n", __func__, status);
        return -1;
    }

    gettimeofday(&tv2, NULL);
    usec = (tv2.tv_sec - tv1.tv_sec) * 1000000 + tv2.tv_usec - tv1.tv_usec;
    ui->Print("Done.(%lld usec)\n", usec);

    ui->Print("Phase(2/3) Copying extraced file to internal /qb_data partition ...\n");
    pid = fork();
    if(pid==0) {
        __copy_sparse_ext4_img(dir_path1, CACHE);
    }

    status = wait_child_ps(pid);
    if(status) {
        ui->Print("[%s] error(%d)\n", __func__, status);
        return -1;
    }

    pid = fork();
    if(pid==0) {
        __copy_sparse_ext4_img(dir_path1, USERDATA);
    }

    status = wait_child_ps(pid);
    if(status) {
        ui->Print("[%s] error(%d)\n", __func__, status);
        return -1;
    }

    gettimeofday(&tv3, NULL);
    usec = (tv3.tv_sec - tv2.tv_sec) * 1000000 + tv3.tv_usec - tv2.tv_usec;
    ui->Print("Done.(%lld usec)\n", usec);

    ui->Print("Phase(3/3) Preparing rom images used by fwdn ...\n");
    pid = fork();
    if (pid==0) {
        __copy_general_img(dir_path1, dir_path2);
    }

    status = wait_child_ps(pid);
    if (status) {
        ui->Print("[%s] error(%d)\n", __func__, status);
        return -1;
    }

    ui->Print("Phase(3-1/3) Making qb_data.img ...\n");
    strcpy(tmp_buf, "");
    sprintf(tmp_buf, "%s/%s", dir_path1, file_list[SNAPSHOT][1]);
    remove(tmp_buf);

    pid = fork();
    if (pid == 0) {
        __make_qb_data(QB_DATA, dir_path1, dir_path2);
    }

    status = wait_child_ps(pid);
    if (status) {
        ui->Print("[%s] error(%d)\n", __func__, status);
        return -1;
    }

    ui->Print("Phase(3-2/3) Converting userdata.sparse.img, cache.sparse.img, and snapshot.raw.img ...\n");
    strcpy(tmp_buf, "");
    sprintf(tmp_buf, "%s/%s", dir_path2, file_list[USERDATA][1]);

    pid = fork();
    if(pid == 0) {
        __do_gunzip(tmp_buf);
    }

    status = wait_child_ps(pid);
    if(status) {
        ui->Print("[%s] error(%d)\n", __func__, status);
        return -1;
    }

    sprintf(tmp_buf, "%s/%s", dir_path2, file_list[CACHE][1]);
    pid = fork();
    if(pid == 0) {
        __do_gunzip(tmp_buf);
    }

    status = wait_child_ps(pid);
    if(status) {
        ui->Print("[%s] error(%d)\n", __func__, status);
        return -1;
    }

    sprintf(tmp_buf, "%s/%s", dir_path2, file_list[SNAPSHOT][1]);
    pid = fork();
    if(pid == 0) {
        __do_gunzip(tmp_buf);
    }

    status = wait_child_ps(pid);
    if(status) {
        ui->Print("[%s] error(%d)\n", __func__, status);
        return -1;
    }

    sprintf(tmp_buf, "%s/%s", dir_path2, file_list[SNAPSHOT][0]);
    pid = fork();
    if (pid == 0) {
        __do_simg2img(SNAPSHOT, tmp_buf, dir_path2);
    }

    status = wait_child_ps(pid);
    if (status) {
        ui->Print("[%s] error(%d)\n", __func__, status);
        return -1;
    }

#if 0 //snapshot.sparse.img rename......
    char src_name[256];
    char dest_name[256];
    char build_time[PROP_VALUE_MAX];

    sprintf(src_name, "%s/snapshot.sparse.img", dir_path2);

    get_build_time(build_time, PROP_VALUE_MAX);
    sprintf(dest_name, "%s/snapshot.sparse_%s.img", dir_path2 , build_time);

    rename(src_name, dest_name);
#endif

    gettimeofday(&tv4, NULL);
    usec = (tv4.tv_sec - tv3.tv_sec) * 1000000 + tv4.tv_usec - tv3.tv_usec;
    ui->Print("Done.(%lld usec)\n", usec);

    gettimeofday(&tv2, NULL);
    usec = (tv2.tv_sec - tv1.tv_sec) * 1000000 + tv2.tv_usec - tv1.tv_usec;
    ui->Print("[extract_qb_data] Finished.(%lld usec)\n", usec);

    sync();
    sync();
    sync();

    return 0;
}

void checkfolder(const char *path)
{
    char dir_root[4096];
    int fd_temp = 0;
    time_t timer;
    time_t current_time;
    char product_name[PROP_VALUE_MAX] = {0, };

    get_product_name(product_name, PROP_VALUE_MAX);
    sprintf(dir_root, "%s/daudio_%s.%04d.%02d.%02d.%02d.%02d.%02d",
            path, product_name, t->tm_year + 1900,
            t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);

    mkdir(dir_root, 0666);
    if(errno == EEXIST) {
        sleep(1); // 1 sec
        timer = time( &current_time);
        t = localtime(&timer);
        checkfolder(path);
    }
}

int extract_qb_data(const char *external_storage_root, int extract_qb_data_only)
{
    struct timeval tv1, tv2, tv3, tv4;
    unsigned long long usec = 0;
    pid_t pid;
    int status = 0;

    if(extract_qb_data_only) {
        external_storage_root = " ";
    }

    ui->Print("[extract_qb_data] extracting qb data to [%s]\n", external_storage_root);

    format_volume("/cache");

    gettimeofday(&tv1, NULL);
    ui->Print("[extract_qb_data] Start extracting qb data...\n");

    char dir_root[4096], dir_path1[4096], dir_path2[4096], tmp_buf[4096];
    time_t timer;
    char product_name[PROP_VALUE_MAX];

    timer = time(NULL);
    t = localtime(&timer);

#if 1  //+[TCCQB] Making QB_DATA
    checkfolder(external_storage_root);
#endif //-[TCCQB]

    get_product_name(product_name, PROP_VALUE_MAX);
    strcpy(dir_root, "");
    sprintf(dir_root, "%s/daudio_%s.%04d.%02d.%02d.%02d.%02d.%02d",
            external_storage_root, product_name, t->tm_year + 1900,
            t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
    strcpy((char*)save_qbdata_path_usb , (char*)dir_root);
#if 0 //+[TCCQB] Making QB_DATA
    int ret = 0;
    ret = mkdir(dir_root, 0666);

    if(0 != ret) {
        ui->Print("[%s] create fail error = (%d), error no = (%d)\n", dir_root, ret, errno);
    }
#endif //-[TCCQB]
    if(extract_qb_data_only) {
        strcpy(dir_path1, "");
    } else {
        strcpy(dir_path1, "");
        strcpy(dir_path1, dir_root);
    }

    strcat(dir_path1, "/qb_data");
    strcpy(dir_path2, "");
    strcpy(dir_path2, dir_root);
    strcat(dir_path2, "/fwdn");

    printf("external_storage_root(%s)\n", external_storage_root);
    printf("dir_root(%s)\n", dir_root);
    printf("dir_path1(%s)\n", dir_path1);
    printf("dir_path2(%s)\n", dir_path2);

#if 1 //+[TCCQB] Making QB_DATA
    mkdir(dir_path1, 0666);
#else
    ret = mkdir(dir_path1, 0666);
    if(0 != ret) {
        ui->Print("[%s] create fail error = (%d), error no = (%d)\n", dir_path1, ret, errno);
    }
#endif //-[TCCQB]

    if(extract_qb_data_only) {
        ui->Print("Phase(1/3) Extracting quickboot image ...\n");
        pid = fork();
        if(pid == 0) {
            __do_convert_ext2simg(dir_path1, CACHE);
        }

        status = wait_child_ps(pid);
        if(status) {
            ui->Print("[%s] error(%d)\n", __func__, status);
            return -1;
        }

        pid = fork();
        if(pid == 0) {
            __do_convert_ext2simg(dir_path1, USERDATA);
        }

        status = wait_child_ps(pid);
        if(status) {
            ui->Print("[%s] error(%d)\n", __func__, status);
            return -1;
        }

        ui->Print("extract QB to qb_data done.\n");
        sync();
        sync();
        sync();

        return 0;
    } else { //not extract qb data only
        ui->Print("copy /qb_data : dir_path1 = [%s]\n", dir_path1);
        sleep(1);

        pid = fork();
        if(pid == 0) {
            __copy_general_img("/qb_data/cache.sparse.img.gz", dir_path1 );
        }

        status = wait_child_ps(pid);
        if(status) {
            ui->Print("[%s] error(%d)\n", __func__, status);
            return -1;
        }

        pid = fork();
        if(pid == 0) {
            __copy_general_img("/qb_data/userdata.sparse.img.gz",dir_path1 );
        }

        status = wait_child_ps(pid);
        if(status) {
            ui->Print("[%s] error(%d)\n", __func__, status);
            return -1;
        }

        sleep(1);
    }

    pid = fork();
    if(pid == 0) {
        __do_convert_img2simg(dir_path1, SNAPSHOT);
    }

    status = wait_child_ps(pid);
    if(status) {
        ui->Print("[%s] error(%d)\n", __func__, status);
        return -1;
    }

    gettimeofday(&tv2, NULL);
    usec = (tv2.tv_sec - tv1.tv_sec) * 1000000 + tv2.tv_usec - tv1.tv_usec;
    ui->Print("Done.(%lld usec)\n", usec);

    ui->Print("Phase(2/3) Copying extraced file to internal /qb_data partition ...\n");
    pid = fork();
    if(pid == 0) {
        __copy_sparse_ext4_img(dir_path1, CACHE);
    }

    status = wait_child_ps(pid);
    if(status) {
        ui->Print("[%s] error(%d)\n", __func__, status);
        return -1;
    }

    pid = fork();
    if(pid == 0) {
        __copy_sparse_ext4_img(dir_path1, USERDATA);
    }

    status = wait_child_ps(pid);
    if(status) {
        ui->Print("[%s] error(%d)\n", __func__, status);
        return -1;
    }

#if 0 //DAUDIO CODE
    pid = fork();
    if(pid == 0) {
        __copy_sparse_raw_img(dir_path1, SNAPSHOT);
    }

    status = wait_child_ps(pid);
    if (status) {
        ui->Print("[%s] error(%d)\n", __func__, status);
        return -1;
    }
#endif //DAUDIO CODE

    gettimeofday(&tv3, NULL);
    usec = (tv3.tv_sec - tv2.tv_sec) * 1000000 + tv3.tv_usec - tv2.tv_usec;
    ui->Print("Done.(%lld usec)\n", usec);

    ui->Print("Phase(3/3) Preparing rom images used by fwdn ...\n");
    pid = fork();
    if(pid == 0) {
        __copy_general_img(dir_path1, dir_path2);
    }

    status = wait_child_ps(pid);
    if(status) {
        ui->Print("[%s] error(%d)\n", __func__, status);
        return -1;
    }

    ui->Print("Phase(3-1/3) Making qb_data.img ...\n");
    strcpy(tmp_buf, "");
    sprintf(tmp_buf, "%s/%s", dir_path1, file_list[SNAPSHOT][1]);
    remove(tmp_buf);

    pid = fork();
    if (pid == 0) {
        __make_qb_data(QB_DATA, dir_path1, dir_path2);
    }

    status = wait_child_ps(pid);
    if(status) {
        ui->Print("[%s] error(%d)\n", __func__, status);
        return -1;
    }

    ui->Print("Phase(3-2/3) Converting userdata.sparse.img, cache.sparse.img, and snapshot.raw.img...\n");
    strcpy(tmp_buf, "");
    sprintf(tmp_buf, "%s/%s", dir_path2, file_list[USERDATA][1]);

    pid = fork();
    if(pid == 0) {
        __do_gunzip(tmp_buf);
    }

    status = wait_child_ps(pid);
    if(status) {
        ui->Print("[%s] error(%d)\n", __func__, status);
        return -1;
    }

    sprintf(tmp_buf, "%s/%s", dir_path2, file_list[CACHE][1]);
    pid = fork();
    if(pid == 0) {
        __do_gunzip(tmp_buf);
    }

    status = wait_child_ps(pid);
    if(status) {
        ui->Print("[%s] error(%d)\n", __func__, status);
        return -1;
    }

    sprintf(tmp_buf, "%s/%s", dir_path2, file_list[SNAPSHOT][1]);
    pid = fork();
    if(pid == 0) {
        __do_gunzip(tmp_buf);
    }

    status = wait_child_ps(pid);
    if(status) {
        ui->Print("[%s] error(%d)\n", __func__, status);
        return -1;
    }

    sprintf(tmp_buf, "%s/%s", dir_path2, file_list[SNAPSHOT][0]);
    pid = fork();
    if(pid == 0) {
        __do_simg2img(SNAPSHOT, tmp_buf, dir_path2);
    }

    status = wait_child_ps(pid);
    if(status) {
        ui->Print("[%s] error(%d)\n", __func__, status);
        return -1;
    }

#if 0//snapshot.sparse.img rename......
    {
        char src_name[256];
        char dest_name[256];
        char build_time[PROP_VALUE_MAX];

        sprintf(src_name, "%s/snapshot.sparse.img", dir_path2);

        get_build_time(build_time, PROP_VALUE_MAX);
        sprintf(dest_name, "%s/snapshot.sparse_%s.img", dir_path2 , build_time);

        rename(src_name, dest_name);
    }
#endif

    gettimeofday(&tv4, NULL);
    usec = (tv4.tv_sec - tv3.tv_sec) * 1000000 + tv4.tv_usec - tv3.tv_usec;
    ui->Print("Done.(%lld usec)\n", usec);

    gettimeofday(&tv2, NULL);
    usec = (tv2.tv_sec - tv1.tv_sec) * 1000000 + tv2.tv_usec - tv1.tv_usec;
    qb_data_time = usec;	// save qb_data_time.

    ui->Print("[extract_qb_data] Wait for data sync...\n");
    ui->Print("[extract_qb_data] Finished.(%lld usec)\n", usec);

    sync();
    sync();
    sync();

    return 0;
}

int update_qb_image(const char* dev_mnt_point, int which) 
{
  int in, out, ret;
  struct sparse_file *s;
  pid_t pid;
  char tin_gzip_path[512], tin_gunzip_path[512], in_gzip_path[512], of_path[512];

  sprintf(in_gzip_path, "%s/%s", dev_mnt_point, file_list[which][1]);
  sprintf(of_path, "%s", v_head[which].list->blk_device);

  in = open(in_gzip_path,  O_RDONLY);
  if (in < 0) {
    ui->Print("[update_qb_image] Cannot open input file %s\n", in_gzip_path);
    return -1;
  }
  close(in);

#ifdef LOW_STORAGE_FOOTPRINT
  /*High Memory Footprint*/
  sprintf(tin_gzip_path, "%s/temp_file.gz", TEMPORARY_DIR_ROOT);
  sprintf(tin_gunzip_path, "%s/temp_file", TEMPORARY_DIR_ROOT);
#else
  sprintf(tin_gzip_path, "%s/temp_file.gz", dev_mnt_point);
  sprintf(tin_gunzip_path, "%s/temp_file", dev_mnt_point);
#endif
  ui->Print("[update_qb_image] Copy temp input file (%s->%s)\n", in_gzip_path, tin_gzip_path);
  pid = vfork();
  if (pid == -1) {
    ui->Print("Fail to create cp ps.\n");
  } else if (pid == 0) {
    __copy_general_img(in_gzip_path, tin_gzip_path);
  } else { 
    ret = wait_child_ps(pid);
    if (ret) {
      ui->Print("[%s] error(%d)\n", __func__, ret);
      return -1;
    }
  }

  ui->Print("[update_qb_image] Start to decompress %s... ", tin_gzip_path);
  pid = vfork();
  if (pid == -1) {
    ui->Print("Fail to create gunzip ps.\n");
  } else if (pid == 0) {
    __do_gunzip(tin_gzip_path);
  } else { 
    ret = wait_child_ps(pid);
    if (ret) {
      ui->Print("[%s] error(%d)\n", __func__, ret);
      return -1;
    }
  }
  ui->Print("Finished.\n");

  ui->Print("[update_qb_image] Open input file %s\n", tin_gunzip_path);
  in = open(tin_gunzip_path,  O_RDONLY);
  if (in < 0) {
    ui->Print("[update_qb_image] Cannot open input file %s\n", tin_gunzip_path);
    exit(-1);
  }

  ui->Print("[update_qb_image] Open output file %s\n", of_path);
  out = open(of_path, O_WRONLY);
  if (out < 0) {
    ui->Print("[update_qb_image] Cannot open output file %s\n", of_path);
    exit(-1);
  }

  ui->Print("[update_qb_image] Read sparse file\n");
  s = (struct sparse_file*)sparse_file_import(in, true, false);
  if (!s) {
    ui->Print("[update_qb_image] Failed to read sparse file\n");
    exit(-1);
  }

  lseek(out, 0, SEEK_SET);

  ui->Print("[update_qb_image] Write output file\n");
  ret = sparse_file_write(s, out, false, false, false);
  if (ret < 0) {
    ui->Print("[update_qb_image] Cannot write output file\n");
    exit(-1);
  }
  ui->Print("[update_qb_image] Destroy sparse file\n");
  sparse_file_destroy(s);
  close(in);
  close(out);
  remove(tin_gunzip_path);

  sync();
  sync();
  sync();

  return 0;
}

void init_volume_list(const char* volume, int num)
{
  char *token, *t_blk_label;
  char t_path[512];

  v_head[num].list = volume_for_path(volume);
  strcpy(t_path, v_head[num].list->blk_device);

  token = strtok(t_path, "/");
  do {
    t_blk_label = strdup(token);
  } while ((token=strtok(NULL, "/")) != NULL);
  strcpy(v_head[num].blk_label, t_blk_label);

#if 1
  printf("v_head[%d].list->device(%s)\n", num, v_head[num].list->blk_device);
  printf("v_head[%d].list->mount_point(%s)\n", num, v_head[num].list->mount_point);
  printf("v_head[%d].list->fs_type(%s)\n", num, v_head[num].list->fs_type);
  printf("v_head[%d].blk_label(%s)\n", num, v_head[num].blk_label);
#endif
}

int quickboot_factory_reset_opt(int sel)	// USERDATA : 0x1, CACHE : 0x2, SNAPSHOT, 0x4
{
 
  int error = 0;
  int ret = 0;
  if (sel & QB_OPT_DATA) {
	  init_volume_list("/data", USERDATA);
	  error = update_qb_image(QB_INTERNAL_ROOT, USERDATA);
	  if (error) {
		/*  persist.sys.QB.user.allow = "false"  Setting 	*/
		ret |= QB_OPT_DATA;
	  }
  }
  
  if (sel & QB_OPT_CACHE) {
	  init_volume_list("/cache", CACHE);
	  error = update_qb_image(QB_INTERNAL_ROOT, CACHE);
	  if (error) {
		ret |= QB_OPT_CACHE;
	  }
  }
 
  if (sel & QB_OPT_SNAPSHOT) {
	  init_volume_list("/snapshot", SNAPSHOT);
	  error = update_qb_image(QB_INTERNAL_ROOT, SNAPSHOT);
	  if (error) {
		ret |= QB_OPT_SNAPSHOT;
	  }
  }
  return ret;
}


int extract_qb_data_auto(const char* path, int extract_qb_data_only)
{
    int ret = 0;

    init_volume_list("/data", USERDATA);
    init_volume_list("/cache", CACHE);
    init_volume_list("/snapshot", SNAPSHOT);
    init_volume_list("/qb_data", QB_DATA);

    ret = extract_qb_data(path, extract_qb_data_only);
    if(0 != ret) {
        ui->Print("USB Image fail\n");
    } else {
        if(0 == extract_qb_data_only) {
            ret = ensure_path_mounted(TEMP1_ROOT);
            if(0 != ret) {
                sleep(1);
                ensure_path_mounted(TEMP1_ROOT);
            }

            extract_qb_data_hu(TEMP1_ROOT);
        }
    }
    ensure_path_unmounted(SYSTEM_ROOT);

    return ret;
}

int do_qb_prebuilt(Device* device)
{
  static const char** title_headers = NULL;

  bool enable_qb_prebuilt_menu = false;
  char enable_qb_prebuilt[4096];
  property_get("ro.tcc.qb_prebuilt_mode", enable_qb_prebuilt, "");
  if (!strcmp(enable_qb_prebuilt, "1"))
    enable_qb_prebuilt_menu = true;

  if (title_headers == NULL) {
    const char* headers[] = { "Quickboot Pre-built related functions",
      //"  THIS CAN NOT BE UNDONE.",
      "",
      NULL };
    title_headers = prepend_title((const char**)headers);
  }

  init_volume_list("/data", USERDATA);
  init_volume_list("/cache", CACHE);
  init_volume_list("/snapshot", SNAPSHOT);
  init_volume_list("/qb_data", QB_DATA);

  const char* items_enable[] = { 
    " Extract QB data to external SD card",
    " Extract QB data to USB memory stick",
    " Update userdata image from SD card",
    " Update userdata image from USB memory stick",
    " Update userdata image from internal storage",
    " Update snapshot image from SD card",
    " Update snapshot image from USB memory stick",
    " Update snapshot image from internal storage",
    " Update cachedata image from SD card",
    " Update cachedata image from USB memory stick",
    " Update cachedata image from internal storage",
    " Update all(userdata, cache, snapshot) from SD card",
    " Update all(userdata, cache, snapshot) from USB memory stick",
    " Update all(userdata, cache, snapshot) from internal storage",
    " Back",
    NULL };
  const char* items_disable[] = { 
    " Back",
    NULL };

  ensure_path_mounted(SYSTEM_ROOT);
  ensure_path_unmounted(CACHE_ROOT);

  while (1) 
  {
    int chosen_item = 0;
      chosen_item = get_menu_selection(title_headers, items_enable, 1, 0, device);
    switch (chosen_item) 
    {
      case 0:
	ensure_path_mounted(SDCARD_ROOT);
	ensure_path_mounted(QB_INTERNAL_ROOT);
	extract_qb_data(SDCARD_ROOT, 0);
	ensure_path_unmounted(QB_INTERNAL_ROOT);
	ensure_path_unmounted(SDCARD_ROOT);
	if (qb_data_time != 0)
		ui->Print("[extract_qb_data] Finished.(%lld usec)\n", qb_data_time);
	qb_data_time = 0;
	break;
      case 1:
	ensure_path_mounted(USB_ROOT);
	ensure_path_mounted(QB_INTERNAL_ROOT);
	extract_qb_data(USB_ROOT, 0);
	ensure_path_unmounted(QB_INTERNAL_ROOT);
	ensure_path_unmounted(USB_ROOT);
	if (qb_data_time != 0)
		ui->Print("[extract_qb_data] Finished.(%lld usec)\n", qb_data_time);
	qb_data_time = 0;
	break;
      case 2:
	ensure_path_mounted(SDCARD_ROOT);
	update_qb_image(SDCARD_ROOT, USERDATA);
	break;
      case 3:
	ensure_path_mounted(USB_ROOT);
	update_qb_image(USB_ROOT, USERDATA);
	break;
      case 4:
	ensure_path_mounted(QB_INTERNAL_ROOT);
	update_qb_image(QB_INTERNAL_ROOT, USERDATA);
	break;
      case 5:
	ensure_path_mounted(SDCARD_ROOT);
	update_qb_image(SDCARD_ROOT, SNAPSHOT);
	break;
      case 6:
	ensure_path_mounted(USB_ROOT);
	update_qb_image(USB_ROOT, SNAPSHOT);
	break;
      case 7:
	ensure_path_mounted(QB_INTERNAL_ROOT);
	update_qb_image(QB_INTERNAL_ROOT, SNAPSHOT);
	break;
      case 8:
	ensure_path_mounted(SDCARD_ROOT);
	update_qb_image(SDCARD_ROOT, CACHE);
	break;
      case 9:
	ensure_path_mounted(USB_ROOT);
	update_qb_image(USB_ROOT, CACHE);
	break;
      case 10:
	ensure_path_mounted(QB_INTERNAL_ROOT);
	update_qb_image(QB_INTERNAL_ROOT, CACHE);
	break;
      case 11:
	ensure_path_mounted(SDCARD_ROOT);
	update_qb_image(SDCARD_ROOT, USERDATA);
	update_qb_image(SDCARD_ROOT, CACHE);
	update_qb_image(SDCARD_ROOT, SNAPSHOT);
	break;
      case 12:
	ensure_path_mounted(USB_ROOT);
	update_qb_image(USB_ROOT, USERDATA);
	update_qb_image(USB_ROOT, CACHE);
	update_qb_image(USB_ROOT, SNAPSHOT);
	break;
      case 13:
	ensure_path_mounted(QB_INTERNAL_ROOT);
	update_qb_image(QB_INTERNAL_ROOT, USERDATA);
	update_qb_image(QB_INTERNAL_ROOT, CACHE);
	update_qb_image(QB_INTERNAL_ROOT, SNAPSHOT);
	break;
      case 14:
	umount(SYSTEM_ROOT);
	return 0;
      default:
	ui->Print("This menu isn't implemented yet...\n");
    }
  }

  umount(SYSTEM_ROOT);
  
  return 0;
}

int write_sparse_image_no_zip(const char *src_path, const char *dest_path)
{
	int ret = 0;
	int in, out;
	struct sparse_file *s;

	in = open(src_path, O_RDONLY);
	if (in < 0) {
		printf("%s: can't open %s\n", __func__, src_path);
		ret = -1;
		return ret;
	}

	out = open(dest_path, O_WRONLY);
	if (out < 0) {
		printf("%s: can't open %s\n", __func__, dest_path);
		ret = -2;
		return ret;
	}

	lseek(in, 0, SEEK_SET);
	lseek(out, 0, SEEK_SET);
	s = sparse_file_import(in, true, false);
	if (!s) {
		printf("[%d] Failed to read sparse file\n",__LINE__);
		ret = -3;
		return ret;
	}

	ret = sparse_file_write(s, out, false, false, false);
	if (ret < 0) {
		printf("[%d] Can't write output file\n", __LINE__);
		ret = -4;
		return ret;
	}

	fprintf(stdout, "%s: %s  successfully finished(NoZip)!!\n", src_path, dest_path);
	sparse_file_destroy(s);
	close(in);
	close(out);

	return ret;
}
//-[TCCQB]
//
