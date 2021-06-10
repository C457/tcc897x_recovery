#include <stdio.h>
#include <stdarg.h>

#include "RB_ImageUpdate.h"
#include "RB_vRM_Update.h"

//3 To do: customize the flash_drive file
//#include "flash_driver.h"

#define FS_U_RAM_SIZE	300*1024*1024  //추후 변경...
#define FS_ID_MAX_LEN 4

#ifndef RB_MAX_PATH
#define RB_MAX_PATH 1024
#endif


#define IMAGE_FILE_PATH	  ""
//#define TEMP_STORAGE_PATH "tmp"
#define TEMP_STORAGE_PATH "/data/update/test_tmp"
#define MOUNT_POINT       "/data/update/system"
#define PARTITION_NAME    "cp_system"




extern int gtargetisimage;
extern char *PartitionName;
extern char *gdiffBinName;
extern int  gblockSize;
extern int  gscout;

/*****************************************************************************************************
	RB_DoReset

******************************************************************************************************/
void DoReset(void)
{
        //3 To do: Implement reset.
}


/*****************************************************************************************************
	RB_ReadImageNewKey

	Description : Read content from flash assuming it is the updated image .
			     This function is used when the update installer reads content from flash that is assumed to
			     be the updated version. This is useful when the image is encrypted and the key of the old
			     version and the new version might be different.

******************************************************************************************************/
long RB_ReadImageNewKey(void *pbUserData, unsigned char *pbBuffer, RB_UINT32 dwStartAddress, RB_UINT32 dwSize)
{
	return RB_ReadImage(pbUserData, pbBuffer, dwStartAddress, dwSize);
}

/*****************************************************************************************************
	RB_ReadImage

	Description : This function should copy data from flash memory into a RAM buffer. This function is used to read image data.

******************************************************************************************************/
long RB_ReadImage(
	void *pbUserData,				/* User data passed to all porting routines */
	unsigned char *pbBuffer,			/* pointer to user buffer */
	RB_UINT32 dwStartAddress,		/* memory address to read from */
	RB_UINT32 dwSize)				/* number of bytes to copy */
{

	return (S_RB_SUCCESS);
}


/*****************************************************************************************************
	RB_ReadBackupBlock

	Description : Can copy data of specified size from any location in one of the buffer blocks and into specified RAM location.

******************************************************************************************************/
long RB_ReadBackupBlock(
	void *pbUserData,				/* User data passed to all porting routines */
	unsigned char *pbBuffer,			/* pointer to user buffer in RAM where the data will be copied */
	RB_UINT32 dwBlockAddress,		/* address of data to read into RAM. Must be inside one of the backup buffer blocks */
	RB_UINT32 dwSize)				/* buffer size in bytes */
{
	return RB_ReadImage(pbUserData,pbBuffer, dwBlockAddress, dwSize);
}



long RB_EraseBackupBlock(
	void *pbUserData,
	RB_UINT32 dwStartAddress)
{
	return 0;
}


long RB_WriteBackupPartOfBlock(
	void *pbUserData,
	RB_UINT32 dwStartAddress,
	RB_UINT32 dwSize,
	unsigned char* pbBuffer)
{
	return 0;
}


/*****************************************************************************************************
	RB_WriteBlock

	Description : Erases the image location and writes the data to that location.

******************************************************************************************************/
long RB_WriteBlock(
	void *pbUserData,					/* User data passed to all porting routines */
	RB_UINT32 dwBlockAddress,		/* address of the block to be updated */
	unsigned char *pbBuffer)			/* pointer to data to be written */
{
	return (S_RB_SUCCESS);
}

/*****************************************************************************************************
	RB_WriteBackupBlock

	Description : This function copies data from specified address in RAM to a backup buffer block.
			     This will write part of a block (sector) that was already written with RB_WriteBackupBlock().

******************************************************************************************************/
long RB_WriteBackupBlock(
	void *pbUserData,					/* User data passed to all porting routines */
	RB_UINT32 dwBlockStartAddress,	/* address of the block to be updated */
	unsigned char *pbBuffer)  	        /* RAM to copy data from */
{
	return RB_WriteBlock(pbUserData,dwBlockStartAddress, pbBuffer);
}

