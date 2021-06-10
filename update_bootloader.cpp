#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "ui.h"
#include "update_bootloader.h"

#define HIDDEN_AREA_NUM 10
#define ENTRY_POINT 0xA8000000 //TCC897x
//#define ENTRY_POINT 0x88000000 //TCC897x
//#define ENTRY_POINT 0x30000000 //TCC802x
#define HEAD_FLAGS "HEAD"
#define CRC32_SIZE 4
#define MMC_CAP_4_BIT_DATA 0x1
#define MMC_CAP_8_BIT_DATA 0x2
#define MMC_CAP_NORMALSPEED 0x1
#define MMC_CAP_HIGHSPEED 0x4
#define BOOT_PART0_PATH "/dev/block/mmcblk0boot0"
#define BOOT_PART1_PATH "/dev/block/mmcblk0boot1"
#define MEM_INIT_START_ADDR 0x70
#define MEM_INIT_END_ADDR 0x74

extern RecoveryUI* ui;

char t_buf[16*1024*1024];

const unsigned long _CRC32_TABLE[256] = {
  0x00000000,     0x90910101,     0x91210201,     0x01B00300,
  0x92410401,     0x02D00500,     0x03600600,     0x93F10701,
  0x94810801,     0x04100900,     0x05A00A00,     0x95310B01,
  0x06C00C00,     0x96510D01,     0x97E10E01,     0x07700F00,
  0x99011001,     0x09901100,     0x08201200,     0x98B11301,
  0x0B401400,     0x9BD11501,     0x9A611601,     0x0AF01700,
  0x0D801800,     0x9D111901,     0x9CA11A01,     0x0C301B00,
  0x9FC11C01,     0x0F501D00,     0x0EE01E00,     0x9E711F01,
  0x82012001,     0x12902100,     0x13202200,     0x83B12301,
  0x10402400,     0x80D12501,     0x81612601,     0x11F02700,
  0x16802800,     0x86112901,     0x87A12A01,     0x17302B00,
  0x84C12C01,     0x14502D00,     0x15E02E00,     0x85712F01,
  0x1B003000,     0x8B913101,     0x8A213201,     0x1AB03300,
  0x89413401,     0x19D03500,     0x18603600,     0x88F13701,
  0x8F813801,     0x1F103900,     0x1EA03A00,     0x8E313B01,
  0x1DC03C00,     0x8D513D01,     0x8CE13E01,     0x1C703F00,
  0xB4014001,     0x24904100,     0x25204200,     0xB5B14301,
  0x26404400,     0xB6D14501,     0xB7614601,     0x27F04700,
  0x20804800,     0xB0114901,     0xB1A14A01,     0x21304B00,
  0xB2C14C01,     0x22504D00,     0x23E04E00,     0xB3714F01,
  0x2D005000,     0xBD915101,     0xBC215201,     0x2CB05300,
  0xBF415401,     0x2FD05500,     0x2E605600,     0xBEF15701,
  0xB9815801,     0x29105900,     0x28A05A00,     0xB8315B01,
  0x2BC05C00,     0xBB515D01,     0xBAE15E01,     0x2A705F00,
  0x36006000,     0xA6916101,     0xA7216201,     0x37B06300,
  0xA4416401,     0x34D06500,     0x35606600,     0xA5F16701,
  0xA2816801,     0x32106900,     0x33A06A00,     0xA3316B01,
  0x30C06C00,     0xA0516D01,     0xA1E16E01,     0x31706F00,
  0xAF017001,     0x3F907100,     0x3E207200,     0xAEB17301,
  0x3D407400,     0xADD17501,     0xAC617601,     0x3CF07700,
  0x3B807800,     0xAB117901,     0xAAA17A01,     0x3A307B00,
  0xA9C17C01,     0x39507D00,     0x38E07E00,     0xA8717F01,
  0xD8018001,     0x48908100,     0x49208200,     0xD9B18301,
  0x4A408400,     0xDAD18501,     0xDB618601,     0x4BF08700,
  0x4C808800,     0xDC118901,     0xDDA18A01,     0x4D308B00,
  0xDEC18C01,     0x4E508D00,     0x4FE08E00,     0xDF718F01,
  0x41009000,     0xD1919101,     0xD0219201,     0x40B09300,
  0xD3419401,     0x43D09500,     0x42609600,     0xD2F19701,
  0xD5819801,     0x45109900,     0x44A09A00,     0xD4319B01,
  0x47C09C00,     0xD7519D01,     0xD6E19E01,     0x46709F00,
  0x5A00A000,     0xCA91A101,     0xCB21A201,     0x5BB0A300,
  0xC841A401,     0x58D0A500,     0x5960A600,     0xC9F1A701,
  0xCE81A801,     0x5E10A900,     0x5FA0AA00,     0xCF31AB01,
  0x5CC0AC00,     0xCC51AD01,     0xCDE1AE01,     0x5D70AF00,
  0xC301B001,     0x5390B100,     0x5220B200,     0xC2B1B301,
  0x5140B400,     0xC1D1B501,     0xC061B601,     0x50F0B700,
  0x5780B800,     0xC711B901,     0xC6A1BA01,     0x5630BB00,
  0xC5C1BC01,     0x5550BD00,     0x54E0BE00,     0xC471BF01,
  0x6C00C000,     0xFC91C101,     0xFD21C201,     0x6DB0C300,
  0xFE41C401,     0x6ED0C500,     0x6F60C600,     0xFFF1C701,
  0xF881C801,     0x6810C900,     0x69A0CA00,     0xF931CB01,
  0x6AC0CC00,     0xFA51CD01,     0xFBE1CE01,     0x6B70CF00,
  0xF501D001,     0x6590D100,     0x6420D200,     0xF4B1D301,
  0x6740D400,     0xF7D1D501,     0xF661D601,     0x66F0D700,
  0x6180D800,     0xF111D901,     0xF0A1DA01,     0x6030DB00,
  0xF3C1DC01,     0x6350DD00,     0x62E0DE00,     0xF271DF01,
  0xEE01E001,     0x7E90E100,     0x7F20E200,     0xEFB1E301,
  0x7C40E400,     0xECD1E501,     0xED61E601,     0x7DF0E700,
  0x7A80E800,     0xEA11E901,     0xEBA1EA01,     0x7B30EB00,
  0xE8C1EC01,     0x7850ED00,     0x79E0EE00,     0xE971EF01,
  0x7700F000,     0xE791F101,     0xE621F201,     0x76B0F300,
  0xE541F401,     0x75D0F500,     0x7460F600,     0xE4F1F701,
  0xE381F801,     0x7310F900,     0x72A0FA00,     0xE231FB01,
  0x71C0FC00,     0xE151FD01,     0xE0E1FE01,     0x7070FF00
};

