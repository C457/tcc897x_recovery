/*
 * Copyright (c) 2016 Beyless Co., Ltd.
 */

#ifndef TRIMBLE_GPS_MSGRECEIVER_H
#define TRIMBLE_GPS_MSGRECEIVER_H

#include <sys/poll.h>
#include "trimble_gps_msgqueue.h"

namespace TrimbleGPS
{
    class MsgReceiver : private Thread {
    private:
        struct pollfd pfds[1];
    public:
        MsgQueue queue;
    protected:
        virtual void* OnRun();
    public:
        MsgReceiver();
    public:
        int Run(int fd);
        void* Stop();
    };
};

#endif // TRIMBLE_GPS_MSGRECEIVER_H
