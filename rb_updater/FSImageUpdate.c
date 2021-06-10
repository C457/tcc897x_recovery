#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#ifdef QNX
#include <sys/statvfs.h>
#else
#include <sys/xattr.h>
#include <sys/statfs.h>
#endif
#include <sys/mount.h>
#include <sys/types.h>
#include <stdarg.h>
#include <fcntl.h>


#include "RB_FSImageUpdate.h"
#include "RB_vRM_Errors.h"
#include "RB_common.h"

/* #include "fota_comp_codec_me.h" */
/* #include "RB_ExtCompApi.h"*/

#define printf_dbg     printf

#define BACKUP_PATH "/data/update/back_tmp"

#define FS_IMAGE_MAX 3

#define FS_IMAGE_BOOT_OFFSET        0x1000000 // 16M
#define FS_IMAGE_RECOVERY_OFFSET    0x2000000 // 32M
#define FS_IMAGE_SPLASH_OFFSET      0x4000000 // 16M

int FS_ImgHandle[FS_IMAGE_MAX] = {0,};

long RB_SyncImage(void *pbUserData)
{
    printf_dbg(" %s\n",__func__);
    sync();
    return S_RB_SUCCESS;
}

long RB_SyncBackup(void *pbUserData)
{
    //backup fileø° ¥Î«— sync
    return S_RB_SUCCESS;
}

static int get_real_address(RB_UINT64 StartAddress, struct fsImage_data *fs_img_t)
{
	printf_dbg(" %s\n",__func__);
	if((FS_IMAGE_BOOT_OFFSET <= StartAddress)
        && (StartAddress < FS_IMAGE_RECOVERY_OFFSET)) {
        fs_img_t->RealStartAddr = StartAddress - FS_IMAGE_BOOT_OFFSET;
        fs_img_t->pwHandle = FS_ImgHandle[FS_IMAGE_BOOT];
	} else if((FS_IMAGE_RECOVERY_OFFSET <= StartAddress)
	           && (StartAddress < FS_IMAGE_SPLASH_OFFSET)) {
        fs_img_t->RealStartAddr = StartAddress - FS_IMAGE_RECOVERY_OFFSET;
        fs_img_t->pwHandle = FS_ImgHandle[FS_IMAGE_RECOVERY];
	} else if((FS_IMAGE_SPLASH_OFFSET <= StartAddress)
	           && (StartAddress < (FS_IMAGE_SPLASH_OFFSET+FS_IMAGE_BOOT_OFFSET))) {
        fs_img_t->RealStartAddr = StartAddress - FS_IMAGE_SPLASH_OFFSET;
        fs_img_t->pwHandle = FS_ImgHandle[FS_IMAGE_SPLASH];
    } else {
        printf_dbg("%s. Do Nothing\n",__func__);
        return -1;
	}

	printf_dbg("==> StartAddress = 0x%llx  realAdd = 0x%llx\n",StartAddress, fs_img_t->RealStartAddr);
	printf_dbg("==> fs_img_t.pwHandle = %d\n", fs_img_t->pwHandle);

	return 0;
}

long RB_ReadImage64(void *pbUserData,
                            unsigned char *pbBuffer,        /* pointer to user buffer */
                            RB_UINT64 llStartAddress,       /* memory address to read from */
                            RB_UINT32 dwSize)               /* number of bytes to copy */
{
    int ret = 0;
    struct fsImage_data fsImage = {0, };
    printf_dbg(" %s\n",__func__);

    ret = get_real_address(llStartAddress, &fsImage);

    if((0 >= fsImage.pwHandle) || (0 > ret)) {
        printf_dbg("input file open failed, handle = %d\n", fsImage.pwHandle);
        return -1;
	}

	ret = lseek64(fsImage.pwHandle, fsImage.RealStartAddr, SEEK_SET);
	if(ret < 0) {
        printf_dbg("input file seek failed\n");
        return -1;
	}

	ret = read(fsImage.pwHandle, pbBuffer, dwSize);
	if(0 > ret) {
        printf_dbg("read failed with return value: %ld\n",ret);
        return -1;
	}

	return S_RB_SUCCESS;
}

long RB_ReadBackup64(void* pbUserData,
                              unsigned char* pbBuffer,
                              RB_UINT64 llAddress,
                              RB_UINT32 dwSize)
{
    long  pwHandle;

    printf_dbg("%s\n",__func__);
    //rb_readfile call
    pwHandle = open(BACKUP_PATH,O_RDONLY);

    if(pwHandle < 0) {
        printf_dbg("%s : %s ==> open fail\n",__func__, BACKUP_PATH);
    }
  
    RB_ReadFile(pbUserData, pwHandle, llAddress, pbBuffer, dwSize);

    return S_RB_SUCCESS;
}

long RB_WriteBackup64(void* pbUserData,
                              unsigned char* pbBuffer,
                              RB_UINT64 llAddress,
                              RB_UINT32 dwSize)
{
    long  pwHandle;

    printf_dbg("%s\n",__func__);

    pwHandle = open(BACKUP_PATH,O_RDONLY);

    if(pwHandle < 0) {
        printf_dbg(" %s : %s ==> open fail\n",__func__, BACKUP_PATH);
    }

    RB_WriteFile(pbUserData, pwHandle,  llAddress,  pbBuffer, dwSize);

    return S_RB_SUCCESS;
}
		
long RB_WriteImage64(void* pbUserData,
                            unsigned char* pbBuffer,
                            RB_UINT64 llAddress,
                            RB_UINT32 dwSize)
{
    printf_dbg(" %s\n",__func__);

    int ret = 0;
    struct fsImage_data fsImage = {0 , };
    ret = get_real_address(llAddress, &fsImage);

	if((0 >= fsImage.pwHandle) || (0 > ret)) {
        printf_dbg("input file open failed, handle = %d\n", fsImage.pwHandle);
        return -1;
	}

	ret = lseek64(fsImage.pwHandle, fsImage.RealStartAddr, SEEK_SET);
	if(ret < 0) {
        printf_dbg("[%s] input file seek failed : %d\n", __func__, ret);
        return -1;
	}

	ret = write(fsImage.pwHandle, pbBuffer, dwSize);
	if(ret < 0) {
        printf_dbg("Failed with return value: %d, (%s)\n",ret, strerror(errno));
        return -1;
	}

	return S_RB_SUCCESS;
}

long RB_WriteBlockWithMetadata (void *pbUserData,
                                           unsigned long dwBlockAddress,
                                           unsigned char	*pbBuffer,
                                           unsigned char *pbMDBuffer,
                                           unsigned long dwMdSize)
{
    printf_dbg("%s\n",__func__);
    return S_RB_SUCCESS;
}
                                      
