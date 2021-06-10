/*
 * Copyright (c) 2016 Beyless Co., Ltd.
 */

#ifndef TRIMBLE_GPS_PACKET_H
#define TRIMBLE_GPS_PACKET_H

#define TRIMBLEGPS_HARDWARE_ID_MAX   16

#pragma pack(push, 1)

namespace TrimbleGPS { namespace Packet
{
    const int MAX = 134;

    const unsigned char SOM = 0x81;
    const unsigned char EOM = 0x82;

    const unsigned char START_COMM = 0xA3;
    const unsigned char FLASHER_READY = 0x4A;
    const unsigned char ACK = 0xCC;
    const unsigned char NAK = 0xDD;

    typedef enum
    {
        CODE_ACK = 0x10,
        CODE_VERSION_REPORT = 0x11,
        CODE_HARDWARE_ID_REPORT = 0x12,
    } Code;

    typedef enum
    {
        CMDCODE_QUERY = 0x02,
        CMDCODE_RESET = 0x03,
    } CmdCode;

    typedef enum
    {
        SUBCODE_SYSCMD = 0x03
    } SubCode;

    typedef enum
    {
        SYSCMD_FORCE_TO_MONITOR_MODE = 0x03
    } SubCodeSysCmd;

    typedef struct {
        unsigned char bom;
        unsigned char code;
        unsigned char subcode;
        unsigned char syscmd;
        unsigned char status;
        unsigned char cs;
        unsigned char eom;
    } SystemCmdAck;

    typedef struct {
        unsigned char bom;
        unsigned char code;
        unsigned char subcode;
        unsigned char major_version;
        unsigned char minor_version;
        unsigned char build_number;
        unsigned char release_day;
        unsigned char release_month;
        unsigned short release_year;
        unsigned char cs;
        unsigned char eom;
    } VersionReport;

    typedef struct {
        unsigned char bom;
        unsigned char code;
        unsigned char subcode;
        char hardware_id[TRIMBLEGPS_HARDWARE_ID_MAX];
        unsigned char cs;
        unsigned char eom;
    } HardwareId;
}};

#pragma pack(pop)

#endif // TRIMBLE_GPS_PACKET_H
