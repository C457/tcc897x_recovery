#ifndef TRIMBLE_CRC32_H
#define TRIMBLE_CRC32_H

#include <stdio.h>

extern unsigned long CalcCRC32 (const unsigned char *pucBuf, unsigned short usSize, unsigned long ulCRC);
extern unsigned long GetFirmwareCRC32(FILE* firmware, unsigned long remaining_size);

#endif
