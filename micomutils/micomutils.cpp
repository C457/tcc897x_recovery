#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#include <sys/poll.h>
#include <termios.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/stat.h>

#include "define.h"
#include "micomutils.h"
#include "../ui.h"
#include "../screen_ui.h"

extern RecoveryUI* ui;

#define MICOM_DISCONNECTED      0
#define MICOM_CONNECTED         1
#define MICOM_DEBUG             0

#define UPDATE_BINARY_BUF_LEN   256

int nCounts;
int bin_data;
int gResult = 1;
int currentDevice = 0;
int status;
int running = 0;    //receive thread flag
int request_mode = R_SEQ_RESET_MODE;

long updateOffset;

unsigned char pBuffs[UPDATE_BINARY_PTK_LEN];

struct itimerval tout_val;

pthread_t receive_thd;
pthread_mutex_t mutex_lock;

FILE *fins = NULL;

const unsigned char SYN = 0xff;
const unsigned char DEV_ID = 0x8a;
const unsigned char PAYLOAD_LENGTH = 0x01;

const unsigned char SYS_BOOT_MODE_FNCODE_HIGH       = 0x81;
const unsigned char SYS_BOOT_MODE_FNCODE_LOW        = 0x01;

const unsigned char UPDATE_MICOM_M_RESP_FUNC_HIGH   = 0x86;
const unsigned char UPDATE_MICOM_M_RESP_FUNC_LOW    = 0x01;

const unsigned char UPDATE_RESP_ERROR               = 0x00;
const unsigned char UPDATE_RESP_OK                  = 0x01;
const unsigned char UPDATE_RESP_FINISH              = 0x02;

const unsigned char UPDATE_NOTI_FUNC_L_ID           = 0x02;
const unsigned char UPDATE_DATA_SEND_FUNC_L_ID      = 0x03;
const unsigned char UPDATE_FINISH_FUNC_L_ID         = 0x04;

const unsigned char UPDATE_DEVICE_NOTIFY_C_REQ[PKT_SIZE]   					= {0xff, 0x86, 0x06, 0x01, 0x06, 0x02, 0x01, 0x01};
const unsigned char UPDATE_DATA_SEND_C_REQ[PKT_SIZE]       					= {0xff, 0x86, 0x06, 0x01, 0x06, 0x03, 0x01, 0x01};
const unsigned char UPDATE_START_C_REQ[UPDATE_START_PKT_SIZE]  				= {0xff, 0x86, 0x06, 0x01, 0x06, 0x05, 0x00, 0x02, 0x01};
const unsigned char UPDATE_FINISH_C_REQ[UPDATE_START_PKT_SIZE] 				= {0xff, 0x86, 0x06, 0x01, 0x06, 0x05, 0x00, 0x02, 0x00};
const unsigned char SYS_RESET_PKT[PKT_SIZE]                					= {0xff, 0x8a, 0x01, 0x01, 0x01, 0x1c, 0x00, 0x01};
const unsigned char UPDATE_CANCEL_RESERVATION_C[UPDATE_RESERVATION_PKT_SIZE]= {0xff, 0x8A, 0x01, 0x01, 0x01, 0x57, 0x00, 0x04, 0x00, 0x00, 0x20};
const unsigned char UPDATE_SHUTDOWN_C[UPDATE_SHUTDOWN_PKT_SIZE]             = {0xff, 0x8A, 0x01, 0x01, 0x01, 0x58, 0x00, 0x01};

void printData(const char* msg ,unsigned char* data,int size)
{
    int i = 0;

    if(MICOM_DEBUG) {
        fprintf(stdout, "%s : ",msg);

        for(i = 0;i < size;i++) {
            fprintf(stdout, "0x%x, ",data[i]);

            if(i % 30 == 0 && i != 0)
                fprintf(stdout, "\n");
        }
        fprintf(stdout, "\n");
    }
}

void time_expired (int i)
{
    pthread_mutex_lock(&mutex_lock);
    running = MICOM_DISCONNECTED;
    gResult = -1;
    pthread_mutex_unlock(&mutex_lock);
    //timer_cancel();
    //signal(SIGALRM,alarm_wakeup);
    printf("Timer Expired\n");
}

void timer_cancel()
{
    /*
    tout_val.it_interval.tv_sec = 0;
    tout_val.it_interval.tv_usec = 0;
    tout_val.it_value.tv_sec = 0;
    tout_val.it_value.tv_usec = 0;

    setitimer(ITIMER_REAL, &tout_val,0);*/
    fprintf(stderr, "timer_cancel\n");
    signal(SIGALRM, SIG_IGN); /* set the Alarm signal capture */
}

void timer_start()
{
    fprintf(stderr, "timer_start\n");
    tout_val.it_interval.tv_sec = 0;
    tout_val.it_interval.tv_usec = 0;
    tout_val.it_value.tv_sec = TIMEOUT; /* set timer for "INTERVAL (30) seconds */
    tout_val.it_value.tv_usec = 0;
    setitimer(ITIMER_REAL, &tout_val,0);
    signal(SIGALRM, time_expired); /* set the Alarm signal capture */
}

void timer_set(int nTime)
{
    tout_val.it_interval.tv_sec = 0;
    tout_val.it_interval.tv_usec = 0;
    tout_val.it_value.tv_sec = nTime; /* set timer for "INTERVAL (30) seconds */
    tout_val.it_value.tv_usec = 0;
    setitimer(ITIMER_REAL, &tout_val,0);
    signal(SIGALRM, time_expired); /* set the Alarm signal capture */
}

int openDevice(void)
{
    int fd;
    struct termios tios;

    fd = open(MICOM_DEVICE, O_NOCTTY | O_RDWR);
    if(fd == -1) {
        fprintf(stderr, "device(%s) open failed errno[%d]\n", MICOM_DEVICE, errno);
        return -1;
    }

    if(tcgetattr(fd, &tios) != 0) {
        fprintf(stderr, "device(%s) tcgetattr failed\n", MICOM_DEVICE);
        close(fd);
        return -1;
    }

    cfmakeraw(&tios);
    if(cfsetospeed(&tios, MICOM_BAUDRATE) != 0) {
        fprintf(stderr, "device(%s) cfsetospeed failed\n", MICOM_DEVICE);
        close(fd);
        return -1;
    }

    if(cfsetispeed(&tios, MICOM_BAUDRATE) != 0) {
        fprintf(stderr, "device(%s) cfsetispeed failed\n", MICOM_DEVICE);
        close(fd);
        return -1;
    }

    if(tcsetattr(fd, TCSANOW, &tios) != 0) {
        fprintf(stderr, "device(%s) tcsetattr failed\n", MICOM_DEVICE);
        close(fd);
        return -1;
    }

    return fd;
}

