#ifndef __UPDATE_BOOTLOADER_H__
#define __UPDATE_BOOTLOADER_H__

int update_bootloader(int partition_no, const char *bootloader_path, char *data, unsigned int size);

#endif //__UPDATE_BOOTLOADER_H__