/*****************************************************************************************************
	RB_GetBlockSize

	Description : Returns the size of one memory block.

******************************************************************************************************/
long RB_GetBlockSize(void *pbUserData)
{
	return 0x10000;//UA_NAND_FLASH_BLOCK_SIZE;
}

/*****************************************************************************************************
	RB_SyncAll

	Description : .

******************************************************************************************************
long RB_SyncAll(void* pUser, const char* strPath)
{
  //TODO : what is this?
  printf("RB_SyncALL is using that concurrently multi partition update");
}
*/

/*****************************************************************************************************
	RB_ImageUpdateMain

******************************************************************************************************/
int ImageUpdateMain(void)
{
	int ret = 0;
	RB_UINT32 ComponentInstallerType[1]={0};
  unsigned char del[RB_MAX_PATH]={'\0'};
  unsigned char* pRamMem =(unsigned char*)calloc(FS_U_RAM_SIZE,1);
	char partition_name[256], mount_point[256], TempPath[256];
	vRM_DeviceData pDeviceDatum;
  
	CustomerPartitionData pCustomerPartData;

  printf(">>>>>> ImageUpdateMain <<<<<<<\n");

	strcpy(partition_name,PARTITION_NAME);
	strcpy(mount_point,    MOUNT_POINT);
	strcpy(TempPath, TEMP_STORAGE_PATH);  //fail safe storage data

  memset(&pDeviceDatum, 0, sizeof(vRM_DeviceData));
	memset(&pCustomerPartData, 0, sizeof(CustomerPartitionData));
  
	pCustomerPartData.partition_name			= partition_name;
	pCustomerPartData.rom_start_address		= 0;
	pCustomerPartData.mount_point				  = mount_point;
	pCustomerPartData.partition_type			= PT_FOTA;
	pCustomerPartData.strSourcePath				= 0;
	pCustomerPartData.strTargetPath				= 0;
	pCustomerPartData.priv						    = NULL;

	pDeviceDatum.ui32Operation 					  = gscout;
	pDeviceDatum.pRam 							      = pRamMem;
	pDeviceDatum.ui32RamSize					    = FS_U_RAM_SIZE;
	pDeviceDatum.ui32NumberOfBuffers			= 0;
	pDeviceDatum.pBufferBlocks					  = 0;
	pDeviceDatum.ui32NumberOfPartitions		= 1;
	pDeviceDatum.pFirstPartitionData			= &pCustomerPartData;
	pDeviceDatum.pTempPath						    = TempPath;
	pDeviceDatum.pComponentInstallerTypes	= ComponentInstallerType;
	pDeviceDatum.ui32ComponentInstallerTypesNum	= 1;
	pDeviceDatum.enmUpdateType					  = UT_IN_PLACE;
	pDeviceDatum.pDeltaPath						    = del;
	pDeviceDatum.pbUserData						    = NULL;
	pDeviceDatum.ui32ComponentUpdateFlags = 0xFFFFFFFF;

#if 0
  ret = RB_ReadImage(void * pbUserData, unsigned char * pbBuffer, RB_UINT32 dwStartAddress, RB_UINT32 dwSize);
	if (ret != 0)
	{
		printf_dbg("%s: read img file %s failed.\n", __func__, IMAGE_FILE_PATH);

		return E_RB_OPENFILE_ONLYR;
	}
#endif
	ret = RB_vRM_Update(&pDeviceDatum);
	
  printf("RB_vRM_Update, 0x%08x, %d\n", ret, RB_IS_ERROR(ret));

	return ret;
}

#if 0 //Only Updage Base on image not file system.
void BootMain(void)
{
	//Get the current update state
	if(Get_usd())
	{
		if(UpdateStatusData[0] == 0x41544F46)
		{
			if(UpdateStatusData[2] == 0x31) //trigger update for UA
				ImageUpdateMain();
		}
	}
}
#endif
