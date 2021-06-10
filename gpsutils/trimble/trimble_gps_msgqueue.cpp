/*
 * Copyright (c) 2016 Beyless Co., Ltd.
 */

//#define TRIMBLE_GPS_MSGQUEUE_DEBUG

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>

#include "trimble_gps_msgqueue.h"

//#define TRIMBLE_GPS_MSGQUEUE_DEBUG
#define TRIMBLE_GPS_DEBUG 0

#ifdef TRIMBLE_GPS_MSGQUEUE_DEBUG
    #ifndef TRIMBLE_GPS_DEBUG
        #define TRIMBLE_GPS_DEBUG 1
    #endif
    #define MSGQUEUE_DBG_MSG DBG_MSG
#else
    #define MSGQUEUE_DBG_MSG(...)
#endif

#include "trimble_debug.h"

//#define TRIMBLE_GPS_MSGQUEUE_DUMP
#ifdef TRIMBLE_GPS_MSGQUEUE_DUMP
#include <sys/stat.h>
#include <fcntl.h>
static int _dump = 0;
#endif

namespace TrimbleGPS {

    int Event::Wait(pthread_mutex_t* mutex, int sec /*=0*/, int nsec /*=0*/)
    {
        MSGQUEUE_DBG_MSG("%s: sec=%d, nsec=%d\n", __func__, sec, nsec);

        if (sec <= 0 && nsec <= 0) {
            return pthread_cond_wait(&cond, mutex);
        }

        struct timespec ts;

        clock_gettime(CLOCK_REALTIME, &ts);

        ts.tv_sec += sec;
        ts.tv_nsec += nsec;

        return pthread_cond_timedwait(&cond, mutex, &ts);
    }

    const int Msg::GROW_SIZE = 134;

    Msg::Msg(const Msg& msg) : capacity(0), buf(0), size(0)
    {
        Copy(msg);
    }

    Msg::Msg(const unsigned char* src, int s) : buf(0), size(0)
    {
        capacity = (double)s / (double)GROW_SIZE + (s % GROW_SIZE ? GROW_SIZE: 0);
        buf = (unsigned char*)malloc(capacity);
        memcpy(buf, src, s);
        size = s;
    }

    void Msg::Copy(const Msg& src)
    {
        MSGQUEUE_DBG_MSG("%s: capacity=%d\n", __func__, src.capacity);

        if (buf) {
            free(buf);
        }

        capacity = src.capacity;
        size = src.size;
        buf = (unsigned char*)malloc(capacity);
        memcpy(buf, src.buf, size);
    }

    void Msg::Grow()
    {
        capacity += GROW_SIZE;
        buf = (unsigned char*)realloc(buf, capacity);
    }

    void Msg::Append(const unsigned char* val, int s)
    {
        while (Remain() < s) {
            Grow();
        }
        memcpy(buf + size, val, s);
        size += s;
    }

    Msg& Msg::operator=(const Msg& src)
    {
        Copy(src);
        return *this;
    }

    bool Msg::SkipMessage(const Msg &msg)
    {
        switch (msg.Code()) {
        case 0x70: // Diagnostic Report Packets
        case 0x14: // Log Report
        case 0x30: // Fast Fix with Raw DR Data
            return true;
            break;
        }

        return false;
    }

    unsigned char Msg::CalcChecksum(unsigned char csum, const unsigned char *b, int len)
    {
        while (len) {
            csum += *b;
            b++;
            len--;
        }
        csum = ~csum + 0x01;
        DBG_MSG("calc_checksum : [0x%x] \n", csum);
        return csum;
    }

    MsgQueue::MsgQueue() : node(NULL), count(0)
    {
#ifdef TRIMBLE_GPS_MSGQUEUE_DUMP
        _dump = open("/upgrade/trimblegps_msg.txt", O_WRONLY | O_CREAT);
#endif
    }

    MsgQueue::~MsgQueue()
    {
        mutex.Lock();

        Msg msg;

        while (!_Pop(msg)) {

        }

        MSGQUEUE_DBG_MSG("%s\n", __func__);

        mutex.Unlock();
#ifdef TRIMBLE_GPS_MSGQUEUE_DUMP
        close(_dump);
#endif
    }

    MsgQueue::Node* MsgQueue::Head()
    {
        return node;
    }

    MsgQueue::Node* MsgQueue::Tail()
    {
        if (IsEmpty()) {
            //MSGQUEUE_DBG_MSG("%s: empty\n", __func__);
            return NULL;
        }

        MsgQueue::Node* tail = node;

        while (tail->next != NULL) {
            tail = tail->next;
        }

        return tail;
    }