unsigned long mmc_get_crc8(char *base, unsigned int length)
{
  unsigned long crcout = 0;
  unsigned long cnt;
  char code, tmp;

  for(cnt=0; cnt<length; cnt++)
  {
    code = base[cnt];
    tmp = code^crcout;
    crcout = (crcout>>8)^_CRC32_TABLE[tmp&0xFF];
  }
  return crcout;
}



struct boot_header {
  unsigned long entry_point;
  unsigned long base_addr;
  unsigned long mem_init_code_size;
  unsigned long bootloader_size;
  unsigned long mmc_caps;
  unsigned long boot_partition_size;
  unsigned long mem_init_code_base;
  unsigned long bootloader_base;
  char serial_number[64];
  unsigned long reserved96[8];
  unsigned long hidden_start_addr;
  unsigned long hidden_area_size[HIDDEN_AREA_NUM];
  unsigned long reserved176[83];
  char flags[4];
  unsigned long crc;
};

struct mem_code_info {
  unsigned long start_code_offset;
  unsigned long end_code_offset;
  unsigned long code_size;
  char *init_code;
};

struct bootloader_info {
  unsigned long size;
};

unsigned long byte_to_sector(unsigned long byte) { return (((byte+0x1FF) & 0xfffffe00) >> 9); }
unsigned long sector_to_byte(unsigned long sector) { return sector << 9; }


