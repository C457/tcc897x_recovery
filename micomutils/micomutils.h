#ifndef __MICOMUTILS_H__
#define __MICOMUTILS_H__

#define DEVICE_MON          0x0001
#define DEVICE_AMP          0x0002
#define DEVICE_HD           0x0004
#define DEVICE_AUDIO        0x0008
#define DEVICE_RADIO        0x0010
#define DEVICE_DIMM         0x0020
#define DEVICE_MICOM        0x0040
#define DEVICE_HERO         0x0080
#define DEVICE_KEYBOARD     0x0100
#define DEVICE_EXCDP        0x0200
#define DEVICE_EPICS        0x0400
#define DEVICE_HIFI2        0x0800
#define DEVICE_DRM          0x1000

#define NOTI_ID_MON         0x01
#define NOTI_ID_AMP         0x02
#define NOTI_ID_HD          0x03
#define NOTI_ID_AUDIO       0x04
#define NOTI_ID_RADIO       0x05
#define NOTI_ID_DIMM        0x06
#define NOTI_ID_HERO        0x07
#define NOTI_ID_KEYBOARD    0x08
#define NOTI_ID_EXCDP       0x09
#define NOTI_ID_MICOM       0x0B
#define NOTI_ID_HIFI2       0x0C
#define NOTI_ID_EPICS       0x0D
#define NOTI_ID_DRM         0x0E

#define MON_FILE            "/upgrade/micom/mon.bin"
#define AMP_FILE            "/upgrade/micom/amp.bin"
#define HD_FILE             "/upgrade/micom/hdradio.bin"
#define AUDIO_FILE          "/upgrade/micom/audio_map.bin"
#define RADIO_FILE          "/upgrade/micom/radio_map.bin"
#define DIMM_FILE           "/upgrade/micom/dimm.bin"
#define HERO_FILE           "/upgrade/micom/hero_module.bin"
#define KEYBOARD_FILE       "/upgrade/micom/keyboard_module.bin"
#define EXCDP_FILE          "/upgrade/micom/excdp_module.bin"
#define HIFI2_FILE          "/upgrade/micom/hifi2_module.bin"
#define EPICS_FILE          "/upgrade/micom/epics_module.bin"
#define MICOM_FILE          "/upgrade/micom/micom_sw.bin"
#define DRM_FILE            "/upgrade/micom/drm_module.bin"
#define NOTIFY_FILE         "/upgrade/updateStart.log"

enum {
    MICOM_STATUS_OK=0,

    // This value is set by callback function to report status.
    MICOM_STATUS_ERROR_RESP = 101,
    MICOM_STATUS_DATA_FRIST_PTK_ERROR ,
    MICOM_STATUS_DATA_CONTINUOUS_PTK_ERROR ,
    MICOM_STATUS_FINISH_PTK_ERROR ,
    MICOM_STATUS_NO_RESPONSE ,
    MICOM_STATUS_UNKNOW_PTK_RESP_ERROR ,
    MICOM_STATUS_ALL_DATA_SEND ,

    // error modules
    MICOM_STATUS_ERROR_MON = 1001,
    MICOM_STATUS_ERROR_AMP,
    MICOM_STATUS_ERROR_HD,
    MICOM_STATUS_ERROR_AUDIO,
    MICOM_STATUS_ERROR_RADIO,
    MICOM_STATUS_ERROR_DIMM,
    MICOM_STATUS_ERROR_HERO,
    MICOM_STATUS_ERROR_KEYBOARD,
    MICOM_STATUS_ERROR_EXCDP,
    MICOM_STATUS_ERROR_MICOM,
    MICOM_STATUS_ERROR_HIFI2,
    MICOM_STATUS_ERROR_EPICS,
    MICOM_STATUS_ERROR_DRM,
};

int micomConnect();
void micomDisConnect(int uFd);
void hardReset();
void cancelReservationUpdate();
int uComUpdateFunc();
int Update_Device_Notify_C_Req_Func(int fsize, int mDevice, int mDeviceNum, int mTurn, int uFd);
int Update_Data_Send_C_Req_Init(int uFd);
void update_Start_C_Func();
void update_Finish_C_Func();
int Update_Data_Send_C_Req_Func(int uFd);
void requestShutdown(void);

#endif //__MICOMUTILS_H__
