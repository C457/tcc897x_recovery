/*
 * Copyright (c) 2016 Beyless Co., Ltd.
 */

#ifndef TRIMBLE_GPS_H
#define TRIMBLE_GPS_H

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <poll.h>
#include <stdio.h>

#include "trimble_debug.h"
#include "trimble_gps_msgqueue.h"
#include "trimble_gps_msgreceiver.h"

#define TRIMBLEGPS_HARDWARE_ID_MAX   16

#define FIRMWARE_START_BYTES_LEN    4
#define FIRMWARE_OFFSET_HW_CODE     16
#define FIRMWARE_OFFSET_VERSION     20
#define FIRMWARE_VERSION_LEN        3

namespace TrimbleGPS {

class Updater
{
public:
    typedef enum
    {
        WAIT_STATUS_TIMEDOUT = 0,
        WAIT_STATUS_OK = 1,
        WAIT_STATUS_FAIL = 2,
        WAIT_STATUS_MONITOR_MODE = 3,
    } WAIT_STATUS;

    enum {
        FIRMWARE_VERSION_MAJOR = 0,
        FIRMWARE_VERSION_MINOR,
        FIRMWARE_VERSION_BUILD,
    };

protected:
    typedef struct
    {
        int fd;
        struct {
            unsigned short id;
            char id_sz[TRIMBLEGPS_HARDWARE_ID_MAX + 1];
        } hardware;

        struct
        {
            struct {
                unsigned char major;
                unsigned char minor;
                unsigned char build;
                unsigned short year;
                unsigned char month;
                unsigned char day;
            } version;
            struct stat st;
            FILE* f;
            unsigned long crc32;
        } firmware;
    } gps_t;

protected:
    MsgReceiver receiver;
    gps_t gps;
    Mutex mutex;
    struct pollfd poll_events;
    bool monitor_mode;

public:
    Updater();
    ~Updater() { Close(); }

public:
    int Poll(int timeout);

public:
    int Open(const char* gps_device);
    int Close();

    bool CheckMonitorMode();
    void SetMonitorMode(bool mm) { monitor_mode = mm; }
    bool IsMonitorMode() const { return monitor_mode; }

    const char* GetHardwareIdString() const { return gps.hardware.id_sz; }
    void SetHardwareIdString(const char* id);

    unsigned short GetHardwareId() const { return gps.hardware.id; }
    void SetHardwareId(int id);

    ssize_t WriteMsgRaw(const unsigned char* buf, int size) { DUMP_N(buf, "WriteMsgRaw", size, 10); return write(gps.fd, buf, size); }
    ssize_t WriteMsgRaw(unsigned char ch) { return WriteMsgRaw(&ch, 1); }
    ssize_t WriteMsg(const unsigned char* buf, int size);
    ssize_t WriteFlasherId();

    //int WaitForStatus(GPSStatus status_ok, GPSStatus status_fail, int timeout_sec, bool check_monitor_mode);
    int BeginReceive() { return receiver.Run(gps.fd); }

public:
    int WaitMessage(unsigned char code, unsigned char subcode, unsigned char syscmd, Msg& msg, int timeout)
    {
        unsigned char buf[3] = {code, subcode, syscmd};
        return WaitMessage(buf, 3, msg, timeout);
    }
    int WaitMessage(unsigned char code, unsigned char subcode, Msg& msg, int timeout)
    {
        unsigned char buf[2] = {code, subcode};
        return WaitMessage(buf, 2, msg, timeout);
    }
    int WaitMessage(const unsigned char* buf, int size, Msg& msg, int timeout);
    int WaitMessage(unsigned char buf, int timeout)
    {
        Msg msg;
        return WaitMessage(&buf, 1, msg, timeout);
    }
    int WaitACK(int timeout);
#if 0
    int WaitMessage(unsigned char code, int timeout);
    int QueryMessage(unsigned char* buf, int size, Msg& reply, int timeout);
    int QueryMessage(unsigned char code, unsigned char subcode, unsigned char syscmd, Msg& reply, int timeout);
    int QueryMessage(unsigned char code, unsigned char subcode, Msg& reply, int timeout);
#endif
    int RequestHardwareId();
    int RequestVersionReport();
    int ValidateFirmware(const char* firmware_filename);
    int ForceMonitorMode();
    int SendFlasherId();
    int StartComm();
    int SendFirmwareLoadingConfig();
    int SendFirmwareData();
    unsigned long GetFirmwareSize() const { return gps.firmware.st.st_size - 256; }
};

}; // namespace TrimbleGPS

extern int requestUpdateGPS2(const char* firmware_file);

#endif