unsigned long mmc_get_mem_code_base() { return 1; }
unsigned long mmc_get_bootloader_base(unsigned long mem_init_code_size)
{
    return mmc_get_mem_code_base() + byte_to_sector(mem_init_code_size);
}

int mmc_set_serial_no(int fd, char *dst)
{
  lseek(fd, 32, SEEK_SET);
  read(fd, dst, 64);
  lseek(fd, 0, SEEK_SET);
  return 0;
}


int init_mem_code_info(struct mem_code_info *mem, char *src)
{
	memcpy(&mem->start_code_offset, &src[MEM_INIT_START_ADDR], 4);
	memcpy(&mem->end_code_offset, &src[MEM_INIT_END_ADDR], 4);

	mem->code_size = mem->end_code_offset - mem->start_code_offset + CRC32_SIZE;
	mem->start_code_offset -= ENTRY_POINT;
	mem->end_code_offset -= ENTRY_POINT;

	mem->init_code = NULL;
	mem->init_code = (char *)malloc(mem->code_size);
	if (mem->init_code == NULL) {
		fprintf(stderr,"Fail to allocate mem_init_code array...\n");
		return -1;
	}

	memset(mem->init_code, 0x0, mem->code_size);
	memcpy(mem->init_code, &src[mem->start_code_offset], mem->code_size);

	fprintf(stderr,"[%s] mem->code_size         : 0x%08ld\n", __func__, mem->code_size);
	fprintf(stderr,"[%s] mem->start_code_offset : 0x%08lx\n", __func__, mem->start_code_offset);
	fprintf(stderr,"[%s] mem->end_code_offset   : 0x%08lx\n", __func__, mem->end_code_offset);

	return 0;
}

int init_bootloader_info(struct bootloader_info *bootloader, unsigned int size)
{
  bootloader->size = size;
#if defined(DEBUG)
  fprintf(stderr,"[%s] Bootloader size : %ld Byte\n", __func__, bootloader->size);
#endif

  return 0;
}

int disable_force_ro(int partition_no)
{
  int fd=0;
  char sys_path[4096];
  char disable[4];

  memset(sys_path, 0x0, 4096);
  sprintf(sys_path, "/sys/block/mmcblk0boot%d/force_ro", partition_no);

  fd = open(sys_path, O_RDWR);
  if (fd<0) {
    fprintf(stderr,"[%s] Failed to open %s...\n", sys_path, sys_path);
    return -1;
  }

  memset(disable, 0x0, 4);
  strcpy(disable, "0");
  write(fd, disable, 4);

  close(fd);

  return 0;
}

int enable_force_ro(int partition_no)
{
  int fd=0;
  char sys_path[4096];
  char disable[4];

  memset(sys_path, 0x0, 4096);
  sprintf(sys_path, "/sys/block/mmcblk0boot%d/force_ro", partition_no);

  fd = open(sys_path, O_RDWR);
  if (fd<0) {
    fprintf(stderr,"[%s] Failed to open %s...\n", sys_path, sys_path);
    return -1;
  }

  memset(disable, 0x0, 4);
  strcpy(disable, "1");
  write(fd, disable, 4);

  close(fd);

  return 0;
}

int mmc_update_header(
    int fd, 
    struct mem_code_info *mem, 
    struct bootloader_info *bootloader)
{
  struct boot_header h, t_h;

  memset(&h, 0x0, sizeof(struct boot_header));
  memset(&t_h, 0x0, sizeof(struct boot_header));