void closeDevice(int fd)
{
    close(fd);
}

unsigned char CalcCheckSum(unsigned char *data,int nsize)
{
    int i, sum = 0;

    for (i = 0; i < nsize; i++) {
        sum ^= data[i];
    }

    return sum;
}

int sendPacket(int uFd, unsigned char *pkt, int size)
{
    int ret = 0;

    pkt[size - 1] = CalcCheckSum(pkt, size - 1);

    if(MICOM_DEBUG)
        printData("sendpacket", pkt, size);

    ret = write(uFd, pkt, size);
    if(0 >= ret) {
        fprintf(stderr, "packet send failed\n");
        return -1;
    }

    if(MICOM_DEBUG)
        fprintf(stdout, "write count = : %d\n", ret);

    return 0;
}

void *receive_data(void *fd)
{
    int i;
    int uFd = (int)fd;
    int cnt = 0;
    int check_result = 0;
    int poll_b_len = 0;

    unsigned char buf[RECEIVE_PTK_LENGTH];
    unsigned char poll_buf[RECEIVE_PTK_LENGTH];

    struct pollfd poll_events;
    int poll_state;
    int jj = 0;

    memset(buf, 0 , RECEIVE_PTK_LENGTH);
    memset(poll_buf, 0 , RECEIVE_PTK_LENGTH);
    poll_b_len = 0;

    fprintf(stdout, "read thread start! fd[%d]\n", uFd);

    poll_events.fd = uFd;
    poll_events.events = POLLIN | POLLERR;
    poll_events.revents = 0;

    while (1) {
        if(running == MICOM_DISCONNECTED){
            fprintf(stdout, "MICOM_DISCONNECTED in receive_data\n");
            break;
        }

        poll_state = poll((struct pollfd *)&poll_events, 1, 20000);
        if(0 < poll_state) {
            if(poll_events.revents & POLLIN) {
                memset(buf, 0 , RECEIVE_PTK_LENGTH);
                cnt = read(uFd, buf, RECEIVE_PTK_LENGTH);
                if(MICOM_DEBUG)
                    fprintf(stdout,"POLLIN event\n");

                if(cnt <= 0) {
                    fprintf(stderr, "read error\n");
                    pthread_mutex_lock(&mutex_lock);
                    gResult = MICOM_STATUS_ERROR_RESP;
                    pthread_mutex_unlock(&mutex_lock);
                    break;
                }

                if(cnt < 12)
                {
                    if(poll_b_len == 0) {
                        for(jj = 0; jj<cnt; jj++) {
                            poll_buf[jj] = buf[jj];
                        }
                        poll_b_len = cnt;
                    } else {
                        printData("poll_buf data", poll_buf,poll_b_len);

                        for(jj = 0; jj<cnt; jj++) {
                            poll_buf[poll_b_len + jj] = buf[jj];
                        }
                        poll_b_len += cnt;

                        if(poll_b_len == 12) {
                            for(jj = 0;jj < poll_b_len;jj++) {
                                buf[jj] = poll_buf[jj];
                            }
                        }

                        printData("Fixed buf data", buf,poll_b_len);
                    }
                }

                if(0 < poll_b_len && poll_b_len < 12) {
                    //fprintf(stdout,"poll_b_len : %d cnt : %d\n",poll_b_len,cnt);
                    printData("receive data", buf,cnt);
                } else if(buf[OFFSET_FUNC_ID_HIGH]== SYS_BOOT_MODE_FNCODE_HIGH
                        && buf[OFFSET_FUNC_ID_LOW] == SYS_BOOT_MODE_FNCODE_LOW) {
                    fprintf(stdout, "BOOT mode response!\n");
                    printData("receive data", buf, cnt);
                    break;
                } else if(buf[OFFSET_FUNC_ID_HIGH]== UPDATE_MICOM_M_RESP_FUNC_HIGH
                        && buf[OFFSET_FUNC_ID_LOW] == UPDATE_MICOM_M_RESP_FUNC_LOW) {
                    if(buf[8] == UPDATE_NOTI_FUNC_L_ID) {
                        if(buf[9] == UPDATE_RESP_OK) {
                            // 0xff,  0x6,  0x86,  0x1,  0x86,  0x1,  0x0,  0x4,  0x2,  0x1,  0x0,  0xfe
                            printData("receive data", buf, cnt);

                            if(MICOM_DEBUG)
                         	    fprintf(stdout, "update PKT OK : %d\n", currentDevice);

                            gResult = Update_Data_Send_C_Req_Init(uFd);
                            poll_b_len = 0; // init

                            if(gResult == MICOM_STATUS_DATA_FRIST_PTK_ERROR) {
                                fprintf(stdout, "update first data result error\n");
                                break;
                            }
                        } else {
                            gResult = MICOM_STATUS_ERROR_RESP;
                            fprintf(stdout, "NOTI_FUNC update PKT error ! \n");
                            printData("receive data", buf, cnt);
                            break;
                        }
                    } else if(buf[8] == UPDATE_DATA_SEND_FUNC_L_ID) {
                        if(buf[9] == UPDATE_RESP_OK) {
                            gResult = Update_Data_Send_C_Req_Func(uFd);

                            if(MICOM_DEBUG)
                                fprintf(stdout,"update PKT OK : %d\n", gResult);

                            poll_b_len = 0; // init

                            if(gResult == MICOM_STATUS_ALL_DATA_SEND) {
                                if(MICOM_DEBUG)
                                    fprintf(stdout," update PKT OK end\n");
                            } else if(gResult == MICOM_STATUS_DATA_CONTINUOUS_PTK_ERROR) {
                                fprintf(stdout,"RESP_OK update PKT result -1 \n");
                                printData("receive data", buf, cnt);
                                break;
                            }
                        } else if(buf[9] == UPDATE_RESP_FINISH) {
                            fprintf(stdout, "We sent all data. \n");
                            fprintf(stdout, "received finish message from the Micom \n");
                            gResult = MICOM_STATUS_OK;
                            break;
                        } else {
                            gResult = MICOM_STATUS_ERROR_RESP;
                            fprintf(stdout, "RESP_FINISH update PKT error ! \n");
                            printData("receive data", buf, cnt);
                            break;
                        }
                    }else if(buf[8] == UPDATE_FINISH_FUNC_L_ID) {
                        if(buf[9] == UPDATE_RESP_OK) {
                            gResult = MICOM_STATUS_OK;
                            if(MICOM_DEBUG)
                                fprintf(stdout,"update PKT OK \n");
                        } else {
                            gResult = MICOM_STATUS_ERROR_RESP;
                            fprintf(stdout, "FINISH update PKT error!\n");
                            printData("receive data", buf, cnt);
                        }
                        break;
                    } else {
                        printData("receive data", buf,cnt);
                        gResult = MICOM_STATUS_UNKNOW_PTK_RESP_ERROR;
                    }
                } else {
                    fprintf(stdout,"Unknown response ~\n FN_H:%d FN_L:%d\n",
                        buf[OFFSET_FUNC_ID_HIGH], buf[OFFSET_FUNC_ID_LOW]);

                    for(check_result = 0;check_result < cnt;check_result++) {
                        fprintf(stdout, " 0x%x, ", buf[check_result]);
                    }

                    fprintf(stdout, "\n ");
                    gResult = MICOM_STATUS_UNKNOW_PTK_RESP_ERROR;
                    break;
                }
            }

            if(poll_events.revents & POLLERR) {
                fprintf(stderr, "serial error\n");
                pthread_mutex_lock(&mutex_lock);
                gResult = MICOM_STATUS_ERROR_RESP;
                pthread_mutex_unlock(&mutex_lock);
                break;
            }
        } else if(poll_state < 0) {
            fprintf(stdout, "polling failed\n");

            pthread_mutex_lock(&mutex_lock);
            gResult = MICOM_STATUS_ERROR_RESP;
            pthread_mutex_unlock(&mutex_lock);
            break;
        } else if (poll_state == 0) {
            fprintf(stdout,"poll state is 0\n");

            pthread_mutex_lock(&mutex_lock);
            gResult = MICOM_STATUS_NO_RESPONSE;
            pthread_mutex_unlock(&mutex_lock);
            break;
        }
    }

    //close(poll_events.fd);
    timer_cancel();

	if(MICOM_DEBUG)
    	fprintf(stdout, "read thread end!\n");

    return NULL;
}

