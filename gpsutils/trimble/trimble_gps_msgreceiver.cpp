/*
 * Copyright (c) 2016 Beyless Co., Ltd.
 */

//#define TRIMBLE_GPS_MSGRECEIVER_DEBUG

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>

#ifdef TRIMBLE_GPS_MSGRECEIVER_DEBUG
    #ifndef TRIMBLE_GPS_DEBUG
        #define TRIMBLE_GPS_DEBUG 1
    #endif
#define MSGRECV_DBG_MSG DBG_MSG
#else
    #define MSGRECV_DBG_MSG(...)
#endif

#include "trimble_debug.h"
#include "trimble_gps_msgreceiver.h"

#define MAX_DATA_LEN 128

namespace TrimbleGPS
{
    MsgReceiver::MsgReceiver()
    {
        memset(pfds, 0, sizeof(pfds));
    }

    void* MsgReceiver::OnRun()
    {
        int nevents;
        unsigned char buffer[MAX_DATA_LEN];
        Msg msg;

        MSGRECV_DBG_MSG("%s\n", __func__);

        while (1) {
            nevents = poll(pfds, 1, 500);

            if (nevents == 0) {
                continue;
            }
            if (nevents < 0) {
                MSGRECV_DBG_MSG("%s: Failed poll (%d)\n", __func__, nevents);
                break;
            }

            if (pfds[0].revents & POLLERR) {
                MSGRECV_DBG_MSG("%s: error while reading message\n", __func__);
                break;
            }
            if (pfds[0].revents & POLLHUP) {
                MSGRECV_DBG_MSG("%s: fd closed\n", __func__);
                break;
            }

            if (!(pfds[0].revents & POLLIN)) {
                MSGRECV_DBG_MSG("%s: revents=0x%03X\n", __func__, pfds[0].revents);
                break;
            }

            int nbytes = read(pfds[0].fd, buffer, MAX_DATA_LEN);
            //unsigned char* ptr = buffer;

            //MSGRECV_DBG_MSG("%s: %d byte(s) received\n", __func__, nbytes);
            //DUMP_N(buffer, "buffer", nbytes, 10);

            queue.Parse(buffer, nbytes);
        }

        MSGRECV_DBG_MSG("%s: return\n", __func__);

        return NULL;
    }

    int MsgReceiver::Run(int fd)
    {
        MSGRECV_DBG_MSG("%s\n", __func__);

        if (fd <= 0) {
            return 0;
        }

        pfds[0].fd = fd;
        pfds[0].events = POLLIN | POLLERR | POLLHUP;

        return CreateThread();
    }

    void* MsgReceiver::Stop()
    {
        void* retval;

        MSGRECV_DBG_MSG("%s: Stopping msg receiver...\n", __func__);

        Thread::Join(&retval);

        MSGRECV_DBG_MSG("%s: Stopped\n", __func__);

        return retval;
    }
};
