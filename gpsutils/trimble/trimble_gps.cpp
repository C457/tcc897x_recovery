/*
 * Copyright (c) 2016 Beyless Co., Ltd.
 */

#define TRIMBLE_GPS_DEBUG 0
#include "trimble_debug.h"

#include "trimble_gps.h"
#include "trimble_gps_packet.h"
#include "crc32.h"
#include "../../ui.h"
#include "../../screen_ui.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>

#define BEGIN_WAIT() time_t _end; time_t _start; _end = _start = clock()
#define END_WAIT() _end = clock()
#define WAIT_TIME() ((_end - _start) / CLOCKS_PER_SEC)

#define MSG_TIMEOUT 10 // sec.

#define HARDWARE_ID_UNKNOWN 0
#define HARDWARE_ID_BISON   9006
#define HARDWARE_ID_BISON3  9013

extern RecoveryUI* ui;
#define MSG(...) ui->Print("[TrimbleGPS]: " __VA_ARGS__)

namespace TrimbleGPS {

Updater::Updater() : monitor_mode(false)
{
    memset(&gps, 0, sizeof(gps));
    gps.fd = -1;

    DBG_MSG("%s\n", __func__);
}

ssize_t Updater::WriteMsg(const unsigned char* buf, int size)
{
    static unsigned char msg[Packet::MAX];

    msg[0] = Packet::SOM;
    memcpy(msg + 1, buf, size);
    msg[size + 1] = Msg::CalcChecksum((0x81 + 0x82) & 0xff, buf, size);
    msg[size + 2] = Packet::EOM;

    return WriteMsgRaw(msg, size + 3);
}

ssize_t Updater::WriteFlasherId()
{
    static const unsigned char flasher_id[4] = { 0xF4, 0x01, 0xD5, 0xBC };
    DBG_MSG("Send ");
    DUMP(flasher_id, "FLASHER_IDENTIFIER", 4);
    return WriteMsgRaw(flasher_id, 4);
}

int Updater::Open(const char* gps_device)
{
    Close();

    struct termios tios;

    int fd = open(gps_device, O_NOCTTY | O_RDWR);
    if (fd == -1) {
        fprintf(stderr, "device(%s) open failed\n", gps_device);
        return -1;
    }
    if (tcgetattr(fd, &tios) != 0) {
        fprintf(stderr, "device(%s) tcgetattr failed (%d)\n", gps_device, errno);
        close(fd);
        return -1;
    }
    cfmakeraw(&tios);

    if (cfsetospeed(&tios, B115200) != 0) {
        fprintf(stderr, "device(%s) cfsetospeed failed (%d)\n", gps_device, errno);
        close(fd);
        return -1;
    }
    if (cfsetispeed(&tios, B115200) != 0) {
        fprintf(stderr, "device(%s) cfsetispeed failed (%d)\n", gps_device, errno);
        close(fd);
        return -1;
    }

    tios.c_cflag &= ~PARENB; //No parity
    tios.c_cflag &= ~CSTOPB; // stop bit 1
    tios.c_cflag &= ~CSIZE;
    tios.c_cflag |= CS8; // data bit 8
    tcflush(fd, TCIFLUSH);

    if (tcsetattr(fd, TCSANOW, &tios) != 0) {
        fprintf(stderr, "device(%s) tcsetattr failed (%d)\n", gps_device, errno);
        close(fd);
        return -1;
    }

    gps.fd = fd;

    poll_events.fd        = gps.fd;
    poll_events.events    = POLLIN | POLLERR;
    poll_events.revents   = 0;

    return fd;
}

int Updater::Close()
{
    if (gps.fd == -1) {
        return -1;
    }

    mutex.Lock();

    close(gps.fd);
    gps.fd = -1;

    if (gps.firmware.f) {
        fclose(gps.firmware.f);
        gps.firmware.f = NULL;
    }

    mutex.Unlock();

    return 0;
}

int Updater::Poll(int timeout)
{
    return poll((struct pollfd*)&poll_events, 1, timeout);
}

void Updater::SetHardwareId(int id)
{
    gps.hardware.id = id;
}

void Updater::SetHardwareIdString(const char* id)
{
    memcpy(gps.hardware.id_sz, id, TRIMBLEGPS_HARDWARE_ID_MAX);
    gps.hardware.id_sz[TRIMBLEGPS_HARDWARE_ID_MAX] = 0;
}

bool Updater::CheckMonitorMode()
{
    MSG("Checking monitor mode...\n");

    int poll_state = Poll(2 * 1000);

    if(poll_state == 0) {
        monitor_mode = true;
    }
    else {
        monitor_mode = false;
    }

    return monitor_mode;
}

#if 0

int Updater::WaitForStatus(GPSStatus status_ok, GPSStatus status_fail, int timeout_sec, bool check_monitor_mode)
{
    for (int i = 0; i < timeout_sec * 10; i++) {
        if (GetResultStatus() == status_ok) {
            return WAIT_STATUS_OK;
        }
        if (GetResultStatus() == status_fail) {
            return WAIT_STATUS_FAIL;
        }
        if (check_monitor_mode) {
            if (IsMonitorMode()) {
                SetResultStatus(GPS_STATUS_MONITOR_MODE_OK);
                return WAIT_STATUS_MONITOR_MODE;
            }
        }
        usleep(100 * 1000);
    }

    return WAIT_STATUS_OK;
}

#endif

int Updater::WaitMessage(const unsigned char* buf, int size, Msg& msg, int timeout)
{
    BEGIN_WAIT();

    while (WAIT_TIME() < timeout) {
        int retval = receiver.queue.Pop(msg);
        END_WAIT();

        if (!retval) {
            int i;
            for (i = 0; i < size; ++i) {
                if (msg[i + 1] != buf[i]) {
                    break;
                }
            }
            if (i == size) {
                return 0;
            }
            DBG_MSG("Waiting for ");
#if TRIMBLE_GPS_DEBUG
            for (int i = 0; i < size; ++i) {
                DBG_MSG("%02x ", buf[i]);
            }
            DBG_MSG("\n");
#endif
            DUMP_N(msg.buf, "Skip", msg.size, 4);
        }

        usleep(1000);
    }

    return ETIMEDOUT;
}

int Updater::WaitACK(int timeout)
{
    Msg msg;

    BEGIN_WAIT();

    while (WAIT_TIME() < timeout) {
        int retval = receiver.queue.Pop(msg);
        END_WAIT();

        if (!retval) {
            if (msg[0] == Packet::ACK) {
                DUMP(msg.buf, "ACK", msg.size);
                return 0;
            }
            if (msg[0] == Packet::NAK) {
                DUMP(msg.buf, "NAK", msg.size);
                return -1;
            }
            DUMP(msg.buf, "WaitACK Skip", msg.size);
        }

        usleep(1000);
    }

    return ETIMEDOUT;
}

#if 0

int Updater::WaitMessage(unsigned char code, int timeout)
{
    Msg msg;

    BEGIN_WAIT();

    while (WAIT_TIME() < timeout) {
        int retval = receiver.queue.Pop(msg);
        END_WAIT();

        if (!retval) {
            if (msg[0] == code) {
                return 0;
            }
            DBG_MSG("%s: Skip (%02x %02x %02x)\n", __func__, msg[1], msg[2], msg[3]);
        }

        usleep(1000);
    }

    return ETIMEDOUT;
}

int Updater::QueryMessage(unsigned char* buf, int size, Msg& reply, int timeout)
{
    if (WriteMsg(buf, size) == -1) {
        return -1;
    }

    return WaitMessage(buf[1], buf[2], reply, timeout);
}

int Updater::QueryMessage(unsigned char code, unsigned char subcode, unsigned char syscmd, Msg& reply, int timeout)
{
    static unsigned char buf[6];

    buf[1] = code;
    buf[2] = subcode;
    buf[3] = syscmd;

    return QueryMessage(buf, 3, reply, timeout);
}

int Updater::QueryMessage(unsigned char code, unsigned char subcode, Msg& reply, int timeout)
{
    static unsigned char buf[6];

    buf[1] = code;
    buf[2] = subcode;

    return QueryMessage(buf, 2, reply, timeout);
}

#endif

int Updater::RequestVersionReport()
{
    Msg msg;

    MSG("Request version report\n");

    unsigned char req_ver[3] = {Packet::CMDCODE_QUERY, Packet::CODE_VERSION_REPORT, 0x01};
    WriteMsg(req_ver, 3);

    if (ETIMEDOUT == WaitMessage(Packet::CODE_VERSION_REPORT, 0x01, msg, MSG_TIMEOUT)) {
        return ETIMEDOUT;
    }

    if (msg.size == sizeof(Packet::VersionReport)) {
        Packet::VersionReport* vr = (Packet::VersionReport*)msg.buf;

        gps.firmware.version.major = vr->major_version;
        gps.firmware.version.minor = vr->minor_version;
        gps.firmware.version.build = vr->build_number;
        gps.firmware.version.year = vr->release_year;
        gps.firmware.version.month = vr->release_month;
        gps.firmware.version.day = vr->release_day;

        MSG("Firmware Version: %d.%d (%04d-%02d-%02d)\n",
                gps.firmware.version.major,
                gps.firmware.version.minor,
                gps.firmware.version.year,
                gps.firmware.version.month,
                gps.firmware.version.day);

        if (gps.firmware.version.major == 1) {
            SetHardwareId(HARDWARE_ID_BISON);
        }
        else {
            SetHardwareId(HARDWARE_ID_BISON3);
        }

        return 0;
    }

    return -1;
}

int Updater::RequestHardwareId()
{
    Msg msg;

    MSG("Request hardware id\n");

    unsigned char req_hw_id[3] = {Packet::CMDCODE_QUERY, Packet::CODE_HARDWARE_ID_REPORT, 0x04};
    WriteMsg(req_hw_id, 3);

    if (ETIMEDOUT == WaitMessage(Packet::CODE_HARDWARE_ID_REPORT, 0x04, msg, MSG_TIMEOUT)) {
        return ETIMEDOUT;
    }

    if (msg.size == sizeof(Packet::HardwareId)) {
        Packet::HardwareId* hid = (Packet::HardwareId*)msg.buf;

        SetHardwareIdString(hid->hardware_id);
        MSG("Hardware ID=%s\n", GetHardwareIdString());

        return 0;
    }

    return -1;
}

int Updater::ValidateFirmware(const char* firmware_filename)
{
    stat(firmware_filename, &gps.firmware.st);
    gps.firmware.f = fopen(firmware_filename, "rb");

    if (gps.firmware.f == NULL) {
        MSG("Failed to open firmware file %s (%d)\n", firmware_filename, errno);
        goto exit_error;
    }

    char header[FIRMWARE_START_BYTES_LEN];

    if (fread(header, 1, FIRMWARE_START_BYTES_LEN, gps.firmware.f) != FIRMWARE_START_BYTES_LEN) {
        MSG("Failed to read first 4 bytes (%d)\n", errno);
        goto exit_error;
    }
    if (strncmp(header, "TRMB", FIRMWARE_START_BYTES_LEN)) {
        MSG("First 4 characters does not math 'TRMB'\n");
        goto exit_error;
    }

    unsigned short hardware_id;

    fseek(gps.firmware.f, FIRMWARE_OFFSET_HW_CODE, SEEK_SET);
    if (fread(&hardware_id, 1, 2, gps.firmware.f) != 2) {
        MSG("Failed to read hardware id (%d)\n", errno);
        goto exit_error;
    }
    hardware_id = htons(hardware_id);
    if (hardware_id != GetHardwareId()) {
        MSG("Hardware ID does not match %d (%02X)\n", GetHardwareId(), GetHardwareId());
        goto exit_error;
    }

    unsigned char version_buffer[FIRMWARE_VERSION_LEN];

    fseek(gps.firmware.f, FIRMWARE_OFFSET_VERSION, SEEK_SET);
    if (fread(version_buffer, 1, FIRMWARE_VERSION_LEN, gps.firmware.f) != 3) {
        MSG("Failed to read firmware version (%d)\n", errno);
        goto exit_error;
    }

    MSG("Version info : Major [%d] Minor [%d] Build [%d]\n",
        version_buffer[FIRMWARE_VERSION_MAJOR],
        version_buffer[FIRMWARE_VERSION_MINOR],
        version_buffer[FIRMWARE_VERSION_BUILD]);

    int prev, current;
    prev = (gps.firmware.version.major * 100) +
           (gps.firmware.version.minor * 10) +
           gps.firmware.version.build;

    current = (version_buffer[FIRMWARE_VERSION_MAJOR] * 100) +
              (version_buffer[FIRMWARE_VERSION_MINOR] * 10) +
              version_buffer[FIRMWARE_VERSION_BUILD];

    if(prev >= current) {
        MSG("Not need upgrade version prev [%d] current [%d]\n", prev, current);

        if (gps.firmware.f) {
            fclose(gps.firmware.f);
            gps.firmware.f = NULL;
        }

        return 1;
    }

    gps.firmware.crc32 = GetFirmwareCRC32(gps.firmware.f, GetFirmwareSize());

    MSG("Validate firmware %s OK\n", firmware_filename);

    return 0;

exit_error:
    if (gps.firmware.f) {
        fclose(gps.firmware.f);
        gps.firmware.f = NULL;
    }

    return -1;
}

int Updater::ForceMonitorMode()
{
    Msg msg;

    MSG("Force target into monitor mode\n");

    unsigned char force_mm[2] = {Packet::CMDCODE_RESET, 0x03};
    WriteMsg(force_mm, 2);

    if (ETIMEDOUT == WaitMessage(Packet::CODE_ACK, Packet::CMDCODE_RESET, 0x03, msg, MSG_TIMEOUT)) {
        return ETIMEDOUT;
    }

    if (msg.size == sizeof(Packet::SystemCmdAck)) {
        Packet::SystemCmdAck* ack = (Packet::SystemCmdAck*)msg.buf;
        if (ack->status != 0) {
            return ack->status;
        }
        MSG("Force into monitor mode OK\n");
    }

    return 0;
}

int Updater::SendFlasherId()
{
    const unsigned int flasher_id = 0xBCD501F4;
    Msg msg;
    int interval = 1;

    if (GetHardwareId() == HARDWARE_ID_BISON3) {
        interval = 0;
    }

    BEGIN_WAIT();
    while (WAIT_TIME() < 10) {
        WriteMsgRaw((unsigned char*)&flasher_id, 4);
        DUMP((unsigned char*)&flasher_id, "Send FLASHER_IDENTIFIER", 4);
        int retval = receiver.queue.Pop(msg, interval);
        END_WAIT();

        if (!retval) {
            if (0x83984073 == *((unsigned int*)msg.buf)) {
                DUMP(msg.buf, "FLASHER_SYNC received", msg.size);
                return 0;
            }
            DUMP_N(msg.buf, "Recv", msg.size, 5);
        }

        usleep(1000);
    }

    return -1;
}

int Updater::StartComm()
{
    DBG_MSG("START_COMM (0x%02X)\n", Packet::START_COMM);
    WriteMsgRaw(Packet::START_COMM);

    int retval = WaitACK(MSG_TIMEOUT);

    if (retval == 0) {
        MSG("The target and host are now synchronized\n");
        return 0;
    }
    return retval;
}

#define FIRMWARE_CONFIG_SIZE    20

int Updater::SendFirmwareLoadingConfig()
{
    unsigned long firmware_size = GetFirmwareSize();
    unsigned char firmware_config[FIRMWARE_CONFIG_SIZE];

    DBG_MSG("Firmware size: %lu (0x%04x), Firmware CRC32: %lu (0x%04x)\n",
            firmware_size, firmware_size,
            gps.firmware.crc32, gps.firmware.crc32);

    firmware_config[0] = GetHardwareId() == HARDWARE_ID_BISON ? 0x00: 0x01;
    firmware_config[1] = 0x00;
    firmware_config[2] = 0x00;
    firmware_config[3] = 0x01;
    firmware_config[4] = (0x000000ff & firmware_size);
    firmware_config[5] = (0x0000ff00 & firmware_size)>>8;
    firmware_config[6] = (0x00ff0000 & firmware_size)>>16;
    firmware_config[7] = (0xff000000 & firmware_size)>>24;
    firmware_config[8] = (0x000000ff & gps.firmware.crc32);
    firmware_config[9] = (0x0000ff00 & gps.firmware.crc32)>>8;
    firmware_config[10] = (0x00ff0000 & gps.firmware.crc32)>>16;
    firmware_config[11] = (0xff000000 & gps.firmware.crc32)>>24;
    firmware_config[12] = 0x00;
    firmware_config[13] = 0x00;
    firmware_config[14] = 0x10;
    firmware_config[15] = 0x00;
    firmware_config[16] = 0x00;
    firmware_config[17] = 0x00;
    firmware_config[18] = 0x06;
    firmware_config[19] = 0x00;

    DUMP(firmware_config, "firmware_config", FIRMWARE_CONFIG_SIZE);
    WriteMsgRaw(firmware_config, FIRMWARE_CONFIG_SIZE);

    Msg msg;
    int ack = 2;

    BEGIN_WAIT();
    while (WAIT_TIME() < 30 && ack > 0) {
        WriteMsgRaw(Packet::FLASHER_READY);
        int retval = receiver.queue.Pop(msg, 3);
        END_WAIT();

        if (!retval) {
            if (msg[0] == Packet::ACK) {
                DUMP(msg.buf, "FLAHSER_READY ACK", msg.size);
                if (msg.size == 2 && msg[1] == Packet::ACK) {
                    ack = 0;
                }
                else {
                    ack--;
                }
            }
            else {
                DUMP(msg.buf, "msg", msg.size);
            }
        }
    }

    if (ack > 0) {
        return -1;
    }

    MSG("Target begin erasure of the FLASH area\n");
    if (ETIMEDOUT == WaitACK(MSG_TIMEOUT)) {
        return -1;
    }
    MSG("Target begin erasure of the FLASH area - Done\n");

    if (GetHardwareId() == HARDWARE_ID_BISON3) {
        MSG("Target begin resetting the NVM area\n");
        if (ETIMEDOUT == WaitACK(MSG_TIMEOUT)) {
            return -1;
        }
        MSG("Target begin resetting the NVM area - Done\n");
    }

    return 0;
}

#define FIRMWARE_CHUNK  (16 * 1024)

int Updater::SendFirmwareData()
{
    static unsigned char buf[FIRMWARE_CHUNK];
    unsigned long firmware_left = gps.firmware.st.st_size - 256;

    MSG("Send firmware data to target\n");

    fseek(gps.firmware.f, 256, SEEK_SET);

#if TRIMBLE_GPS_DEBUG
    unsigned long firmware_size = GetFirmwareSize();
    unsigned long crc32 = CalcCRC32((unsigned char*)&firmware_size, 4, 0);
#endif

    while (firmware_left > 0) {
        unsigned int bytes_read = firmware_left < FIRMWARE_CHUNK ? firmware_left: FIRMWARE_CHUNK;
        DBG_MSG("Reading firmware data from file (%d byte(s))\n", bytes_read);
        if (fread(buf, 1, bytes_read, gps.firmware.f) != bytes_read) {
            MSG("Failed to read firmware\n");
            return -1;
        }
        DBG_MSG("Writing firmware data...\n");
        if (WriteMsgRaw(buf, bytes_read) != bytes_read) {
            MSG("Failed to write firmware data\n");
            return -1;
        }
        firmware_left -= bytes_read;
        MSG("Send firmware data %d byte(s) (%ld byte(s) left)\n", bytes_read, firmware_left);
        if (ETIMEDOUT == WaitACK(MSG_TIMEOUT)) {
            MSG("ACK timed out\n");
            return -1;
        }

#if TRIMBLE_GPS_DEBUG
        crc32 = CalcCRC32(buf, bytes_read, crc32);
#endif
    }

    MSG("Send firmware data done\n");

    DBG_MSG("Firmware CRC32: %lu (0x%04x)\n", crc32, crc32);

    MSG("Waiting for CRC32 calculation\n");

    int ack = WaitACK(MSG_TIMEOUT);

    if (ack == ETIMEDOUT) {
        return -1;
    }

    if (ack != 0) {
        MSG("CRC32 failed\n");
        return -1;
    }

    return 0;
}

}; // namespace TrimbleGPS