  fprintf(stderr,"[1/3 - %s] Update Header...", __func__);
  h.entry_point = ENTRY_POINT;
  h.base_addr = ENTRY_POINT;
  h.mem_init_code_size = mem->code_size;
  h.bootloader_size = bootloader->size;
  h.mmc_caps = MMC_CAP_4_BIT_DATA | MMC_CAP_NORMALSPEED;
  h.mem_init_code_base = mmc_get_mem_code_base();
  h.bootloader_base = mmc_get_bootloader_base(h.mem_init_code_size);
  mmc_set_serial_no(fd, h.serial_number);
  h.flags[0] = 'H';
  h.flags[1] = 'E';
  h.flags[2] = 'A';
  h.flags[3] = 'D';
  h.crc = mmc_get_crc8((char*)&h, sizeof(struct boot_header)-sizeof(h.crc));

  lseek(fd, 0, SEEK_SET);
  write(fd, &h, sizeof(struct boot_header));

  lseek(fd, 0, SEEK_SET);
  read(fd, &t_h, sizeof(struct boot_header));
  if (memcmp((void*)&h, (void*)&t_h, sizeof(struct boot_header)) != 0) {
    fprintf(stderr,"[mmc_update_header] Fail to verify boot header!\n");
    return -1;
  }
  fprintf(stderr,"Done.\n");
  
  fprintf(stderr,"============ Boot Header Information ============\n");
  fprintf(stderr,"[%s] entry_point         : 0x%08lx\n", __func__, h.entry_point);
  fprintf(stderr,"[%s] base_addr           : 0x%08lx\n", __func__, h.base_addr);
  fprintf(stderr,"[%s] mem_init_code_size  : 0x%08lx\n", __func__, h.mem_init_code_size);
  fprintf(stderr,"[%s] bootloader_size     : 0x%08lx\n", __func__, h.bootloader_size);
  fprintf(stderr,"[%s] mem_init_code_base  : 0x%08lx\n", __func__, h.mem_init_code_base);
  fprintf(stderr,"[%s] bootloader_base     : 0x%08lx\n", __func__, h.bootloader_base);
  fprintf(stderr,"[%s] serial number       : %s\n", __func__, h.serial_number);
  fprintf(stderr,"[%s] crc                 : 0x%08lx\n", __func__, h.crc);
  fprintf(stderr,"=================================================\n");
  
  return 0;
}

int mmc_update_mem_init_code(
    int fd, 
    struct mem_code_info *mem, 
    char *src)
{
  unsigned long crc = 0;

  fprintf(stderr,"[2/3 - %s] Update Mem Init Code...", __func__);
  crc = mmc_get_crc8(mem->init_code, mem->code_size - CRC32_SIZE);
  memcpy(mem->init_code + mem->code_size - CRC32_SIZE, &crc, CRC32_SIZE);

  lseek(fd, sector_to_byte(mmc_get_mem_code_base()), SEEK_SET);
  write(fd, mem->init_code, mem->code_size);

  lseek(fd, sector_to_byte(mmc_get_mem_code_base()), SEEK_SET);
  read(fd, t_buf, mem->code_size);
  
  if (memcmp((void*)mem->init_code, (void*)t_buf, mem->code_size) != 0) {
    fprintf(stderr,"[%s] Failed to verify mem init code!\n", __func__);
    return -1;
  }
  fprintf(stderr,"Done.\n");
  
  return 0;
}

int mmc_update_bootloader(int dst, struct mem_code_info *mem, 
                          struct bootloader_info *bootloader_info, char *src)
{
    fprintf(stderr,"[3/3 - %s] Update Bootloader...", __func__);

    lseek(dst, sector_to_byte(mmc_get_bootloader_base(mem->code_size)), SEEK_SET);
    write(dst, src, bootloader_info->size);

    lseek(dst, sector_to_byte(mmc_get_bootloader_base(mem->code_size)), SEEK_SET);
    read(dst, t_buf, bootloader_info->size);
    if(memcmp((void*)src, (void*)&t_buf, bootloader_info->size) != 0) {
        fprintf(stderr,"[%s] Failed to verify bootloader!\n", __func__);
        return -1;
    }
  