/**
 * Description : Micom update thread start API
 * @param no : None
 * @return : A return code of -1 means fail, and  1 means success.
 * @see    : None
 */
int threadStart(int uFd)
{
    int thread_id;

    fprintf(stdout, "-threadStart-\n");
    running = MICOM_CONNECTED;

    fprintf(stdout, "%s : ufd value [%d]\n", __func__, uFd);

    pthread_mutex_init(&mutex_lock, NULL);
    thread_id = pthread_create(&receive_thd, NULL, receive_data, (void *)(uFd));

    if(thread_id < 0) {
        fprintf(stderr, "receive data thread create failed\n");
        closeDevice(uFd);
        return -1;
    }

    return 0;
}

int micomConnect() {
    int fd;
    int cur_retry_cnt = 0;

    while(cur_retry_cnt < MAX_RETRY_COUNT) {
        if((fd = openDevice()) > -1) {
            cur_retry_cnt = 0;
            break;
        }

        cur_retry_cnt++;
        usleep(500);
        fprintf(stderr, "device open retry count[%d] \n",cur_retry_cnt);
    }

    if(cur_retry_cnt == MAX_RETRY_COUNT) {
        fprintf(stderr, "micom device open failed\n");
        closeDevice(fd);

        return -1;
    }

    fprintf(stdout, "micom connected\n");

    return fd;
}

void micomDisConnect(int uFd) {
    running = MICOM_DISCONNECTED;
    closeDevice(uFd);

    if(fins != NULL)
      fclose(fins);

    fprintf(stdout, "micom disconnected\n");
}

 /**
 * Description : wait and return receivce thread result
 * @param no : None
 * @return : receive thread result
 * @see    : None
 */
int wait_listen() {
    int status;

    fprintf(stdout, "wait_listen\n");

    timer_start();

    pthread_join(receive_thd, (void **) &status);
    pthread_mutex_destroy(&mutex_lock);

    fprintf(stderr, "-threadend-\n");
    fprintf(stdout, "status : %d result : %d\n", status, gResult);

    status = gResult;

    return status;
}

 /**
 * Description : Send update request packet to Micom
 * @param no : filesize, update device, total device num, update count, uart device file description
 * @return : packet send result
 * @see    : None
 */
int Update_Device_Notify_C_Req_Func(int fsize, int mDevice, int mDeviceNum, int mTurn, int uFd)
{
    unsigned char data[DATA_MAX_LENGTH + 1];

    memset(data, 0, DATA_MAX_LENGTH + 1);
    fprintf(stdout, "Update_Device_Notify_C_Req_Func\n");

    if(threadStart(uFd) < 0) {
        fprintf(stderr, "recv thread start failed!\n");
        return -1;
    }

    fprintf(stdout, "file size : %d, turn : %d, mDeviceNum : %d\n", fsize, mTurn, mDeviceNum);
    bin_data = fsize;
    currentDevice = mDevice;
    request_mode = SYS_DEV_NOTI_MODE;

    // packet initialize
    memcpy(data, UPDATE_DEVICE_NOTIFY_C_REQ, sizeof(unsigned char) * HEADER_LENGTH);

    data[8] = mDeviceNum;   // update device num
    data[9] = mTurn;        // update count
    data[10] = mDevice;     // update device
    data[13] = (fsize & 0xff);
    data[12] = ((fsize >> 8) & 0xff);
    data[11] = ((fsize >> 16) & 0xff); // update file size

    fprintf(stderr, "11 : 0x%02X, 12 : 0x%02X , 13 : 0x%02X\n", data[11], data[12], data[13]);

    if(sendPacket(uFd, data, DATA_MAX_LENGTH + 1) < 0) {
        fprintf(stderr, "UPDATE_DEVICE_NOTIFY_C_REQ SENDING FAILED!!\n");
        micomDisConnect(uFd);
        return -1;
    }

    if(mDevice == NOTI_ID_MICOM) {
        fprintf(stdout, "make update notify file\n");

        FILE *finz = fopen(NOTIFY_FILE, "w");
        if (finz == NULL) {
            fprintf(stderr, "finz == null\n");
        }

        fclose(finz);
    }

    return wait_listen();
}