int requestUpdateGPS2(const char *firmware_file)
{
    int ret;
    clock_t start = clock();
    TrimbleGPS::Updater updater;

    if (-1 == updater.Open("/dev/ttyTCC3")) {
        MSG("Failed to open GPS firmware file %s\n", firmware_file);
        return -1;
    }

#ifdef TRIMBLE_GPS_BISON3
    updater.SetHardwareId(HARDWARE_ID_BISON3);
#else
    updater.SetHardwareId(HARDWARE_ID_BISON);
#endif

    if (updater.CheckMonitorMode()) {
        MSG("GPS module is in MONITOR MODE\n");
    }
    else {
        MSG("GPS module is in NORMAL MODE\n");
    }

    if (updater.BeginReceive()) {
        return -1;
    }

    if (!updater.IsMonitorMode()) {
        if (updater.RequestHardwareId()) {
            MSG("Failed to getting hardware id\n");
            return -1;
        }

        if (updater.RequestVersionReport()) {
            MSG("Failed to getting version report\n");
            return -1;
        }
    }

    ret = updater.ValidateFirmware(firmware_file);
    if (ret) {
        if(ret > 0) {
            MSG("Do not need firmware update\n");
            return 0;
        }

        MSG("Failed to validate firmware\n");
        return -1;
    }

    if (!updater.IsMonitorMode()) {
        int status = updater.ForceMonitorMode();
        if (status) {
            MSG("Failed to force target into monitor mode: %d\n", status);
            return -1;
        }
    }

    if (updater.SendFlasherId()) {
        MSG("Failed to FLASHER_IDENTIFIER\n");
        return -1;
    }

    if (updater.StartComm()) {
        MSG("Failed to synchronization with target\n");
        return -1;
    }

    if (updater.SendFirmwareLoadingConfig()) {
        MSG("Failed to send firmware loading configuration\n");
        return -1;
    }

    if (updater.SendFirmwareData()) {
        MSG("Failed to send firmware data\n");
        return -1;
    }

    clock_t end = clock();

    MSG("Firmware Updated!\n");

    MSG("==================\n");
    MSG("End: %f\n",(double)(end - start)/CLOCKS_PER_SEC);
    MSG("==================\n");

    return 0;
}
