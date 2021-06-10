 /*!
 *******************************************************************************
 * \file	RB_common.h
 *
 * \brief	UPI FS Update API
 *******************************************************************************
 */
 
#ifndef _RB_COMMON_H_
#define _RB_COMMON_H_

#define F_OK 0 // file exist ? 
#define X_OK 1 // possible execute ?  
#define W_OK 2 // possible write ?
#define R_OK 4 // possible read ?

#define false 0
#define true 1

#define ERASE_BUF_SIZE_MAX 1048576*4 //4M
#define W_BUF_SIZE_MAX 4096
#define SYSTEM_UD_PT_SIZE  4
#define IMAGE_NAME_SIZE  32
#define DEV_MOUNT_BLK_SIZE  128

typedef struct fsImage_data {
    RB_UINT64 RealStartAddr; 
    int pwHandle;
};

typedef enum partition_for_fs
{
    FS_SYSTEM_EXT4,
    FS_VR1_EXT4,
    FS_VR2_EXT4,
    FS_EXT4_MAX
}FS_PT_TYPE;

typedef enum partition_for_img
{
    FS_IMAGE_BOOT,
    FS_IMAGE_RECOVERY,
    FS_IMAGE_SPLASH,
    FS_IMAGE_MAX
}IMG_PT_TYPE;

enum {
    F_SYSTEM = 0,
    F_VR,
    F_MICOM,
    F_MODEM,
    F_GPS,
    F_MAX,
};

enum {
    PT_BOOT = 0,
    PT_RECOVERY,
    PT_SPLASH,
    PT_SYSTEM,
    PT_VR1,
    PT_VR2,
    PT_MAX,
};

static const char *gen5_folder_name[F_MAX] = {"SYSTEM", "VR", "MICOM", "MODEM", "GPS"};
//static const char *update_folder_name = "/storage/upgrade/GEN5WIDE_KOR";
static const char *update_folder_path = "/storage/upgrade";
//static const char *model_name = "GEN5WIDE";
#define REC_FOTA_UPATE_SUCCESS          "success"
#define REC_FOTA_UPATE_FAIL             "fail"

#define REC_FOTA_RESULT_INFO            "/upgrade/ota/ota_result.cfg"

typedef struct get_folder_info{
    char folder_str[F_MAX];
    int cnt;
}GET_FOLDER_INFO;

typedef struct check_pt_pt_image{
    char *pt_name;
    char *pt_image_file;
}CHECK_PT_IMAGE;

static CHECK_PT_IMAGE write_pt_check[SYSTEM_UD_PT_SIZE] = {
    {{"boot"}       , {"boot.img"}},
    {{"system"}     , {"system.ext4"}},
    {{"recovery"}   , {"recovery.img"}},
    {{"vr1"}        , {"vr.img"}}
};

#endif // _RB_COMMON_H_

