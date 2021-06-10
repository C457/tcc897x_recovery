/*
 * Copyright (c) 2016 Beyless Co., Ltd.
 */

#ifndef TRIMBLE_GPS_MSGQUEUE_H
#define TRIMBLE_GPS_MSGQUEUE_H

#ifdef __cplusplus

#include <pthread.h>
#include <semaphore.h>

#include <errno.h>

namespace TrimbleGPS
{
    class Mutex {
    public:
        pthread_mutex_t mutex;
    public:
        Mutex() { pthread_mutex_init(&mutex, NULL); }
        ~Mutex() { pthread_mutex_destroy(&mutex); }
    public:
        void Lock() { pthread_mutex_lock(&mutex); }
        void Unlock() { pthread_mutex_unlock(&mutex); }
    };

    class Event {
    protected:
        pthread_cond_t cond;
    public:
        Event() {
            pthread_cond_init(&cond, NULL);
        }
        ~Event() {
            pthread_cond_destroy(&cond);
        }
    public:
        int Wait(Mutex& mutex, int sec = 0, int nsec = 0) { return Wait(&mutex.mutex, sec, nsec); }
        int Wait(pthread_mutex_t* mutex, int sec = 0, int nsec = 0);
        void Signal()
        {
            pthread_cond_broadcast(&cond);
        }
    };

    class Thread {
    protected:
        pthread_t thread;
    public:
        virtual ~Thread() {}
    private:
        static void* start_routine(void* param);
    protected:
        virtual void* OnRun() = 0;
    public:
        int CreateThread();
        int Join(void** retval);
    };

    class Msg {
    protected:
        static const int GROW_SIZE;
    private:
        int capacity;
    public:
        unsigned char* buf;
        int size;
    public:
        Msg() : capacity(0), buf(0), size(0) {}
        Msg(const unsigned char* src, int s);
        Msg(const Msg& msg);
        ~Msg() { if (buf) { free(buf); } };
    protected:
        void Grow();
        bool IsFull() const { return size == capacity; }
        int Remain() const { return capacity - size; }
        void Alloc(int s);
        void Delete();
    public:
        void Append(const unsigned char* val, int s);
        void Empty() { size = 0; }
        int Size() const { return size; }
        void Copy(const Msg& src);
    public:
        unsigned char Code() const { return buf[1]; }
        unsigned char Subcode() const { return buf[2]; }
    public:
        Msg& operator=(const Msg& src);
        unsigned char& operator[] (const int index) { return buf[index]; }
        operator unsigned char* () { return buf; }
    public:
        static bool SkipMessage(const Msg &msg);
        static unsigned char CalcChecksum(unsigned char csum, const unsigned char *b, int len);
    };

    class MsgQueue {
    private:
        class Node {
        public:
            Node* next;
            Msg* msg;
        public:
            Node() : next(NULL), msg(0) {}
        };
    private:
        Node* node;
        Mutex mutex;
        Event event;
        int count;
        Msg msgTemp;
    public:
        MsgQueue();
        ~MsgQueue();
    private:
        Node* Head();
        Node* Tail();
        Msg* Front();
        int _Pop(Msg& msg);
    public:
        bool IsEmpty() const;
        void Push(const unsigned char* buf, int size);
        void Push(const Msg& msg);
        int Pop(Msg& msg, int wait_sec = 0);
        void Parse(const unsigned char* buf, int size);
    };
};

#endif // __cplusplus

#endif // TRIMBLE_GPS_MSGQUEUE_H