int Update_Data_Send_C_Req_Init(int uFd)
{
    int ret = 0;

    if(currentDevice == NOTI_ID_MON)
        fins = fopen(MON_FILE, "rb");
    else if(currentDevice == NOTI_ID_AMP)
        fins = fopen(AMP_FILE, "rb");
    else if(currentDevice == NOTI_ID_HD)
        fins = fopen(HD_FILE, "rb");
    else if(currentDevice == NOTI_ID_AUDIO)
        fins = fopen(AUDIO_FILE, "rb");
    else if(currentDevice == NOTI_ID_RADIO)
        fins = fopen(RADIO_FILE, "rb");
    else if(currentDevice == NOTI_ID_DIMM)
        fins = fopen(DIMM_FILE, "rb");
    else if(currentDevice == NOTI_ID_HERO)
        fins = fopen(HERO_FILE, "rb");
    else if(currentDevice == NOTI_ID_KEYBOARD)
        fins = fopen(KEYBOARD_FILE, "rb");
    else if(currentDevice == NOTI_ID_EXCDP)
        fins = fopen(EXCDP_FILE, "rb");
    else if(currentDevice == NOTI_ID_HIFI2)
        fins = fopen(HIFI2_FILE, "rb");
    else if(currentDevice == NOTI_ID_EPICS)
        fins = fopen(EPICS_FILE, "rb");
    else if(currentDevice == NOTI_ID_DRM)
        fins = fopen(DRM_FILE, "rb");
    else if(currentDevice == NOTI_ID_MICOM)
        fins = fopen(MICOM_FILE, "rb");

    if (fins == NULL) {
        fprintf(stderr, "input file() open failed\n");
        return MICOM_STATUS_DATA_FRIST_PTK_ERROR;
    }

    nCounts = 0;
    updateOffset = 0;

    memset(pBuffs, 0, UPDATE_BINARY_PTK_LEN);
    memcpy(pBuffs, UPDATE_DATA_SEND_C_REQ, sizeof(unsigned char) * HEADER_LENGTH);

    ret = Update_Data_Send_C_Req_Func(uFd);

    return ret;
}

int Update_Data_Send_C_Req_Func(int uFd)
{
    int ret;
    int readCnt;
    int writeCnt;

    unsigned char buffers[UPDATE_BINARY_BUF_LEN];

    updateOffset = nCounts * UPDATE_BINARY_BUF_LEN;

    if(MICOM_DEBUG)
        fprintf(stdout," offset:%ld, nCount:%d \n", updateOffset, nCounts);

    if(bin_data < updateOffset) {
        fprintf(stderr, "END data bin:%d\n",bin_data);
        return MICOM_STATUS_ALL_DATA_SEND;
    }

    ret = fseek(fins, updateOffset, SEEK_SET);
    if(ret < 0) {
        fprintf(stderr, "input file seek failed\n");
        return MICOM_STATUS_DATA_CONTINUOUS_PTK_ERROR;
    }

    readCnt = fread(buffers, 1, UPDATE_BINARY_BUF_LEN, fins);
    if(ferror(fins)) {
        fprintf(stderr, "input file read error ()\n");
        return MICOM_STATUS_DATA_CONTINUOUS_PTK_ERROR;
    }

    if(readCnt < 0) {
        fprintf(stderr, "input file failed\n");
        return MICOM_STATUS_DATA_CONTINUOUS_PTK_ERROR;
    }

    if(readCnt != 0) {
        memset(pBuffs + HEADER_LENGTH, 0, UPDATE_BINARY_PTK_LEN - HEADER_LENGTH);
        memcpy(pBuffs + HEADER_LENGTH, buffers, sizeof(unsigned char) * readCnt);
        pBuffs[UPDATE_BINARY_PTK_LEN - 1] = CalcCheckSum(pBuffs, UPDATE_BINARY_PTK_LEN - 1);

        if(MICOM_DEBUG) {
            printData("sendpacket", pBuffs, UPDATE_BINARY_PTK_LEN);
        }

        if((writeCnt = write(uFd, pBuffs, UPDATE_BINARY_PTK_LEN)) <= 0) {
            fprintf(stderr, " output file write failed\n");
            return MICOM_STATUS_DATA_CONTINUOUS_PTK_ERROR;
        }
        nCounts++;
    }

    return MICOM_STATUS_OK;
}

 /**
 * Description : Draw update label
 * @param no : update binary type
 * @return : None
 * @see    : None
 */
void drawLabel(int mType)
{
    ui->Print("drawLabel : %d \n",mType);
    ui->countPresent++;
	//ui->SetStatusMessageHeight(MESSAGE_HEIGHT_DEFAULT_DY);
	ui->ui_set_status_message(RecoveryUI::INSTALLING_MESSAGE);
    switch(mType){
        case DEVICE_MON:
            ui->SetUpdateLableType(RecoveryUI::CUR_UPDATE_LABLE_TYPE_MONITOR);
            ui->SetProgressType(RecoveryUI::DETERMINATE);
            ui->ShowProgress(1.0, 30);
            break;
        case DEVICE_AMP:
            ui->SetUpdateLableType(RecoveryUI::CUR_UPDATE_LABLE_TYPE_AMP);
            ui->SetProgressType(RecoveryUI::DETERMINATE);
            ui->ShowProgress(1.0, 30);
            break;
        case DEVICE_HD:
            ui->SetUpdateLableType(RecoveryUI::CUR_UPDATE_LABLE_TYPE_HD);
            ui->SetProgressType(RecoveryUI::DETERMINATE);
            ui->ShowProgress(1.0, 330);
            break;
        case DEVICE_EXCDP:
            ui->SetUpdateLableType(RecoveryUI::CUR_UPDATE_LABLE_TYPE_CDP);
            ui->SetProgressType(RecoveryUI::DETERMINATE);
            ui->ShowProgress(1.0, 120);
            break;
        case DEVICE_KEYBOARD:
            ui->SetUpdateLableType(RecoveryUI::CUR_UPDATE_LABLE_TYPE_KEYBOARD);
            ui->SetProgressType(RecoveryUI::DETERMINATE);
            ui->ShowProgress(1.0, 120);
            break;
        case DEVICE_HIFI2:
            ui->SetUpdateLableType(RecoveryUI::CUR_UPDATE_LABLE_TYPE_HIFI2);
            ui->SetProgressType(RecoveryUI::DETERMINATE);
            ui->ShowProgress(1.0, 30);
            break;
        case DEVICE_EPICS:
            ui->SetUpdateLableType(RecoveryUI::CUR_UPDATE_LABLE_TYPE_EPICS);
            ui->SetProgressType(RecoveryUI::DETERMINATE);
            ui->ShowProgress(1.0, 30);
            break;
        case DEVICE_DRM:
            ui->SetUpdateLableType(RecoveryUI::CUR_UPDATE_LABLE_TYPE_DRM);
            ui->SetProgressType(RecoveryUI::DETERMINATE);
            ui->ShowProgress(1.0, 60);
            break;
        default:
            ui->SetUpdateLableType(RecoveryUI::CUR_UPDATE_LABLE_TYPE_MICOM);
            ui->SetProgressType(RecoveryUI::DETERMINATE);
            ui->ShowProgress(1.0, 180);
            break;
    }
    sleep(1);
}

 /**
 * Description : Draw error label
 * @param no : update binary type
 * @return : None
 * @see    : None
 */