    fprintf(stderr,"Done.\n");

    return 0;
}

int update_bootloader(int partition_no, char *src, unsigned int size)
{
	int ret = 0;
    int fd = 0;
    struct mem_code_info mem_code_info;
    struct bootloader_info bootloader_info;

    fprintf(stderr,"[%s][%d] \r\n",__func__,__LINE__);
    memset(&mem_code_info, 0x0, sizeof(struct mem_code_info));
    memset(&bootloader_info, 0x0, sizeof(struct bootloader_info));

    disable_force_ro(partition_no);

    fd = open(partition_no ? BOOT_PART1_PATH : BOOT_PART0_PATH, O_RDWR);
    if(fd < 0) {
        fprintf(stderr,"Failed to open %s\n", BOOT_PART0_PATH);
        exit(-1);
    }
 
    ret = init_mem_code_info(&mem_code_info, src);
	if (ret != 0) {
        fprintf(stderr,"Failed to init mem :  %d\n", ret);
    	close(fd);
    	enable_force_ro(partition_no);
		return -1;
	}

    init_bootloader_info(&bootloader_info, size);

    if(mmc_update_header(fd, &mem_code_info, &bootloader_info)) {
        fprintf(stderr,"Failed to update boot header at %d boot partition...\n", partition_no);
    	close(fd);
    	enable_force_ro(partition_no);
        return -4;
    }

    if(mmc_update_mem_init_code(fd, &mem_code_info, src)) {
        fprintf(stderr,"Failed to update mem init code at %d boot partition...\n", partition_no);
    	close(fd);
    	enable_force_ro(partition_no);
        return -5;
    }

    if(mmc_update_bootloader(fd, &mem_code_info, &bootloader_info, src)) {
        fprintf(stderr,"Failed to update bootloader at %d boot partition...\n", partition_no);
    	close(fd);
    	enable_force_ro(partition_no);
        return -6;
    }

    close(fd);

    enable_force_ro(partition_no);

    return 0;
}

int update_bootloader_function(const char *rom_path)
{
    int ret = 0;
    int fd;
    char *romBuff;
    unsigned int readlength;

    struct stat st;

    fd = open (rom_path, O_RDONLY | O_NDELAY);
    if (0 > fd) {
        ui->Print("[%s] File Open Failed\n", __func__);
        return -1;
    }

    ret = fstat(fd, &st);

    if (ret < 0) {
        ui->Print("[%s] Fstat failed : %d\n", __func__, ret);
        goto update_bootloader_fileclose_out;
    }

    romBuff = (char *)malloc(st.st_size);

    if (romBuff == NULL) {
        ui->Print("[%s] Memory Allocation Failed\n", __func__);
        ret = -1;
        goto update_bootloader_fileclose_out;
    }

    ui->Print("[%s] Start Bootloader Update\n", __func__);

    memset(romBuff, 0, st.st_size);
    readlength = read(fd, romBuff, st.st_size);
    ui->Print("[%s] file size : %d, read length %ld\n", __func__, st.st_size, readlength);

    ret = update_bootloader(0, romBuff, (unsigned int)st.st_size);

    if(0 > ret) {
        ui->Print("[%s] Bootloader 0 update failed : %d\n", __func__, ret);
        goto update_bootloader_memfree_out;
    }
    ui->Print("[%s] Bootloader 0 update Success\n", __func__);

    ret = update_bootloader(1, romBuff, (unsigned int)st.st_size);
    if(0 > ret) {
        ui->Print("[%s] Bootloader 1 update failed : %d\n", __func__, ret);
        goto update_bootloader_memfree_out;
    }
    ui->Print("[%s] Bootloader 1 update Success\n", __func__);

update_bootloader_memfree_out:
    free(romBuff);
update_bootloader_fileclose_out:
    close(fd);

    return ret;
}
