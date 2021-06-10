#ifndef __RECOVERY_ERRNO__
#define __RECOVERY_ERRNO__
/*///////////////////////////////// update sw infomation error case */
#define REC_INFO_ERROR                0

#define REC_INFO_NOT_FOUND_PACKAGE_ERROR        (REC_INFO_ERROR + 1)
#define REC_INFO_SW_CARTYPE_ERROR               (REC_INFO_ERROR + 2)
#define REC_INFO_SW_REGION_ERROR                (REC_INFO_ERROR + 3)
#define REC_INFO_SW_HW_ERROR                    (REC_INFO_ERROR + 4)
#define REC_INFO_SW_MAJOR_VER_ERROR             (REC_INFO_ERROR + 5)
#define REC_INFO_SW_PATH_OPEN_ERROR             (REC_INFO_ERROR + 6)

/*///////////////////////////////// system partition update error case */
#define REC_SYSTEM_ERROR            100

#define REC_SYSTEM_QB_UPDATE_ERROR              (REC_SYSTEM_ERROR + 1)
#define REC_SYSTEM_SYS_UPDATE_ERROR             (REC_SYSTEM_ERROR + 2)
#define REC_SYSTEM_LK_UPDATE_ERROR              (REC_SYSTEM_ERROR + 3)
#define REC_SYSTEM_BOOT_UPDATE_ERROR            (REC_SYSTEM_ERROR + 4)
#define REC_SYSTEM_RECOVERY_UPDATE_ERROR        (REC_SYSTEM_ERROR + 5)
#define REC_SYSTEM_VR_UPDATE_ERROR              (REC_SYSTEM_ERROR + 6)
#define REC_SYSTEM_SPLASH_UPDATE_ERROR          (REC_SYSTEM_ERROR + 7)
#define REC_SYSTEM_SNAPSHOT_UPDATE_ERROR        (REC_SYSTEM_ERROR + 8)
#define REC_SYSTEM_UPDATE_ERROR                 (REC_SYSTEM_ERROR + 9)
#define REC_SYSTEM_INSTALL_CORRUPT              (REC_SYSTEM_ERROR + 10)
#define REC_SYSTEM_INSTALL_ERROR                (REC_SYSTEM_ERROR + 11)
#define REC_SYSTEM_PATH_OPEN_ERROR              (REC_SYSTEM_ERROR + 12)
#define REC_SYSTEM_FILE_SEEK_ERROR              (REC_SYSTEM_ERROR + 13)
#define REC_SYSTEM_FILE_WRITE_ERROR             (REC_SYSTEM_ERROR + 14)
#define REC_SYSTEM_VALIDATION_FILE_ERROR        (REC_SYSTEM_ERROR + 15)
#define REC_SYSTEM_VALIDATION_ERROR        		(REC_SYSTEM_ERROR + 16)
#define REC_SYSTEM_UNTAR_ERROR        			(REC_SYSTEM_ERROR + 17)

/*///////////////////////////////// GPS module update error case */
#define REC_GPS_ERROR               200

#define REC_GPS_INVALID_BINARY_ERROR            (REC_GPS_ERROR + 1)
#define REC_GPS_DEVICE_CONNECT_ERROR            (REC_GPS_ERROR + 2)
#define REC_GPS_VERSION_REQUEST_ERROR           (REC_GPS_ERROR + 3)
#define REC_GPS_SET_MONITOR_ERROR               (REC_GPS_ERROR + 4)
#define REC_GPS_FLASH_IDENTIFIER_ERROR          (REC_GPS_ERROR + 5)
#define REC_GPS_PACKET_SEND_ERROR               (REC_GPS_ERROR + 6)
#define REC_GPS_UART_CONNECT_ERROR              (REC_GPS_ERROR + 7)
#define REC_GPS_FWU_RESPONSE_ERROR              (REC_GPS_ERROR + 8)
#define REC_GPS_FLASHER_SYNK_ERROR              (REC_GPS_ERROR + 9)
#define REC_GPS_START_COMMUNICATION_ERROR       (REC_GPS_ERROR + 10)
#define REC_GPS_BINARY_OPTION_SEND_ERROR        (REC_GPS_ERROR + 11)
#define REC_GPS_FLASH_READY_ERROR               (REC_GPS_ERROR + 12)
#define REC_GPS_BINARY_SEND_ERROR               (REC_GPS_ERROR + 13)
#define REC_GPS_BINARY_SEND_DONE_ERROR          (REC_GPS_ERROR + 14)
#define REC_GPS_WRITE_ERROR                     (REC_GPS_ERROR + 15)
#define REC_GPS_CRC_ERROR                       (REC_GPS_ERROR + 16)
#define REC_GPS_POLL_VALUE_ERROR                (REC_GPS_ERROR + 17)
#define REC_GPS_ERROR_STATUS_RESPONSE_ERROR     (REC_GPS_ERROR + 18)
#define REC_GPS_NO_RESPONSE_ERROR               (REC_GPS_ERROR + 19)

/*///////////////////////////////// MODEM module update error case */
#define REC_MODEM_ERROR             300

#define REC_MODEM_DEVICE_SEARCH_ERROR           (REC_MODEM_ERROR + 1)
#define REC_MODEM_PORT_ERROR                    (REC_MODEM_ERROR + 2)
#define REC_MODEM_ERROR_COMMAND_ERROR           (REC_MODEM_ERROR + 3)
#define REC_MODEM_FILE_OPEN_ERROR               (REC_MODEM_ERROR + 4)
#define REC_MODEM_DOWNLOAD_ERROR                (REC_MODEM_ERROR + 5)
#define REC_MODEM_NO_BINARY_ERROR               (REC_MODEM_ERROR + 6)
#define REC_MODEM_ARGUMENT_COUNT_ERROR          (REC_MODEM_ERROR + 7)
#define REC_MODEM_TIMEOUT_RANGE_ERROR           (REC_MODEM_ERROR + 8)
#define REC_MODEM_TIMEOUT_ERROR                 (REC_MODEM_ERROR + 9)