void drawLabelError(int mType)
{
    ui->Print("drawLabelError : %d \n",mType);

    switch(mType) {
        case DEVICE_MON:
            ui->SetUpdateLableType(RecoveryUI::CUR_UPDATE_LABLE_TYPE_MONITOR_ERROR);
            break;
        case DEVICE_AMP:
            ui->SetUpdateLableType(RecoveryUI::CUR_UPDATE_LABLE_TYPE_AMP_ERROR);
            break;
        case DEVICE_HD:
            ui->SetUpdateLableType(RecoveryUI::CUR_UPDATE_LABLE_TYPE_HD_ERROR);
            break;
        case DEVICE_EXCDP:
            ui->SetUpdateLableType(RecoveryUI::CUR_UPDATE_LABLE_TYPE_CDP_ERROR);
            break;
        case DEVICE_KEYBOARD:
            ui->SetUpdateLableType(RecoveryUI::CUR_UPDATE_LABLE_TYPE_KEYBOARD_ERROR);
            break;
        default:
            ui->SetUpdateLableType(RecoveryUI::CUR_UPDATE_LABLE_TYPE_MICOM_ERROR);
            break;
    }
    ui->ui_set_status_message(RecoveryUI::INSATLL_ERROR_MESSAGE);
    sleep(1);
}

