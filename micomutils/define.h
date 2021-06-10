#ifndef __DEFINE_H__
#define __DEFINE_H__

#define MICOM_DEVICE            "/dev/ttyTCC2"
#define MICOM_BAUDRATE          B115200

#define MAX_RETRY_COUNT         10
#define TIMEOUT                 480

#define OFFSET_SYN              0
#define OFFSET_SEQ              1
#define OFFSET_DEVID            2
#define OFFSET_LENGTH           3
#define OFFSET_FUNC             4
#define OFFSET_FUNC_LOW         5

#define OFFSET_SOF              0
#define OFFSET_SENDER_ID        1
#define OFFSET_RECEIVER_ID      2
#define OFFSET_TYPE_ID          3
#define OFFSET_FUNC_ID_HIGH     4
#define OFFSET_FUNC_ID_LOW      5
#define OFFSET_PAYLOAD_LEN_HIGH 6
#define OFFSET_PAYLOAD_LEN_LOW  7

#define HEADER_LENGTH           8
#define DATA_MAX_LENGTH         (263 + 1)

#define RECEIVE_PTK_LENGTH      30

#define UPDATE_BINARY_PTK_LEN   (HEADER_LENGTH + 256 + 1)
#define UPDATE_PKT_STATUS       8
#define UPDATE_PKT_FAIL_REASON  9

#define PKT_SIZE                			8
#define CS_OFFSET               			5
#define SEQ_RESET_PKT_SIZE      			4
#define UPDATE_NOTI_PKT_SIZE    			14
#define UPDATE_RESERVATION_PKT_SIZE         11
#define UPDATE_START_PKT_SIZE         		9
#define UPDATE_SHUTDOWN_PKT_SIZE      		8

//packet combination status
#define NOT_COMPLETED           0
#define COMPLETED               1

#define R_SEQ_RESET_MODE        0
#define R_BOOT_MODE             1
#define R_ACC_ON_MODE           2
#define R_STOP_RESET_MODE       3
#define R_START_RESET_MODE      4
#define SYS_DEV_NOTI_MODE       5
#define SYS_DEV_NOTI_END_MODE   6
#define UPDATE_DEVICE_NOTI_C    7
#define UPDATE_DATA_SEND_C      8
#define UPDATE_FINISH_C         9
#define SYS_UPDATE_START_MODE   10

#endif //__DEFINE_H__