/*///////////////////////////////// MICOM module update error case */
#define REC_MICOM_ERROR             400
#define REC_MICOM_DEVICE_SEARCH_ERROR           (REC_MICOM_ERROR + 1)
#define REC_MICOM_THREAD_CREATE_ERROR           (REC_MICOM_ERROR + 2)
#define REC_MICOM_PACKET_SEND_ERROR             (REC_MICOM_ERROR + 3)
#define REC_MICOM_ERROR_STATUS_RESPONSE_ERROR   (REC_MICOM_ERROR + 4)
#define REC_MICOM_DATA_FRIST_PACKET_ERROR       (REC_MICOM_ERROR + 5)
#define REC_MICOM_DATA_CONTINUOUS_PACKET_ERROR  (REC_MICOM_ERROR + 6)
#define REC_MICOM_FINISH_PACKET_ERROR           (REC_MICOM_ERROR + 7)
#define REC_MICOM_NO_RESPONSE_ERROR             (REC_MICOM_ERROR + 8)
#define REC_MICOM_UNKNOW_PACKET_RESPONSE_ERROR  (REC_MICOM_ERROR + 9)
#define REC_MICOM_ALL_DATA_SEND_ERROR           (REC_MICOM_ERROR + 10)
#define REC_MICOM_MON_ERROR                     (REC_MICOM_ERROR + 11)
#define REC_MICOM_AMP_ERROR                     (REC_MICOM_ERROR + 12)
#define REC_MICOM_HD_ERROR                      (REC_MICOM_ERROR + 13)
#define REC_MICOM_AUDIO_ERROR                   (REC_MICOM_ERROR + 14)
#define REC_MICOM_RADIO_ERROR                   (REC_MICOM_ERROR + 15)
#define REC_MICOM_DIMM_ERROR                    (REC_MICOM_ERROR + 16)
#define REC_MICOM_HERO_ERROR                    (REC_MICOM_ERROR + 17)
#define REC_MICOM_KEYBOARD_ERROR                (REC_MICOM_ERROR + 18)
#define REC_MICOM_EXCDP_ERROR                   (REC_MICOM_ERROR + 19)
#define REC_MICOM_MICOM_ERROR                   (REC_MICOM_ERROR + 20)
#define REC_MICOM_HIFI2_ERROR                   (REC_MICOM_ERROR + 21)
#define REC_MICOM_EPICS_ERROR                   (REC_MICOM_ERROR + 22)

/*///////////////////////////////// DAB module update error case */
#define REC_DAB_ERROR               500

/*///////////////////////////////// NAVI & VR module update error case */
#define REC_NAVI_VR_ERROR           600
#define REC_NAVI_VR_DIR_OPEN_ERROR              (REC_NAVI_VR_ERROR + 1)
#define REC_NAVI_VR_FILE_OPEN_ERROR             (REC_NAVI_VR_ERROR + 2)
#define REC_NAVI_VR_UNTAR_ERROR                 (REC_NAVI_VR_ERROR + 3)
#define REC_NAVI_VR_FILE_SIZE_READ_ERROR        (REC_NAVI_VR_ERROR + 4)
#define REC_NAVI_VR_FILE_CLOSE_ERROR            (REC_NAVI_VR_ERROR + 5)
#define REC_NAVI_VR_FILE_WRITE_ERROR            (REC_NAVI_VR_ERROR + 6)
#define REC_NAVI_VR_MD5_HASH_CHECK_ERROR        (REC_NAVI_VR_ERROR + 7)
#define REC_NAVI_VR_MD5_HASH_MISMATCH_ERROR     (REC_NAVI_VR_ERROR + 8)
#define REC_NAVI_VR_TAR_FILE_OPEN_ERROR         (REC_NAVI_VR_ERROR + 9)
#define REC_NAVI_VR_CAN_NOT_FOUND_ERROR         (REC_NAVI_VR_ERROR + 10)
#define REC_NAVI_VR_RESUME_FILE_OPEN_ERROR      (REC_NAVI_VR_ERROR + 11)
#define REC_NAVI_VR_RESTART_FILE_OPEN_ERROR     (REC_NAVI_VR_ERROR + 12)
#define REC_NAVI_VR_INFO_PARSING_ERROR          (REC_NAVI_VR_ERROR + 13)
#define REC_NAVI_VR_MEM_ALLOC_ERROR             (REC_NAVI_VR_ERROR + 14)
#define REC_NAVI_VR_IO_ERROR                    (REC_NAVI_VR_ERROR + 15)
#define REC_NAVI_VR_PATH_OPEN_ERROR             (REC_NAVI_VR_ERROR + 16)
#define REC_NAVI_VR_NOT_FOUND_FILE_ERROR        (REC_NAVI_VR_ERROR + 17)
#define REC_NAVI_VR_NOT_AVAIL_SPACE_ERROR       (REC_NAVI_VR_ERROR + 18)
#define REC_NAVI_VR_INCORRECT_REGION_ERROR      (REC_NAVI_VR_ERROR + 19)

/*///////////////////////////////// ETC update error case */
#define REC_OTHER_ERROR             900

#endif //__RECOVERY_ERRNO__