int micomUpdateFunc(int mTurn, int mDevice, int mNum, int uFd)
{
    struct stat sd;
    int micom_result = 0;

    if(mDevice & DEVICE_KEYBOARD)
    {
        stat(KEYBOARD_FILE,&sd);
        ui->Print("keyboard(copied)file size : %d, %d, %d\n", sd.st_size, mNum, mTurn);
        micom_result = Update_Device_Notify_C_Req_Func(sd.st_size, NOTI_ID_KEYBOARD, mNum, mTurn, uFd);

        if(micom_result == MICOM_STATUS_OK)
            remove(KEYBOARD_FILE);
        else
            drawLabelError(DEVICE_KEYBOARD);
    }
    else if(mDevice & DEVICE_EXCDP)
    {
        stat(EXCDP_FILE,&sd);
            ui->Print("EXCDP(copied)file size : %d, %d, %d\n",sd.st_size, mNum, mTurn);
        micom_result = Update_Device_Notify_C_Req_Func(sd.st_size, NOTI_ID_EXCDP, mNum, mTurn, uFd);

        if(micom_result == MICOM_STATUS_OK)
            remove(EXCDP_FILE);
        else
            drawLabelError(DEVICE_EXCDP);
    }
    else if(mDevice & DEVICE_MON)
    {
        stat(MON_FILE,&sd);
        ui->Print("mon(copied)file size : %d, %d, %d\n",sd.st_size, mNum, mTurn);
        micom_result = Update_Device_Notify_C_Req_Func(sd.st_size, NOTI_ID_MON, mNum, mTurn, uFd);

        if(micom_result == MICOM_STATUS_OK)
            remove(MON_FILE);
        else
            drawLabelError(DEVICE_MON);
    }
    else if(mDevice & DEVICE_AMP)
    {
        stat(AMP_FILE,&sd);
        ui->Print("amp(copied)file size : %d, %d, %d\n",sd.st_size, mNum, mTurn);
        micom_result = Update_Device_Notify_C_Req_Func(sd.st_size, NOTI_ID_AMP, mNum, mTurn, uFd);

        if(micom_result == MICOM_STATUS_OK)
            remove(AMP_FILE);
        else
            drawLabelError(DEVICE_AMP);
    }
    else if(mDevice & DEVICE_HD)
    {
        stat(HD_FILE,&sd);
        ui->Print("hd(copied)file size : %d, %d, %d\n", sd.st_size, mNum, mTurn);
        micom_result = Update_Device_Notify_C_Req_Func(sd.st_size, NOTI_ID_HD, mNum, mTurn, uFd);

        if(micom_result == MICOM_STATUS_OK)
            remove(HD_FILE);
        else
            drawLabelError(DEVICE_HD);
    }

    // Below - tuning data.
    else if(mDevice & DEVICE_AUDIO)
    {
        stat(AUDIO_FILE, &sd);
        ui->Print("audio(copied)file size : %d, %d, %d\n", sd.st_size, mNum, mTurn);
        micom_result = Update_Device_Notify_C_Req_Func(sd.st_size, NOTI_ID_AUDIO, mNum, mTurn, uFd);

        if(micom_result == MICOM_STATUS_OK)
            remove(AUDIO_FILE);
        else
            drawLabelError(DEVICE_AUDIO);
    }
    else if(mDevice & DEVICE_RADIO)
    {
        stat(RADIO_FILE, &sd);
        ui->Print("radio(copied)file size : %d, %d, %d\n",sd.st_size, mNum, mTurn);
        micom_result = Update_Device_Notify_C_Req_Func(sd.st_size, NOTI_ID_RADIO, mNum, mTurn, uFd);

        if(micom_result == MICOM_STATUS_OK)
            remove(RADIO_FILE);
        else
            drawLabelError(DEVICE_RADIO);
    }
    else if(mDevice & DEVICE_DIMM)
    {
        stat(DIMM_FILE, &sd);
        ui->Print("dimm(copied)file size : %d, %d, %d\n", sd.st_size, mNum, mTurn);
        micom_result = Update_Device_Notify_C_Req_Func(sd.st_size, NOTI_ID_DIMM, mNum, mTurn, uFd);

        if(micom_result == MICOM_STATUS_OK)
            remove(DIMM_FILE);
        else
            drawLabelError(DEVICE_DIMM);
    }
    else if(mDevice & DEVICE_HERO)
    {
        stat(HERO_FILE, &sd);
        ui->Print("hero(copied)file size : %d, %d, %d\n", sd.st_size, mNum, mTurn);
        micom_result = Update_Device_Notify_C_Req_Func(sd.st_size, NOTI_ID_HERO, mNum, mTurn, uFd);

        if(micom_result == MICOM_STATUS_OK)
            remove(HERO_FILE);
        else
            drawLabelError(DEVICE_HERO);
    }
    else if(mDevice & DEVICE_EPICS)
    {
        stat(EPICS_FILE, &sd);
        ui->Print("EPICS file size : %d, %d, %d\n", sd.st_size, mNum, mTurn);
        micom_result = Update_Device_Notify_C_Req_Func(sd.st_size, NOTI_ID_EPICS, mNum, mTurn, uFd);

        if(micom_result == MICOM_STATUS_OK)
            remove(EPICS_FILE);
        else
            drawLabelError(DEVICE_EPICS);
    }
    else if(mDevice & DEVICE_HIFI2)
    {
        stat(HIFI2_FILE, &sd);
        ui->Print("HIFI2 ile size : %d, %d, %d\n", sd.st_size, mNum, mTurn);
        micom_result = Update_Device_Notify_C_Req_Func(sd.st_size, NOTI_ID_HIFI2, mNum, mTurn, uFd);

        if(micom_result == MICOM_STATUS_OK)
            remove(HIFI2_FILE);
        else
            drawLabelError(DEVICE_HIFI2);
    }
    else if(mDevice & DEVICE_DRM)
    {
        stat(DRM_FILE, &sd);
        ui->Print("DRM ile size : %d, %d, %d\n", sd.st_size, mNum, mTurn);
        micom_result = Update_Device_Notify_C_Req_Func(sd.st_size, NOTI_ID_DRM, mNum, mTurn, uFd);

        if(micom_result == MICOM_STATUS_OK)
            remove(DRM_FILE);
        else
            drawLabelError(DEVICE_DRM);
    }
    else if(mDevice & DEVICE_MICOM)
    {
        stat(MICOM_FILE, &sd);
        ui->Print("micom(copied)file size : %d, %d, %d\n", sd.st_size, mNum, mTurn);
        micom_result = Update_Device_Notify_C_Req_Func(sd.st_size, NOTI_ID_MICOM, mNum, mTurn, uFd);

        if(micom_result == MICOM_STATUS_OK) {
            remove(MICOM_FILE);
			sync();
		}
        else
            drawLabelError(DEVICE_MICOM);
    }

    if(micom_result == MICOM_STATUS_OK)
    {
        remove(NOTIFY_FILE);
        ui->Print("Micom update successed\n");
        sleep(3);
    }
    else
    {
        switch(micom_result) {
            case MICOM_STATUS_ERROR_RESP:
                ui->Print("Micom STATUS_ERROR_RESP\n");
                break;
            case MICOM_STATUS_DATA_FRIST_PTK_ERROR:
                ui->Print("Micom DATA_FRIST_PTK ERROR\n");
                break;
            case MICOM_STATUS_DATA_CONTINUOUS_PTK_ERROR:
                ui->Print("Micom MICOM_STATUS_DATA_CONTINUOUS_PTK_ERROR\n");
                break;
            case MICOM_STATUS_FINISH_PTK_ERROR:
                ui->Print("Micom MICOM_STATUS_FINISH_PTK_ERROR\n");
                break;
            case MICOM_STATUS_NO_RESPONSE:
                ui->Print("Micom MICOM_STATUS_NO_RESPONSE\n");
                break;
            case MICOM_STATUS_UNKNOW_PTK_RESP_ERROR:
                ui->Print("Micom MICOM_STATUS_UNKNOW_PTK_RESP_ERROR\n");
                break;
            case MICOM_STATUS_ALL_DATA_SEND:
                ui->Print("Micom MICOM_STATUS_ALL_DATA_SEND\n");
                break;
        }

        ui->Print("Retry micom update : %d\n", currentDevice);
        switch(currentDevice) {
            case NOTI_ID_MON:
                return MICOM_STATUS_ERROR_MON;
            case NOTI_ID_AMP:
                return MICOM_STATUS_ERROR_AMP;
            case NOTI_ID_HD:
                return MICOM_STATUS_ERROR_HD;
            case NOTI_ID_AUDIO:
                return MICOM_STATUS_ERROR_AUDIO;
            case NOTI_ID_RADIO:
                return MICOM_STATUS_ERROR_RADIO;
            case NOTI_ID_DIMM:
                return MICOM_STATUS_ERROR_DIMM;
            case NOTI_ID_MICOM:
                return MICOM_STATUS_ERROR_MICOM;
            case NOTI_ID_HERO:
                return MICOM_STATUS_ERROR_HERO;
            case NOTI_ID_KEYBOARD:
                return MICOM_STATUS_ERROR_KEYBOARD;
            case NOTI_ID_EXCDP:
                return MICOM_STATUS_ERROR_EXCDP;
            case NOTI_ID_HIFI2:
                return MICOM_STATUS_ERROR_HIFI2;
            case NOTI_ID_EPICS:
                return MICOM_STATUS_ERROR_EPICS;
            case NOTI_ID_DRM:
                return MICOM_STATUS_ERROR_DRM;
        }
    }
    return micom_result;
}

 /**
 * Description : Micom update update start
 * @param no : None
 * @return : update result value
 * @see    : None
 */