    Msg* MsgQueue::Front()
    {
        MsgQueue::Node* head = Head();
        return head ? head->msg: NULL;
    }

    bool MsgQueue::IsEmpty() const
    {
        //MSGQUEUE_DBG_MSG("%s: node = %p\n", __func__, node);
        return NULL == node;
    }

    void MsgQueue::Push(const unsigned char* buf, int size)
    {
        mutex.Lock();

        Msg msg(buf, size);
        Push(msg);

        mutex.Unlock();
    }

    void MsgQueue::Push(const Msg& msg)
    {
        mutex.Lock();

        if (Msg::SkipMessage(msg)) {
            mutex.Unlock();
            return;
        }

        MsgQueue::Node* newNode = new MsgQueue::Node;

        newNode->msg = new Msg;
        newNode->msg->Copy(msg);

#ifdef TRIMBLE_GPS_MSGQUEUE_DUMP
        write(_dump, msg.buf, msg.size);
#endif

        MsgQueue::Node* tail = Tail();

        if (tail) {
            tail->next = newNode;
        }
        else {
            MSGQUEUE_DBG_MSG("%s: First element\n", __func__);
            node = newNode;
            event.Signal();
        }
        count++;

        MSGQUEUE_DBG_MSG("%s: element pushed %p (%d)\n", __func__, node, count);

        mutex.Unlock();
    }

    int MsgQueue::_Pop(Msg& msg)
    {
        if (node) {
            MsgQueue::Node* head = node;

            node = node->next;
            msg = *(head->msg);
            delete head;

            count--;

            return 0;
        }

        return -1;
    }

    int MsgQueue::Pop(Msg& msg, int wait_sec /*= 0*/)
    {
        mutex.Lock();

        while (_Pop(msg) != 0) {
            if (!wait_sec) {
                mutex.Unlock();
                return ETIMEDOUT;
            }
            MSGQUEUE_DBG_MSG("%s: Waiting for new msg...\n", __func__);
            if (ETIMEDOUT == event.Wait(mutex, wait_sec)) {
                MSGQUEUE_DBG_MSG("%s: Timed out\n", __func__);
                mutex.Unlock();
                return ETIMEDOUT;
            }
            usleep(1000);
        }

        MSGQUEUE_DBG_MSG("%s: element popped (%d)\n", __func__, count);

        mutex.Unlock();

        return 0;
    }

    void MsgQueue::Parse(const unsigned char* buf, int size)
    {
#if 1
        for (int i = 0; i < size; i++, buf++) {
            if (msgTemp.Size() > 0) {
                msgTemp.Append(buf, 1);
                if (msgTemp.size == 4) {
                    if (0x83984073 == *((unsigned int*)msgTemp.buf)) {
                        MSGQUEUE_DBG_MSG("%s: FLASHER_SYNC\n", __func__);
                        Push(msgTemp);
                        msgTemp.Empty();
                    }
                }
                if (*buf == 0x82) {
                    MSGQUEUE_DBG_MSG("%s: EOM (%d)\n", __func__, msg.Size());
                    //DUMP(msgTemp.buf, "Received packet", msgTemp.Size());
                    Push(msgTemp);
                    msgTemp.Empty();
                }
            }
            else {
                switch (*buf) {
                case 0x81:
                case 0x73:
                    msgTemp.Append(buf, 1);
                    MSGQUEUE_DBG_MSG("%s: SOM (0x%02X)\n", __func__, *buf);
                    break;
                case 0xCC:
                case 0xDD:
#if 1
                    if (size <= 2) {
                        msgTemp.Append(buf, size);
                        i = size;
                    }
                    else
#endif
                    {
                        msgTemp.Append(buf, 1);
                    }
                    //DUMP(msgTemp.buf, "Received packet", msgTemp.Size());
                    Push(msgTemp);
                    msgTemp.Empty();
                    break;
                default:
                    break;
                }
            }
        }
#else
        DUMP(buf, "Received packet", size);
        msgTemp.Append(buf, size);
        Push(msgTemp);
        msgTemp.Empty();
#endif
    }

    void* Thread::start_routine(void* param)
    {
        Thread* pThread = (Thread*)param;
        void* retval = pThread->OnRun();

        MSGQUEUE_DBG_MSG("%s: return\n", __func__);

        return retval;
    }

    int Thread::CreateThread()
    {
        return pthread_create(&thread, NULL, Thread::start_routine, this);
    }

    int Thread::Join(void** retval)
    {
        int n = pthread_join(thread, retval);

        MSGQUEUE_DBG_MSG("%s: return\n", __func__);

        return n;
    }
};
