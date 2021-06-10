#ifndef __UPDATE_BOOTLOADER_H__
#define __UPDATE_BOOTLOADER_H__

typedef struct {
    int type;
    ssize_t size;
    char* data;
} Value;

int update_bootloader(int partition_no, char *data, unsigned int size);
int update_bootloader_function(const char *rom_path);

#endif //__UPDATE_BOOTLOADER_H__