int uComUpdateFunc()
{
    int uFd;        // Uart File description
    int nRoof = 0;
    int tempList = 0;
    int micomList = 0;
    int micomTotalNums = 0;
    int _micom_result = MICOM_STATUS_OK;
    int thread_id;

    if(access(MON_FILE, F_OK) == 0)
        micomList |= DEVICE_MON;
    if(access(AMP_FILE, F_OK) == 0)
        micomList |= DEVICE_AMP;
    if(access(HD_FILE, F_OK) == 0)
        micomList |= DEVICE_HD;
    if(access(AUDIO_FILE, F_OK) == 0)
        micomList |= DEVICE_AUDIO;
    if(access(RADIO_FILE, F_OK) == 0)
        micomList |= DEVICE_RADIO;
    if(access(DIMM_FILE, F_OK) == 0)
        micomList |= DEVICE_DIMM;
    if(access(HERO_FILE, F_OK) == 0)
        micomList |= DEVICE_HERO;
    if(access(KEYBOARD_FILE, F_OK) == 0)
        micomList |= DEVICE_KEYBOARD;
    if(access(EXCDP_FILE,F_OK) == 0)
        micomList |= DEVICE_EXCDP;
    if(access(EPICS_FILE,F_OK) == 0)
        micomList |= DEVICE_EPICS;
    if(access(HIFI2_FILE,F_OK) == 0)
        micomList |= DEVICE_HIFI2;
    if(access(DRM_FILE,F_OK) == 0)
        micomList |= DEVICE_DRM;
    if(access(MICOM_FILE,F_OK) ==0)
        micomList |= DEVICE_MICOM;
    printf("Micom : micom update list : 0x%X\n", micomList);

    if(micomList == 0) {
        fprintf(stdout, "Micom : not found update binary\n");
        return _micom_result;
    }

    nRoof = 0;
    tempList = micomList;
    for(micomTotalNums = 0;tempList;micomTotalNums++) {
        tempList &= tempList - 1;
    }

    uFd = micomConnect();
    if(0 > uFd) {
        fprintf(stderr, "Micom : device connect failed\n");
        micomDisConnect(uFd);
        return -1;
    }

    if(micomList & DEVICE_HD) {
        sleep(2);
        drawLabel(DEVICE_HD);
        _micom_result += micomUpdateFunc(++nRoof, micomList, micomTotalNums, uFd);
        micomList &= ~DEVICE_HD;
    }

    if(micomList & DEVICE_KEYBOARD) {
        sleep(2);
        drawLabel(DEVICE_KEYBOARD);
        _micom_result += micomUpdateFunc(++nRoof, micomList, micomTotalNums, uFd);
        micomList &= ~DEVICE_KEYBOARD;
    }

    if(micomList & DEVICE_EXCDP) {
        sleep(2);
        drawLabel(DEVICE_EXCDP);
        _micom_result += micomUpdateFunc(++nRoof, micomList, micomTotalNums, uFd);
        micomList &= ~DEVICE_EXCDP;
    }

    if(micomList & DEVICE_EPICS) {
        sleep(2);
        drawLabel(DEVICE_EPICS);
        _micom_result += micomUpdateFunc(++nRoof, micomList, micomTotalNums, uFd);
        micomList &= ~DEVICE_EPICS;
    }

    if(micomList & DEVICE_HIFI2) {
        sleep(2);
        drawLabel(DEVICE_HIFI2);
        _micom_result += micomUpdateFunc(++nRoof, micomList, micomTotalNums, uFd);
        micomList &= ~DEVICE_HIFI2;
    }

    if(micomList & DEVICE_DRM) {
        sleep(2);
        drawLabel(DEVICE_DRM);
        _micom_result += micomUpdateFunc(++nRoof, micomList, micomTotalNums, uFd);
        micomList &= ~DEVICE_DRM;
    }

    if(micomList != 0x00) {
        sleep(2);
        drawLabel(DEVICE_MICOM);

        for(;nRoof < micomTotalNums;nRoof++)
        {
            _micom_result += micomUpdateFunc((nRoof + 1), micomList, micomTotalNums, uFd);

            if(micomList & DEVICE_MON)
                micomList &= ~DEVICE_MON;
            else if(micomList & DEVICE_AMP)
                micomList &= ~DEVICE_AMP;
            else if(micomList & DEVICE_AUDIO)
                micomList &= ~DEVICE_AUDIO;
            else if(micomList & DEVICE_RADIO)
                micomList &= ~DEVICE_RADIO;
            else if(micomList & DEVICE_DIMM)
                micomList &= ~DEVICE_DIMM;
            else if(micomList & DEVICE_MICOM)
                micomList &= ~DEVICE_MICOM;
            else if(micomList & DEVICE_HERO)
                micomList &= ~DEVICE_HERO;
            else if(micomList & DEVICE_EPICS)
                micomList &= ~DEVICE_EPICS;
            else if(micomList & DEVICE_HIFI2)
                micomList &= ~DEVICE_HIFI2;
            else if(micomList & DEVICE_DRM)
                micomList &= ~DEVICE_DRM;
            else if(micomList & DEVICE_KEYBOARD)
                micomList &= ~DEVICE_KEYBOARD;
        }

        if(_micom_result == MICOM_STATUS_OK) {
            ui->SetProgress(1);
        }
    }

    micomDisConnect(uFd);

    return _micom_result;
}

 /**
 * Description : Micom update init packet send API
 * @param no : None
 * @return : None
 * @see    : None
 */
void update_Start_C_Func()
{
    int fd;
    int cur_retry_cnt = 0;
    int mresult = 0;
    unsigned char pBuff[UPDATE_START_PKT_SIZE + 1];

    fprintf(stdout, "update_Start_C_Func\n");

    while(cur_retry_cnt < MAX_RETRY_COUNT) {
        fd = openDevice();

        if(0 < fd) {
            cur_retry_cnt = 0;
            break;
        }

        cur_retry_cnt++;
        usleep(500);
        fprintf(stdout, "uart2 device open retry count[%d] \n",cur_retry_cnt);
    }

    if(cur_retry_cnt >= MAX_RETRY_COUNT) {
        fprintf(stderr, "uart2 device open failed\n");
        return ;
    }

    memset(pBuff, 0, UPDATE_START_PKT_SIZE + 1);
    memcpy(pBuff, UPDATE_START_C_REQ, UPDATE_START_PKT_SIZE);

    while(cur_retry_cnt < MAX_RETRY_COUNT) {
        mresult = sendPacket(fd, pBuff, UPDATE_START_PKT_SIZE + 1);	// HEADER_LENGTH = PKT_SIZE = 8
        if(mresult == 0) {
            cur_retry_cnt = 0;
            break;
        }

        cur_retry_cnt++;
        sleep(1);
        fprintf(stderr, "SYS_UPDATE_START_MODE SENDING FAILED!!/n retry count[%d] \n",cur_retry_cnt);
    }

    if(cur_retry_cnt >= MAX_RETRY_COUNT) {
        fprintf(stderr, "uart2 device packet send failed\n");
        closeDevice(fd);
        return ;
    }

    closeDevice(fd);
}

 /**
 * Description : update finish
 * @param no : None
 * @return : None
 * @see    : None
 */
