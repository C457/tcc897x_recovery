/*
 * Copyright (c) 2016 Beyless Co., Ltd.
 */

#ifndef TRIMBLE_DEBUG_H
#define TRIMBLE_DEBUG_H

#if TRIMBLE_GPS_DEBUG

#define DUMP(buf,fmt,count)  \
    {   \
        DUMP2(buf,fmt,count); \
        printf("\n"); \
    }
#define DUMP2(buf,fmt,count)  \
    {   \
        printf("TrimbleGPS: " fmt " : "); \
        if ((buf) != NULL) { \
            for (int c = 0; c < (count); c++) { \
                printf("%02x ", (buf)[c]); \
            } \
        } else { \
        	printf("NULL"); \
        } \
    }
#define DUMP_N(buf, fmt, count, n)   \
    {   \
        DUMP_N2(buf,fmt,count,n); \
        printf("\n");\
    }
#define DUMP_N2(buf, fmt, count, n)   \
    {   \
        DUMP2(buf,fmt,(count) < (n) ? (count): (n)); \
        if (buf != NULL) { \
            printf("..."); \
        } \
    }
#define DBG_MSG(...)   \
    printf(__VA_ARGS__)

#else

#define DUMP(buf,fmt,count)
#define DUMP2(buf,fmt,count)
#define DUMP_N(buf,fmt,count, n)
#define DUMP_N2(buf,fmt,count,n)
#define DBG_MSG(...)    while (0)

#endif // TRIMBLE_GPS_DEBUG

//#define MSG(...) printf("TrimbleGPS: " __VA_ARGS__)

#endif // TRIMBLE_DEBUG_H