void update_Finish_C_Func()
{
    int fd;
    int cur_retry_cnt = 0;
    int mresult = 0;
    unsigned char pBuff[UPDATE_START_PKT_SIZE + 1];

    fprintf(stdout, "update_Finish_C_Func\n");

    while(cur_retry_cnt < MAX_RETRY_COUNT) {
        fd = openDevice();

        if(0 < fd) {
            cur_retry_cnt = 0;
            break;
        }

        cur_retry_cnt++;
        usleep(500);
        fprintf(stdout, "uart2 device open retry count[%d] \n",cur_retry_cnt);
    }

    if(cur_retry_cnt >= MAX_RETRY_COUNT) {
        fprintf(stderr, "uart2 device open failed\n");
        return ;
    }

    memset(pBuff, 0, UPDATE_START_PKT_SIZE + 1);
    memcpy(pBuff, UPDATE_FINISH_C_REQ, UPDATE_START_PKT_SIZE);

    while(cur_retry_cnt < MAX_RETRY_COUNT) {
        mresult = sendPacket(fd, pBuff, UPDATE_START_PKT_SIZE + 1);	// HEADER_LENGTH = PKT_SIZE = 8
        if(mresult == 0) {
            cur_retry_cnt = 0;
            break;
        }

        cur_retry_cnt++;
        sleep(1);
        fprintf(stderr, "SYS_UPDATE_START_MODE SENDING FAILED!!/n retry count[%d] \n",cur_retry_cnt);
    }

    if(cur_retry_cnt >= MAX_RETRY_COUNT) {
        fprintf(stderr, "uart2 device packet send failed\n");
        closeDevice(fd);
        return ;
    }

    closeDevice(fd);
}

 /**
 * Description : Send reset packet to Micom
 * @param no : None
 * @return : None
 * @see    : None
 */
void hardReset()
{
    int fd;
    int cur_retry_cnt = 0;
    int ret = 0;
    unsigned char pBuff[PKT_SIZE + 1];

    fprintf(stdout, "micom hard reset\n");

    // uart device open try 10 times
    while(cur_retry_cnt < MAX_RETRY_COUNT) {
        fd = openDevice();
        if(fd > 0) {
            cur_retry_cnt = 0;
            break;
        }

        cur_retry_cnt++;
        usleep(500);
        fprintf(stderr, "micom device open retry count[%d]\n", cur_retry_cnt);
    }

    // uart device open check
    if(cur_retry_cnt >= MAX_RETRY_COUNT) {
        fprintf(stderr, "micom device open failed\n");
        return ;
    }

    memset(pBuff, 0, PKT_SIZE + 1);
    memcpy(pBuff, SYS_RESET_PKT, PKT_SIZE);

    // send system reset packter to the uCom
    while(cur_retry_cnt < MAX_RETRY_COUNT) {
        ret = sendPacket(fd, pBuff, PKT_SIZE + 1);
        if(ret == 0) {
            cur_retry_cnt = 0;
            break;
        }

        cur_retry_cnt++;
        sleep(1);
        fprintf(stderr, "micom hard reset send packet retry count[%d]\n", cur_retry_cnt);
    }

    if(cur_retry_cnt >= MAX_RETRY_COUNT) {
        fprintf(stderr, "micom hard reset packet send failed\n");
        closeDevice(fd);
        return ;
    }

    closeDevice(fd);
}

void cancelReservationUpdate(void)
{
    int fd;
	int cur_retry_cnt = 0;
	int mresult = 0;
	unsigned char pBuff[UPDATE_RESERVATION_PKT_SIZE+1];

	fprintf(stdout, "micom cancel reservation message\n");

	while(cur_retry_cnt <MAX_RETRY_COUNT) {
		if((fd = openDevice()) > -1) {
			cur_retry_cnt = 0;
			break;
		}
		cur_retry_cnt++;
		usleep(500);
	}

	if (cur_retry_cnt >= MAX_RETRY_COUNT) {
		fprintf(stderr, "device open retry count[%d] \n",cur_retry_cnt);
		return ;
	}

	cur_retry_cnt = 0;
	memset(pBuff,0x00,UPDATE_RESERVATION_PKT_SIZE+1);
	memcpy(pBuff,UPDATE_CANCEL_RESERVATION_C,UPDATE_RESERVATION_PKT_SIZE);

	while(cur_retry_cnt < MAX_RETRY_COUNT) {
		mresult = sendPacket(fd,pBuff,UPDATE_RESERVATION_PKT_SIZE+1);
		if(mresult == 0) {
			cur_retry_cnt = 0;
			break;
		}
		cur_retry_cnt++;
		sleep(1);
		fprintf(stderr, "micom cancel reservation retry count[%d] \n",cur_retry_cnt);
	}

	closeDevice(fd);
}

void requestShutdown(void)
{
    int fd=0;
	int cur_retry_cnt = 0;
	int mresult = 0;
	unsigned char pBuff[UPDATE_SHUTDOWN_PKT_SIZE+1];

	fprintf(stdout, "micom shutdown message\n");

	while(cur_retry_cnt <MAX_RETRY_COUNT) {
		if((fd = openDevice()) > -1) {
			cur_retry_cnt = 0;
			break;
		}
		cur_retry_cnt++;
		usleep(500);
	}

	if (cur_retry_cnt >= MAX_RETRY_COUNT) {
		fprintf(stderr, "device open retry count[%d] \n",cur_retry_cnt);
		return ;
	}

	cur_retry_cnt = 0;
	memset(pBuff,0x00,UPDATE_SHUTDOWN_PKT_SIZE+1);
	memcpy(pBuff,UPDATE_SHUTDOWN_C,UPDATE_SHUTDOWN_PKT_SIZE);

	while(cur_retry_cnt < MAX_RETRY_COUNT) {
		mresult = sendPacket(fd,pBuff,UPDATE_SHUTDOWN_PKT_SIZE+1);
		if(mresult == 0) {
			cur_retry_cnt = 0;
			break;
		}
		cur_retry_cnt++;
		sleep(1);
		fprintf(stderr, "micom shutdown retry count[%d] \n",cur_retry_cnt);
	}

	closeDevice(fd);
}
