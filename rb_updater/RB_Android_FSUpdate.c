/*
 *******************************************************************************
 * Copyright (c) 1999-2016 Redbend. All Rights Reserved.
 *******************************************************************************
 */

/*
Change log:
Date		Owner		Description
9/8/2011	Amir S.		changed MAX_PATH to RB_MAX_PATH and use it
						format if statement for return in RB_DeleteFile
						remove if from RB_ResizeFile and RB_CloseFile
						call RB_SyncFile in RB_CloseFile
						change implementation of RB_FSTrace to run on Android
5/6/2012	Amir S.		changed API function prototype to be of v8.0 i.e. use UTF8 instead of UTF16 in file path
15/7/2012	Amir S.		Fix bug in RB_GetDelta - call fclose for the file descriptor in case of error
20/7/2012	Amir S.		fix strtol base of user and group ID to be 0x10 and not 10. enlarge the attributes buffer to be on the safe side.
30/7/2012	Amir S.		remove the use of #include <sys/statvfs.h>
22/8/2012	Amir S.		Added compare on the number of written bytes to the number requested
12/9/2012	Amir S.		Fix code for getting partition free size
26/12/2012	Amir S. 	Fix FSMain sample and remove old attributes format support
5/6/2013	Amir S.		Add O_TRUNC to open mode in case of write only (ONLY_W)
20/5/2013	Amir S.		Fix code in RB_Link to check if RB_VerifyLinkReference return success; fix copyright notice date
30/6/2013	Amir S.		Remove call to RB_SyncFile in RB_CloseFile
26/10/2014	Amir S.		fix call RB_VerifyLinkReference; move to use pread and pwrite; open delta file only once
						fix code of RB_SetFileAttributes to support extended attributes.
15/02/2015	Amir S.		Change code so that it will open Delta file only once. Add support for QNX OS
27/04/2015	Amir S.		Added code for RB_SyncAll
23/06/2016  Amir S.		In RB_GetAvailableFreeSpace should use f_bavail instead of f_bfree
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>
#ifdef QNX
#include <sys/statvfs.h>
#else
#include <sys/xattr.h>
#include <sys/statfs.h>
#endif
#include <sys/mount.h>
#include <sys/types.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <fs_mgr.h>

#include "RB_FileSystemUpdate.h"
#include "RB_vRM_Errors.h"
#include "RB_vRM_Update.h"
#include "RB_common.h"

#include "mtdutils/mtdutils.h"
#include "mtdutils/mounts.h"
#include "roots.h"

#include "daudio_settings.h"

#include "updater/updater.h"
#include "common.h"

#include "cutils/android_reboot.h"

/* relate of boot active partition*/
#define DEVICE_FILENAME1        "/dev/usb_7v_detect"
#define SETTINGS_PARTITION_PATH "/dev/block/platform/bdm/by-name/settings"

#define OFFSET_EM_SETTINGS               0
#define OFFSET_VARIANT_SETTINGS          OFFSET_EM_SETTINGS + 512
#define OFFSET_BOOT_ACTIVE_SETTINGS      OFFSET_VARIANT_SETTINGS + 512
/* relate of boot active partition*/

#define MAX_BUFF_SIZE       4096
#define RW_BUFF_SIZE        (4*1024*1024) //4M

#define FS_U_RAM_SIZE       300*1024*1024  //추후 변경...
#define FS_ID_MAX_LEN       4

#ifndef RB_MAX_PATH
#define RB_MAX_PATH         1024
#endif

#define XATTR_SIZE_MAX      1024

#define DEV_MOUNT_PATH_SIZE 512
#define STR_BUF_SIZE_MAX    256
#define PT_NAME_SIZE        32

#define REC_DELTA_FILE_PATH	     "/upgrade/ota/root_debug_delta.up"
#define TEMP_STORAGE_PATH        "/upgrade/ota"

const char *dev_blk_path              = "/dev/block/platform/bdm/by-name";

const char *boot_pt_name              = "boot";
const char *boot_mnt_point            = "/boot";
const char *boot_mirror_mnt_point     = "/boot_mirror";

const char *splash_pt_name            = "splash";
const char *splash_mnt_point          = "/splash";
const char *splash_mirror_mnt_6point  = "/splash_mirror";

const char *recovery_pt_name          = "recovery";
const char *recovery_mnt_point        = "/recovery";
const char *recovery_mirror_mnt_point = "/recovery_mirror";

const char *system_pt_name            = "system";
const char *system_mnt_point          = "/system";
const char *system_mirror_mnt_point   = "/system_mirror";

const char *vr1_pt_name               = "vr1";
const char *vr1_mnt_point             = "/vr1";
const char *vr1_mirror_mnt_point      = "/vr1_mirror";

const char *vr2_pt_name               = "vr2";
const char *vr2_mnt_point             = "/vr2";
const char *vr2_mirror_mnt_point      = "/vr2_mirror";

const char *system_copy_name          = "system_copy";
const char *system_copy_mnt_point     = "/system_copy";

const char *fs_image_name             = "fs_image";

const char *fsImage_pt_name[] = {
    "boot",
    "recovery",
    "splash"
};


#define DBG 1
#define PIPE_PRINT

#ifdef DBG

#ifdef PIPE_PRINT
#define printf_dbg(...) fprintf(g_cmd_pipe, __VA_ARGS__)
#else //PIPE_PRINT
#define printf_dbg     printf
#endif //PIPE_PRINT

#else //DBG
#define printf_dbg(args...)
#endif //DBG

#define WIFEXITED(w)	  (((w) & 0xff) == 0)
#define WIFSIGNALED(w)	(((w) & 0x7f) > 0 && (((w) & 0x7f) < 0x7f))
#define WIFSTOPPED(w)	  (((w) & 0xff) == 0x7f)
#define WEXITSTATUS(w)	(((w) >> 8) & 0xff)
#define WTERMSIG(w)	    ((w) & 0x7f)

// for test
#define TEST_MAKERESULT(rc) (int)((rc)>>31)
//global vars
long h_delta;

#define RB_UPDATE_PATITION_NUM 5

#define CUSTOMER_MULTI_PARTITION

static long start_t, end_t;

extern int FS_ImgHandle[];

unsigned boot_active_partition = 0;  // 0 : boot normal partition, 1 : boot mirror partition

#ifdef PIPE_PRINT
FILE* g_cmd_pipe;
#endif //PIPE_PRINT

static int boot_partition_set(unsigned cur_boot);

/************************************************************
 *                     common functions
 ************************************************************/
 
 /*!
 *******************************************************************************
 * Get free space, in bytes, of a mounted file system.
 *
 * \param	pbUserData				Optional opaque data-structure to pass to
 *									IPL functions
 * \param	path				    Name of any directory within the mounted
 *									file system
 * \param	available_flash_size	(out) Available space in bytes
 *
 * \return	S_RB_SUCCESS on success or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */
long RB_GetAvailableFreeSpace(
	void *pbUserData,
	const char *partition_name,
	RB_UINT64* available_flash_size)
{
#ifdef QNX
	#define stat_fs statvfs
#else
	#define stat_fs statfs
#endif
	struct stat_fs vfs;

	if ( stat_fs(partition_name, &vfs) < 0 )
	{
		printf_dbg("%s : %s : failed to get statvfs\n", __func__,partition_name);
		*available_flash_size = 0;
		return -1;
	}

	*available_flash_size = vfs.f_bsize * vfs.f_bavail;
	printf_dbg("%s, %s : success %lld\n", __func__,partition_name, *available_flash_size);
	return 0;
}

long RB_GetRBDeltaOffset(
	void*		pbUserData,
	RB_UINT32	signed_delta_offset,
	RB_UINT32*	delta_offset)
{
	* delta_offset = signed_delta_offset;
	return 0;
}

long RecursiveFolderCreater(
	const char*	folderpath,
	const	mode_t mode)
{
	int ret = 0;
	char temppath[RB_MAX_PATH] = {'\0'};
	int pathOffset = strlen(folderpath);// For counting back until the '/' delimiter


	if(pathOffset == 0)
		return -1;//if from some reason we got to the end return error!!!.

	while(folderpath[pathOffset] != '/')// get to the next '/' place
		pathOffset--;

	strncpy(temppath, folderpath, pathOffset);// copy one depth below till and without the char '/'

	ret = mkdir(temppath, mode);
	if (ret == 0 || ((ret == -1) && (errno == EEXIST)))
	{
		return 0;//meaning the depth creation is success.
	}
	else if((ret == -1) && (errno == ENOENT))
	{
		ret = RecursiveFolderCreater(temppath, mode);
		if (ret == 0)
		{
			ret = mkdir(temppath, mode);
		}
		return ret;
	}
	else
	{
		return -1;
	}
}

long PT_RecursiveFolderCreater(
	const char*	folderpath,
	const	mode_t mode)
{
	int ret = 0;
	char temppath[RB_MAX_PATH] = {'\0'};
	int pathOffset = strlen(folderpath);// For counting back until the '/' delimiter


	if(pathOffset == 0)
		return -1;//if from some reason we got to the end return error!!!.

	while(folderpath[pathOffset] != '/')// get to the next '/' place
		pathOffset--;

	strncpy(temppath, folderpath, pathOffset);// copy one depth below till and without the char '/'

	ret = mkdir(temppath, mode);
	if (ret == 0 || ((ret == -1) && (errno == EEXIST)))
	{
		return 0;//meaning the depth creation is success.
	}
	else if((ret == -1) && (errno == ENOENT))
	{
		ret = PT_RecursiveFolderCreater(temppath, mode);
		if (ret == 0)
		{
			ret = mkdir(temppath, mode);
		}
		return ret;
	}
	else
	{
		return -1;
	}
}

long PT_CreateFolder(
	const char*	strPath)
{
	mode_t mode = 0;
	int ret = 0;

	mode =
		S_IRUSR /*Read by owner*/ |
		S_IWUSR /*Write by owner*/ |
		S_IXUSR /*Execute by owner*/ |
		S_IRGRP /*Read by group*/ |
		S_IWGRP /*Write by group*/ |
		S_IXGRP /*Execute by group*/ |
		S_IROTH /*Read by others*/ |
		S_IWOTH /*Write by others*/ |
		S_IXOTH /*Execute by others*/;

	ret = mkdir(strPath, mode);

	if (ret == 0 || ((ret == -1) && (errno == EEXIST)))
	{
		return S_RB_SUCCESS;
	}
	else if((ret == -1) && (errno == ENOENT))//maybe multi directory problem
	{
		ret = PT_RecursiveFolderCreater(strPath, mode);
		if(ret == 0)
		{
			ret = mkdir(strPath, mode);//After creating all the depth Directories we try to create the Original one again.
			if(ret == 0)
				return S_RB_SUCCESS;
			else
			{
				printf("Create folder problem value: %d, errno: %d\n", ret, errno);
				return E_RB_FAILURE;
			}
		}
		else
		{
			printf("Create folder problem value: %d, errno: %d\n", ret, errno);
			return E_RB_FAILURE;
		}
	}
	else
	{
		printf("Create folder problem value: %d, errno: %s\n", ret, strerror(errno));
		return E_RB_FAILURE;
	}
}


/*!
 *******************************************************************************
 * Copy file.<p>
 *
 * Must create the path to the new file as required. Must overwrite any contents
 * in the old file, if any. Must check if the source file is a symbolic link.
 * If it is, instead create a new symbolic link using \ref RB_Link.
 *
 * \param	pbUserData	Optional opaque data-structure to pass to IPL
 *						functions
 * \param	strFromPath	Path to old file
 * \param	strToPath	Path to new file
 *
 * \return	S_RB_SUCCESS on success or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */
long RB_CopyFile(
	void*					pbUserData,
	const char*	strFromPath,
	const char*	strToPath)
{
	FILE* fp1 = NULL;
	FILE* fp2 = NULL;
	unsigned int readCount = 0, writeCount = 0;
	char buf[MAX_BUFF_SIZE];
	int ret = 0;

	if (!strFromPath || !strToPath)
	{
		printf_dbg("NULL file name find. Abort.\n");
		return -1;			//should never happen
	}

	fp1 = fopen(strFromPath, "r");
	if (!fp1)
	{
		printf_dbg(" Open %s ENOENT %d\n", strFromPath, errno);
		printf_dbg("Open %s failed. Abort.\n", strFromPath);
		return E_RB_OPENFILE_ONLYR;
	}

	fp2 = fopen(strToPath, "w");
	if (!fp2)
	{
		const char* folder = strrchr(strToPath,'/');
		char* folderPath = (char *) malloc(folder - strToPath + 1);

		if (folderPath == NULL)
		{
			printf_dbg("malloc failure (folderPath).\n");
			return -4;
		}
		memset(folderPath,'\0',folder - strToPath + 1);
		strncpy(folderPath,strToPath,folder - strToPath);
		if ( RB_CreateFolder(pbUserData, folderPath) != S_RB_SUCCESS )
		{
			fclose(fp1);
			printf_dbg("Open %s failed. Abort.\n", strToPath);
			free(folderPath);
			return E_RB_OPENFILE_WRITE;
		}
		else
		{
			free(folderPath);
			fp2 = fopen(strToPath, "w");
			if(!fp2)
			{
				fclose(fp1);
				printf_dbg("Open %s failed. Abort.\n", strToPath);
				return E_RB_OPENFILE_WRITE;
			}
		}
	}

	while( (readCount = fread(buf, 1, MAX_BUFF_SIZE, fp1))> 0)
	{
		writeCount = fwrite(buf, 1, readCount, fp2);
		if (writeCount != readCount)
		{
			printf_dbg(" read %d, but write %d, abort.\n", readCount, writeCount);
			printf_dbg("==> write error (%s)\n", strerror(errno));
			ret = E_RB_WRITE_ERROR;
 			break;
		}
	}

	fclose(fp1);
	fflush(fp2);
	fsync(fileno(fp2));
	fclose(fp2);

	return ret;
}

int isdir(char path[])
{
	struct stat dir;
	stat(path,&dir);
	if(S_ISREG(dir.st_mode))//checks if string is file
		return 0;
	if(S_ISDIR(dir.st_mode))//checks if string is directory
		return 1;
	return -1;
}

int mkdir_p(const char *dir, const mode_t mode) {
    char tmp[256];
    char *p = NULL;
    struct stat sb;
    size_t len;

    /* copy path */
    strncpy(tmp, dir, sizeof(tmp));
    len = strlen(tmp);
    if (len >= sizeof(tmp)) {
        return -1;
    }

    /* remove trailing slash */
    if(tmp[len - 1] == '/') {
        tmp[len - 1] = 0;
    }

    /* recursive mkdir */
    for(p = tmp + 1; *p; p++) {
        if(*p == '/') {
            *p = 0;
            /* test path */
            if (stat(tmp, &sb) != 0) {
                /* path does not exist - create directory */
                if (mkdir(tmp, mode) < 0) {
                    return -1;
                }
				/////////////////////////////////////////
				chmod(tmp, 0755);
    			chown(tmp, 1000, 1000);   // system user
    			/////////////////////////////////////////
    
            } else if (!S_ISDIR(sb.st_mode)) {
                /* not a directory */
                return -1;
            }
            *p = '/';
        }
    }
    /* test path */
    if (stat(tmp, &sb) != 0) {
        /* path does not exist - create directory */
    	if (mkdir(tmp, mode) < 0) {
            return -1;
        }
		/////////////////////////////////////////
		chmod(tmp, 0755);
    	chown(tmp, 1000, 1000);   // system user
    	/////////////////////////////////////////
    } else if (!S_ISDIR(sb.st_mode)) {
        /* not a directory */
        return -1;
    }
    return 0;
}

int copy_dir(char *source_path, char *destination_path)
{
	char *fname = strrchr(source_path,'/');//Get the name of target folder
	char *sys = (char *)malloc(strlen(destination_path)+strlen(fname)+1+6+2);//String buffer to pass to system()

	//printf_dbg(" copy_dir source = %s\n", source_path);
	//printf_dbg(" copy_dir fname = %s\n", fname);
	//printf_dbg(" copy_dir destination = %s\n", destination_path);

	strcat(sys,destination_path);
	strcat(sys,fname);
	
	//printf_dbg(" copy_dir sys = %s\n", sys);
	if (mkdir_p(sys, 0755) == 0) {
	} else {
		printf_dbg(" make directory failure = %s\n", destination_path);
	}

	DIR *d1;
	if((d1 = opendir(source_path)) == NULL) {
		printf_dbg(" Source access interrupted\n");
		return -1;
	}

	struct dirent *cwd;
	struct stat dir;

	while((cwd = readdir(d1)) != NULL)
	{
		char *tsource = (char *)malloc(strlen(source_path)+strlen(cwd->d_name)+2);
		char *tdestination = (char *)malloc(strlen(destination_path)+strlen(fname)+1);

		//printf_dbg(" tsource = %s\n", tsource);
		//printf_dbg(" t..fname = %s\n", fname);
		//printf_dbg(" tdestination = %s\n", tdestination);

		strcpy(tdestination,destination_path);
		//printf_dbg("  tdestination = %s <<+ destination : %s \n", tdestination, destination_path);
		strcat(tdestination,fname);
		//printf_dbg("  tdestination = %s <<+ fname : %s \n", tdestination, fname);

		//printf_dbg("  tsource = %s\n", tsource);
		strcpy(tsource, source_path);

		//printf_dbg("  tsource = %s <<+ source : %s \n", tsource, source_path);
		strcat(tsource,"/");
		strcat(tsource, cwd->d_name);
		//printf_dbg("  tsource = %s <<+ source : %s \n", tsource, cwd->d_name);

		if(isdir(tsource))
		{
			if(strcmp(cwd->d_name,".") == 0 || strcmp(cwd->d_name,"..") == 0) 
        continue;
      
			copy_dir(tsource,tdestination);
      continue;
		}
		stat(tsource,&dir);
		//size = size + dir.st_size;
		copy_file(tsource, tdestination);

		/////////////////////////////////////////
		//printf_dbg(" tsource = %s\n", tsource);
		//printf_dbg(" tdestination = %s\n", tdestination);
		chmod(tdestination, 0755);
		chown(tdestination, 1000, 1000);   // system user
		/////////////////////////////////////////
	}

	closedir(d1);

	return 0;
}

int copy_file(const char* source_file, const char* destination_file)
{
	float total_file_size = 0;
	float copy_file_size = 0;
	int percent_prev_value = 0;

	char total_type[10];
	char copy_type[10];

	struct stat sourcefstat;
	struct stat destinationfstat;
	
	FILE* fp1 = NULL;
	FILE* fp2 = NULL;
	unsigned int readCount = 0, writeCount = 0;
	char buf[RW_BUFF_SIZE];
	int ret = 0;

	if(!isdir(source_file) && isdir(destination_file))
	{
		char *fname = strrchr(source_file,'/');
		destination_file = realloc(destination_file,strlen(destination_file)+strlen(source_file)+2);
		if(destination_file == NULL)
		{
			printf("Memory reallocation error\n");
			return -1;
		}
		strcat(destination_file, fname);
	}

	fp1 = fopen(source_file, "r");
	if (!fp1)
	{
		printf_dbg("Open From  %s failed. Abort.\n", source_file);
		return -1;
	}

	fp2 = fopen(destination_file, "w");
	if (!fp2)
	{
		printf_dbg("Open To %s failed. Abort.\n", destination_file);
		return -1;
	}

	while( (readCount = fread(buf, 1, RW_BUFF_SIZE, fp1))> 0)
	{
		writeCount = fwrite(buf, 1, readCount, fp2);
		if (writeCount != readCount) {
			printf_dbg(" read %d, but write %d, abort.\n", readCount, writeCount);
			return -1;
		}
	}

	fclose(fp1);
	fflush(fp2);
	fsync(fileno(fp2));
	fclose(fp2);

	return 0;
}

int check_FoldersCopy(const char *source, const char *destination)
{
	int retError = 0;

	time_t start = time(NULL);

	if(isdir(source)&&!isdir(destination))//To eliminate case 3]
	{
		printf_dbg(" Destination cannot be a file\n");
		exit(0);
	}

	if(isdir(source) && isdir(destination)) {
		//Case 4] requires traversing
		//printf_dbg(" check_FoldersCopy source = %s\n", source);
		//printf_dbg(" check_FoldersCopy destination = %s\n", destination);
		retError = copy_dir(source, destination);
	} else {
		retError = copy_file(source, destination);//Case 1] and 2] no traversing required, direct copyfile

		if(retError == 0) {
			/////////////////////////////////////////
			//printf_dbg(" copy source = %s\n", source);
			//printf_dbg(" copy destination = %s\n", destination);
			chmod(destination, 0755);
			chown(destination, 1000, 1000);   // system user
			/////////////////////////////////////////
		} else {
			printf_dbg(" check_FoldersCopy >> copy failure !!\n");
		}
	}
	return retError;
}

long PT_CopyFile(
	const char*	strFromPath,
	const char*	strToPath)
{
	FILE* fp1 = NULL;
	FILE* fp2 = NULL;
	unsigned int readCount = 0, writeCount = 0;
	char buf[RW_BUFF_SIZE];
	int ret = 0;

	if (!strFromPath || !strToPath)
	{
		printf_dbg("NULL file name find. Abort.\n");
		return -1;			//should never happen
	}

	fp1 = fopen(strFromPath, "r");
	if (!fp1)
	{
		printf_dbg(" Open %s ENOENT %d\n", strFromPath, errno);
		return E_RB_OPENFILE_ONLYR;
	}

	fp2 = fopen(strToPath, "w");
	if (!fp2)
	{
		printf_dbg(" Open %s ENOENT %d\n", strToPath, errno);
		return E_RB_OPENFILE_ONLYR;
	}

	while( (readCount = fread(buf, 1, RW_BUFF_SIZE, fp1))> 0)
	{
		writeCount = fwrite(buf, 1, readCount, fp2);
		if (writeCount != readCount)
		{
			printf_dbg(" read %d, but write %d, abort.\n", readCount, writeCount);
			printf_dbg("==> write errno = (%s)\n", strerror(errno));
			ret = E_RB_WRITE_ERROR;
 			break;
		}
	}

	fclose(fp1);
	fflush(fp2);
	fsync(fileno(fp2));
	fclose(fp2);

	return ret;
}

/*!
 *******************************************************************************
 * Delete file.<p>
 *
 * Must return error if the file does not exist.
 *
 * \param	pbUserData	Optional opaque data-structure to pass to IPL
 *						functions
 * \param	strPath		Path to file
 *
 * \return	S_RB_SUCCESS on success, E_RB_DELETEFILE if the file cannot be
 *			deleted, or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */
long RB_DeleteFile(
	void*		pbUserData,
	const char*	strPath)
{
	int ret = 0;

	ret = unlink(strPath);
	if (ret < 0 && errno != ENOENT)	//if file does not exist then we can say that we deleted it successfully
	{
		printf("delete failed with return value: %d errno %d\n",ret, errno);
		return E_RB_DELETEFILE;
	}
	return S_RB_SUCCESS;
}

/*!
 *******************************************************************************
 * Delete folder.<p>
 *
 * Must return success if the folder does not exist.
 *
 * \param	pbUserData	Optional opaque data-structure to pass to IPL
 *						functions
 * \param	strPath		Path to folder
 *
 * \return	S_RB_SUCCESS on success or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */
long RB_DeleteFolder(
	void*		pbUserData,
	const char*	strPath)
{
	int ret = 0;

	ret = rmdir(strPath);
	if ((ret == 0) || ((ret < 0) && ((errno == ENOENT) || (errno == ENOTEMPTY ))))
		return S_RB_SUCCESS;
	printf("Delete folder problem value: %d, errno: %d\n", ret, errno);
 	return E_RB_FAILURE;
}

/*!
 *******************************************************************************
 * Create folder.<p>
 *
 * Must return success if the folder already exists. It is
 * recommended that the new folder's attributes are a copy of its parent's
 * attributes. 
 *
 * \param	pbUserData	Optional opaque data-structure to pass to IPL
 *						functions
 * \param	strPath		Path to folder
 *
 * \return	S_RB_SUCCESS on success or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */
long RB_CreateFolder(
	void*		pbUserData,
	const char*	strPath)
{
	mode_t mode = 0;
	int ret = 0;

	mode =
		S_IRUSR /*Read by owner*/ |
		S_IWUSR /*Write by owner*/ |
		S_IXUSR /*Execute by owner*/ |
		S_IRGRP /*Read by group*/ |
		S_IWGRP /*Write by group*/ |
		S_IXGRP /*Execute by group*/ |
		S_IROTH /*Read by others*/ |
		S_IWOTH /*Write by others*/ |
		S_IXOTH /*Execute by others*/;

	ret = mkdir(strPath, mode);

	if (ret == 0 || ((ret == -1) && (errno == EEXIST)))
	{
		return S_RB_SUCCESS;
	}
	else if((ret == -1) && (errno == ENOENT))//maybe multi directory problem
	{
		ret = RecursiveFolderCreater(strPath, mode);
		if(ret == 0)
		{
			ret = mkdir(strPath, mode);//After creating all the depth Directories we try to create the Original one again.
			if(ret == 0)
				return S_RB_SUCCESS;
			else
			{
				printf("Create folder problem value: %d, errno: %d\n", ret, errno);
				return E_RB_FAILURE;
			}
		}
		else
		{
			printf("Create folder problem value: %d, errno: %d\n", ret, errno);
			return E_RB_FAILURE;
		}
	}
	else
	{
		printf("Create folder problem value: %d, errno: %d\n", ret, errno);
		return E_RB_FAILURE;
	}
}



/*!
 *******************************************************************************
 * Open file.<p>
 *
 * Must create the the file (and the path to the file) if it doesn't exist. Must
 * open in binary mode.
 *
 * \param	pbUserData	Optional opaque data-structure to pass to IPL
 *						functions
 * \param	strPath		Path to file
 * \param	wFlag		Read/write mode, an \ref E_RW_TYPE value
 * \param	pwHandle	(out) File handle
 *
 * \return	S_RB_SUCCESS on success, E_RB_OPENFILE_ONLYR if attempting to open a
 *			non-existant file in R/O mode, E_RB_OPENFILE_WRITE if there is an
 *			error opening a file for writing, or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */

int get_flags(E_RW_TYPE wFlag)
{
	switch (wFlag)
	{
		case ONLY_R:
			printf_dbg(" RDONLY \n");
			return O_RDONLY;
		case ONLY_W:
			printf_dbg(" WRONLY \n");
			return O_WRONLY | O_CREAT | O_TRUNC;
		case BOTH_RW:
			printf_dbg(" RDWR %d %d %d\n",O_RDWR ,O_CREAT,O_RDWR | O_CREAT);
			return O_RDWR | O_CREAT;
		default:
			printf_dbg(" Unknown \n");
			return 0;
	}
}

long RB_OpenFile(
	void*		pbUserData,
	const char*	strPath,
	E_RW_TYPE	wFlag,
	long*		pwHandle)
{
    int flags;

    flags = get_flags(wFlag);

    *pwHandle = open(strPath, flags, 0666);
  
	if(*pwHandle == -1) {
        *pwHandle = 0;
        printf_dbg(" First open() with error %d\n", errno);

        if(wFlag == ONLY_R)
            return E_RB_OPENFILE_ONLYR;

		//if we need to open the file for write or read/write then we need to create the folder (in case it does not exist)
		if((wFlag != ONLY_R) && (errno == ENOENT)) {
			char dir[RB_MAX_PATH] = {'\0'};
			int i = 0;
			//copy the full file path to directory path variable
			while (strPath[i] != '\0') {
				dir[i] = strPath[i];
				i++;
			}

			//search for the last '/' char
			while (dir[i--] != '/')
				;
			dir[i+1] = '\0';

			if (RB_CreateFolder(pbUserData, dir)) {
				printf_dbg(" Fail create folder, Leave RB_OpenFile\n");
				return E_RB_OPENFILE_WRITE;
			}
      
            *pwHandle = open(strPath, flags);
			if(*pwHandle == -1 || *pwHandle == 0) {
				*pwHandle = 0;
				printf(" After successful creating folder, fail open() with error %d\n", errno);
				return E_RB_OPENFILE_WRITE;
			}
		}
		else
			return E_RB_OPENFILE_WRITE;
 	}
	return S_RB_SUCCESS;
}


/*!
 *******************************************************************************
 * Set file size.
 *
 * \param	pbUserData	Optional opaque data-structure to pass to IPL
 *						functions
 * \param	wHandle		File handle
 * \param	dwSize		New file size
 *
 * \return	S_RB_SUCCESS on success or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */
 long RB_ResizeFile(
	void*			pbUserData,
	long			wHandle,
	RB_UINT64	dwSize)
{
	int ret = -1;

	ret = ftruncate64(wHandle, dwSize);

	if (ret)
		ret = E_RB_RESIZEFILE;

	return ret;
}

/*!
 *******************************************************************************
 * Close file.
 *
 * \param	pbUserData	Optional opaque data-structure to pass to IPL
 *						functions
 * \param	wHandle		File handle
 *
 * \return	S_RB_SUCCESS on success or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */
long RB_CloseFile(
	void*	pbUserData,
	long 	wHandle)
{
	int ret = E_RB_CLOSEFILE_ERROR;

	ret = close(wHandle);

	if(ret == 0)
		return S_RB_SUCCESS;

	return E_RB_CLOSEFILE_ERROR;
}

/*!
 *******************************************************************************
 * Write data to a specified position within a file.<p>
 *
 * Must return success if the block is written or at least resides in
 * non-volatile memory. Use \ref RB_SyncFile to commit the file to storage.
 *
 * \param	pbUserData	Optional opaque data-structure to pass to IPL
 *						functions
 * \param	wHandle		File handle
 * \param	dwPosition	Position within the file to which to write
 * \param	pbBuffer	Data to write
 * \param	dwSize		Size of \a pbBuffer
 *
 * \return	S_RB_SUCCESS on success, or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */
long RB_WriteFile(
	void*			pbUserData,
	long			wHandle,
	RB_UINT64	dwPosition,
	unsigned char*	pbBuffer,
	RB_UINT32	dwSize)
{
	int ret = 0;
	RB_UINT32 size = 0;

	size = lseek64(wHandle, 0, SEEK_END);
	/* from the guide: if dwPosition is beyond size of file the gap between end-of-file and the position should be filled with 0xff */
	if (size < dwPosition)
	{
		int heap_size = dwPosition - size;
		unsigned char* p_heap = (unsigned char*)malloc(heap_size);

		if (p_heap == NULL)
		{
			printf_dbg("malloc failure (p_heap).\n");
      return E_RB_WRITE_ERROR;
		}
		memset(p_heap, 0xFF, heap_size);
		ret = write(wHandle, p_heap, heap_size);
		free(p_heap);
		if (ret < 0 || ret != heap_size)
		{
			printf_dbg("write failed with return value: %d\n",ret);
      return E_RB_WRITE_ERROR;
		}
	}
	ret = lseek64(wHandle, dwPosition, SEEK_SET);
	if (ret < 0)
	{
		printf_dbg("lseek failed with return value: %d\n",ret);
		return E_RB_WRITE_ERROR;
	}

	ret = write(wHandle, pbBuffer, dwSize);
	if (ret < 0 || (ret != (int)dwSize))
	{
		printf_dbg("Failed with return value: %d, (%s)\n",ret, strerror(errno));
		return E_RB_WRITE_ERROR;
	}
	printf_dbg("Bytes Write: %d\n",ret);

	return S_RB_SUCCESS;
}

/*!
 *******************************************************************************
 * Read data from a specified position within a file.
 * If fewer bytes than requested are available in the specified position, this
 * function should read up to the end of file and return S_RB_SUCCESS.
 *
 * \param	pbUserData	Optional opaque data-structure to pass to IPL
 *						functions
 * \param	wHandle		File handle
 * \param	dwPosition	Position within the file from which to read
 * \param	pbBuffer	Buffer to contain data
 * \param	dwSize		Size of data to read
 *
 * \return	S_RB_SUCCESS on success or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */
long RB_ReadFile(
	void*			pbUserData,
	long			wHandle,
	RB_UINT64		dwPosition,
	unsigned char*	pbBuffer,
	RB_UINT32		dwSize)
{
	long ret = 0;
	RB_UINT64 tmpSize = 0;

	ret = lseek64(wHandle, dwPosition, SEEK_SET);
	if (ret == -1)
	{
		printf_dbg("lseek failed with return value: %ld errono %d\n",ret, errno);
		return E_RB_READ_ERROR;
	}

	ret = read(wHandle, pbBuffer, dwSize);
	if (ret < 0)
	{
		printf(" read failed with return value: %ld, error = %d\n",ret, errno);
		return E_RB_READ_ERROR;
	}

	if ((ret != (int)dwSize) && (((RB_UINT64)ret + dwPosition) != RB_GetFileSize(pbUserData, wHandle)))
	{
		printf("====  read failed\n");
		return E_RB_READ_ERROR;
	}
	return S_RB_SUCCESS;
}

/*!
 *******************************************************************************
 * Get file size.
 *
 * \param	pbUserData	Optional opaque data-structure to pass to IPL
 *						functions
 * \param	wHandle		File handle
 *
 * \return	File size, -1 if file not found, or &lt; -1 on error
 *******************************************************************************
 */
RB_UINT64 RB_GetFileSize(
	void*	pbUserData,
	long	wHandle)
{
	struct stat statbuf;

	fstat(wHandle, &statbuf);
	return statbuf.st_size;
}

/*!
 *******************************************************************************
 * Remove symbolic link.<p>
 *
 * Must return success if the deleted object does not exist or is not a symbolic link.<p>
 *
 * If your platform does not support symbolic links, you do not need to
 * implement this.
 *
 * \param	pbUserData	Optional opaque data-structure to pass to IPL
 *						functions
 * \param	pLinkName	Path to symbolic link
 *
 * \return	S_RB_SUCCESS on success or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */
long RB_Unlink(
	void*	pbUserData,
	const char*	pLinkName)
{
	int ret = 0;

	ret = unlink(pLinkName);
	if(ret < 0 && errno != ENOENT)
	{
		printf("unlink failed with return value: %d errno %d\n",ret, errno);
		return E_RB_FAILURE;
	}

	return S_RB_SUCCESS;
}

long RB_VerifyLinkReference(
	void*		pbUserData,
	const char*	pLinkName,
	const char*	pReferenceFileName)
{
	int ret = 0;
	char linkedpath[RB_MAX_PATH]={'\0'};

	ret = readlink(pLinkName, linkedpath, RB_MAX_PATH);
	if (ret < 0)
	{
		printf("readlink failed with return value: %d\n",ret);
		return E_RB_FAILURE;
	}

	if ((memcmp(linkedpath, pReferenceFileName, ret))!=0)
	{
		printf("not same linked path - linkedpath[%s] pReferenceFileName[%s]\n", linkedpath, pReferenceFileName);
		return E_RB_FAILURE;
	}

	return S_RB_SUCCESS;
}

/*!
 *******************************************************************************
 * Create symbolic link.<p>
 *
 * Must create the path to the link as required. If a file already exists at the
 * named location, must return success if the file is a symbolic link or an
 * error if the file is a regular file. The non-existance of the target of the
 * link must NOT cause an error.<p>
 *
 * If your platform does not support symbolic links, you do not need to
 * implement this.
 *
 * \param	pbUserData			Optional opaque data-structure to pass to IPL
 *								functions
 * \param	pLinkName			Path to the link file to create
 * \param	pReferenceFileName	Path to which to point the link
 *
 * \return	S_RB_SUCCESS on success or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */
 long RB_Link(
	void*		pbUserData,
	const char*	pLinkName,
	const char*	pReferenceFileName)
{
	int ret = 0;

	if(RB_VerifyLinkReference(pbUserData, pLinkName, pReferenceFileName) == S_RB_SUCCESS)
		return S_RB_SUCCESS;

	ret = symlink(pReferenceFileName, pLinkName);
	if (ret != 0)
	{
		if (errno == EEXIST && RB_VerifyLinkReference(pbUserData, pLinkName, pReferenceFileName) == S_RB_SUCCESS)
		{
			return S_RB_SUCCESS;
		}
		return E_RB_FAILED_CREATING_SYMBOLIC_LINK;
	}

	return S_RB_SUCCESS;
}


static int hex_digit(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	else if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	else if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	else
		return -1;
}

#define ISOCTAL(v) ((v) && (v) >= '0' && (v) <= '7')
static int xattr_text_decode(const char *value, char *decoded)
{
	char *d = decoded, *v = (char*)value;

	while(*v)
	{
		if (v[0] == '\\')
		{
			if (v[1] == '\\' || v[1] == '\"')
			{
				*d++ = *++v; v++;
			}
			else if (ISOCTAL(v[1]) &&
				ISOCTAL(v[2]) && ISOCTAL(v[3]))
			{
				*d++ = ((v[1] - '0') << 6) +
					   ((v[2] - '0') << 3) +
					   ((v[3] - '0'));
				v += 4;

			}
			else
				return -1;
		}
		else
			*d++ = *v++;
	}
	return (d-decoded);
}

static int xattr_hex_decode(const char *value, char *decoded)
{
	char *d = decoded, *end, *v = (char*)value;

	end = (char*)value + strlen(value);

	while (v < end)
	{
		int d1, d0;
		d1 = hex_digit(*v++);
		if (!*v)
			return -1;
		d0 = hex_digit(*v++);
		if (d1 < 0 || d0 < 0)
			return -1;

		*d++ = ((d1 << 4) | d0);
	}
	return (d - decoded);
}

#define _REDBEND_PREFIX "_redbend_"
#if (!defined(ANDROID) || defined(ANDROID_KK)) && !defined(QNX)
static int removeXattr(const char *fileName)
{
	char listXattr[1024];
	char *curXattr = NULL;
	int xattrRet = llistxattr(fileName, listXattr, 1024);

	if (xattrRet < 0)
		return -1;
	curXattr = listXattr;
	while (curXattr < listXattr + xattrRet )
	{
		if (strcmp(curXattr, "security.selinux") &&
			lremovexattr(fileName, curXattr) < 0)
		{
			printf_dbg("Error - lremovexattr failed for %s. Errno %d\n",
					fileName, errno);
			return -1;
		}
		curXattr += strlen(curXattr) + 1;
	}
	return 0;
}
#endif
static int xattrDecode (char *encoded, char *decoded, size_t *dsz)
{
	size_t osz = strlen(encoded);

	if (encoded[0]=='"' && encoded[osz-1] == '"')
	{
		encoded[osz-1] = '\0';
		*dsz = xattr_text_decode(++encoded, decoded);
	}
	else if (encoded[0] == '0' && encoded[1] == 'x')
		*dsz = xattr_hex_decode(encoded+2, decoded);
	else
	{
		printf("Error - Invalid encoded\n");
		return -1;
	}
	if (*dsz == (size_t)-1)
	{
		printf("Error - Invalid encoded\n");
		return -1;
	}
	decoded[*dsz] = '\0';
	return 0;
}

/*!
 *******************************************************************************
 * Set file attributes.<p>
 *
 * The file attributes token (\a ui8pAttribs) is defined at generation time.
 * If attributes are not defined explicitly, they are given the following, 
 * OS-dependent values:
 * \li	Windows: _redbend_ro_ for R/O files, _redbend_rw_ for R/W files
 * \li	Linux: _redbend_oooooo:xxxx:yyyy indicating the file mode, uid, and gid
 *		(uid and gid use capitalized hex digits as required)
 *
 * \param	pbUserData		Optional opaque data-structure to pass to IPL
 *							functions
 * \param	pFilePath		File path
 * \param	attribSize		Size of \a ui8pAttribs 
 * \param	pAttribs		Attributes to set
 *
 * \return	S_RB_SUCCESS on success or &lt; 0 on error
 *******************************************************************************
 */
long RB_SetFileAttributes(
	void*				 pbUserData,
	const char*			 FilePath,
	const unsigned int   ui32AttribSize,
	const unsigned char* ui8pAttribs)
{
	int mode, uid, gid, i;
	struct stat sbuf;
	int ret = E_RB_FAILURE;
	// debug start
	int count = 0;
	char *localAttrib = NULL, *decoded_val = NULL, *decoded_key = NULL;
	char *pattr;

	// Skip the redbend prefix
	if (!strncmp((char *)ui8pAttribs, _REDBEND_PREFIX, sizeof(_REDBEND_PREFIX)-1))
		ui8pAttribs += sizeof(_REDBEND_PREFIX)-1;

	count = sscanf((char *)ui8pAttribs, "%6o:%4x:%4x",
			(unsigned int*)&mode, &uid, &gid);

	// Maybe _redbend_ro/w
	if (count != 3)
	{
		char s[3];
		count = sscanf((char *)ui8pAttribs, "%2s", s);
		if (count == 1 && (!strcmp(s, "ro") || !strcmp(s, "rw")))
		{
			ret = S_RB_SUCCESS; // Just ignored
		}
		goto End;
	}

	if (lstat(FilePath, &sbuf))
	{
		printf("Error - lstat failed. Errno %d\n", errno);
		goto End;
	}

	if(lchown(FilePath, (uid_t)uid, (gid_t)gid))
	{
		printf("Error - lchown failed. Errno %d\n", errno);
		goto End;
	}

	if (!S_ISLNK(sbuf.st_mode) && chmod(FilePath, (mode_t)mode))
	{
		printf("Error - chmod failed. Errno %d\n", errno);
		goto End;
	}

	localAttrib = strdup((char*)ui8pAttribs);
	for (i = 1, pattr = strtok(localAttrib, ":");
		pattr && (i < 3); i++)
	{
		ui8pAttribs += strlen(pattr) + 1; // 1 for colon
		pattr = strtok(NULL, ":");
	}
	if (i < 3)
	{
		printf("Bad format for attributes %s\n", ui8pAttribs);
		goto End;
	}
	if (pattr)
		ui8pAttribs += strlen(pattr);

	if (ui8pAttribs[0] != ':')
	{
		ret = S_RB_SUCCESS;
		goto End;
	}
	ui8pAttribs++;
#if (!defined(ANDROID) || defined(ANDROID_KK)) && !defined(QNX)
	if (removeXattr(FilePath) < 0)
	{
		printf("Failed to remove all xattr for %s\n", FilePath);
		goto End;
	}
#endif
    if (ui8pAttribs[0])
    {
		char *saveptr, *xattr;
		if (localAttrib)
			free(localAttrib);
		localAttrib = strdup((char*)ui8pAttribs);
		decoded_val = (char*)malloc(XATTR_SIZE_MAX);
		decoded_key = (char*)malloc(XATTR_SIZE_MAX);
		if (!localAttrib || !decoded_val || !decoded_key)
		{
			printf("Error - Unable to allocated memory for xattr\n");
			goto End;
		}

		for (xattr = strtok_r((char*)localAttrib, ";", &saveptr); xattr;
				xattr = strtok_r(NULL, ";", &saveptr))
		{
			char *key, *value;
			size_t dsz;
			key = strtok(xattr, "=");
			value = xattr + strlen(key) + 1;
			if (xattrDecode(key, decoded_key, &dsz))
				goto End;
			if (xattrDecode(value, decoded_val, &dsz))
				goto End;

#if (!defined(ANDROID) || defined(ANDROID_KK)) && !defined(QNX)
			if (lsetxattr(FilePath, decoded_key, decoded_val, dsz, 0) < 0)
			{
				printf("Error - lsetxattr failed. Errno %d\n", errno);
				goto End;
			}
#else
			printf("lsetxattr is not defined, skipping %s=%s\n", key, value);
#endif
		}
    }

	ret = S_RB_SUCCESS;

End:
	if (localAttrib)
		free(localAttrib);

	if (decoded_val)
		free(decoded_val);
	if (decoded_key)
		free(decoded_key);

	if(ret != S_RB_SUCCESS)
	{
		printf ("RB_SetFileAttributes failed. %s %s\n",	FilePath, ui8pAttribs);
	}
	else
	{
		printf("RB_SetFileAttributes %s: %o %d %d\n", FilePath, mode, uid, gid);
	}
	return ret;
}

/*!
 ************************************************************
 *                     RB_FSGetDelta
 ************************************************************
 *
 * @brief
 *	Reads data from the delta.
 *
 *	A glue function that needs to be implemented by the customer.
 *
 *	It should follow the following restrictions:
 *
 *	1. Returning the proper error level \see RB_vRM_Errors.h
 *
 *	@param pbBuffer					The gives buffer that the data from the open file should be.
 *									copy into.
 *
 *	@param dwStartAddressOffset		The offset in the read file that should be
 *									the starting point for the copy.
 *
 *	@param dwSize					The size in bytes that should be copied from the open file,
 *									starting from the given position offset.
 *
 *	@return							One of the return codes as defined in RB_vRM_Errors.h
 *
 ************************************************************
 */
long RB_GetDelta(
	void*			pbUserData,				/* User data passed to all porting routines */
	unsigned char*		pbBuffer,				/* pointer to user buffer */
	RB_UINT32		dwStartAddressOffset,	/* offset from start of delta file */
	RB_UINT32		dwSize)
{
	return RB_ReadFile(pbUserData, h_delta, dwStartAddressOffset, pbBuffer, dwSize);
}

/**
 *******************************************************************************
 * Get update from DP.<p>
 *
 * This is a Porting Layer function that must be implemented.
 * A default implementation of this function is provided.
 *
 * \param	pbUserData				Optional opaque data-structure to pass to IPL
 *									functions
 * \param	pbBuffer				(out) Buffer to store the update
 * \param	dwStartAddressOffset	Update offset in DP
 * \param	dwSize					Size of \a pbBuffer
 *
 * \return	S_RB_SUCCESS on success or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */
long RB_GetDelta64(
	void *pbUserData,
	unsigned char *pbBuffer,
	RB_UINT64 llStartAddressOffset,
	RB_UINT32 dwSize)
{
	return RB_ReadFile(pbUserData, (long)h_delta, llStartAddressOffset, pbBuffer, dwSize);
}

/**
 *******************************************************************************
 * Display progress information to the end-user.<p>
 *
 * This is a Porting Layer function that must be implemented.
 *
 * Actually, you can do whatever you want with the progress information.
 *
 * \param	pbUserData	Optional opaque data-structure to pass to IPL
 *						functions
 * \param	uPercent	Update percentage
 *
 * \return	None
 *******************************************************************************
 */
void RB_Progress(
		void*			pbUserData,
		RB_UINT32	uPercent)
{
	//To do: display the progress bar
}

/*!
 *******************************************************************************
 * Print status and debug information.
 *
 * \param	pbUserData	Optional opaque data-structure to pass to IPL
 *						functions
 * \param	aFormat		A NULL-terminated printf-like string with support for
 *						the following tags:
 *						\li %x:  Hex number
 *						\li %0x: Hex number with leading zeros
 *						\li %u:  Unsigned decimal
 *						\li %s:  NULL-terminated string
 * \param	...			Strings to insert in \a aFormat
 *
 * \return	S_RB_SUCCESS on success or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */
RB_UINT32 RB_Trace(
	void*		pbUserData,
	const char*	aFormat,
	...)
{
	va_list list;
	int ret = -1;

	va_start(list, aFormat);
	vprintf(aFormat, list);
	va_end(list);
	return S_RB_SUCCESS;
}

/*!
 *******************************************************************************
 * Move (rename) file.<p>
 * 
 * Must return error if strFromPath does not exist or if strToPath exists.
 *
 * \param	pbUserData	Optional opaque data-structure to pass to IPL
 *						functions
 * \param	strFromPath	Path to old file location
 * \param	strToPath	Path to new file location
 *
 * \return	S_RB_SUCCESS on success or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */
long RB_MoveFile(
	void*		pbUserData,
	const char* strFromPath,
	const char* strToPath)
{
	long ret = 0;
	long pwHandle = 0;

	if (!strFromPath || !strToPath)
	{
		printf_dbg("NULL file name find. Abort.\n");
		return -1;			//should never happen
	}

	if (RB_OpenFile(pbUserData, strToPath, ONLY_R, &pwHandle) == S_RB_SUCCESS)
	{
		RB_CloseFile(pbUserData, pwHandle);
		return S_RB_SUCCESS;
	}

	ret = rename (strFromPath,strToPath);
	if (ret < 0)
	{
		printf ("failed to rename file %s: %s -> %s ", __func__, strFromPath, strToPath);
		return -2;
	}
	return S_RB_SUCCESS;
}

/*!
 *******************************************************************************
 * Commit file to storage.<p>
 *
 * Generally called after \ref RB_WriteFile.
 *
 * \param	pbUserData	Optional opaque data-structure to pass to IPL
 *						functions
 * \param	wHandle		File handle
 *
 * \return	S_RB_SUCCESS on success or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */
long RB_SyncFile(
	void*	pbUserData,
	long	wHandle)
{
	int ret = -1;
	ret = fsync(wHandle);
	if (ret < 0)
	{
		printf_dbg("fsync Failed with return value: %d\n",ret);
		return E_RB_WRITE_ERROR;
	}

	return S_RB_SUCCESS;
}

long RB_SyncAll(
	void* pUser,
	const char* strPath)
{
	sync();
	// from sync man page : sync() is always successful.
	return S_RB_SUCCESS;
}

long RB_ResetTimerA(void)
{
		printf("%s \n", __func__);
		return S_RB_SUCCESS;
}

static inline long check_time(void)
{
	struct timeval tv;
	gettimeofday (&tv, NULL);
	return (tv.tv_sec * 1000000 + tv.tv_usec );
}

int wait_child_ps(pid_t waiting_pid)
{
	int status;
	pid_t terminated_pid;

	terminated_pid = wait(&status);

	return status;
}

static int active_pt_copy_mount(const char *dev_mnt_point, const char *pt_name, const char *mnt_point)
{
  int ret; 
  char dev_mnt_ptn_path[DEV_MOUNT_PATH_SIZE];
  
  memset(dev_mnt_ptn_path, 0, DEV_MOUNT_PATH_SIZE);
  sprintf(dev_mnt_ptn_path, "%s/%s", dev_mnt_point, pt_name);
  
	ret = mount(dev_mnt_ptn_path, mnt_point, "ext4",
	               MS_NOATIME | MS_NODEV | MS_NODIRATIME | MS_RDONLY, "");
	if(ret < 0)
	{
    	printf_dbg("[%s] : ret = %d, errno = %d\n", __func__, ret, errno);
		return -1;
	}
  return 0;
}

static int act_pt_copy_to_inactive_pt(const char *dev_mnt_point, const char *pt_name)
{
	int ret;
	char src_path[DEV_MOUNT_PATH_SIZE], dest_path[DEV_MOUNT_PATH_SIZE];
  
	memset(src_path, 0, DEV_MOUNT_PATH_SIZE);
	memset(dest_path, 0, DEV_MOUNT_PATH_SIZE);

#if 1
	start_t = validity_clock();
	//ret = PT_CopyFile(dev_mnt_point, pt_name);
	ret = check_FoldersCopy(dev_mnt_point, pt_name);
	if(ret != 0)
	{
	printf_dbg("[%s] fmt fail : ret = %d, errno = %d\n", __func__, ret, errno);
	return ret;
	}
	end_t = validity_clock() - start_t;
	printf_dbg("==> copy time spend : %3ld.%03ld,%03ld sec \n",  end_t/1000000, end_t/1000, end_t%1000);

#else
  if(boot_active_partition == 0)
  {
	  sprintf(src_path, "%s/%s", dev_mnt_point, pt_name);
	  sprintf(dest_path, "%s/%s%s", dev_mnt_point, pt_name, "_mirror");
  }
  else
  {
	  sprintf(src_path, "%s/%s%s", dev_mnt_point, pt_name, "_mirror");
	  sprintf(dest_path, "%s/%s", dev_mnt_point, pt_name);
  }

  printf_dbg("src_path = %s\n", src_path);
  printf_dbg("dest_path = %s\n", dest_path);

	start_t = validity_clock();
  ret = PT_CopyFile(src_path, dest_path);
  if(ret != 0)
  {
    printf_dbg("[%s] fmt fail : ret = %d, errno = %d\n", __func__, ret, errno);
    return ret;
  }
  end_t = validity_clock() - start_t;
  printf_dbg("==> copy time spend : %3ld.%03ld,%03ld sec \n",  end_t/1000000, end_t/1000, end_t%1000);
#endif
  return S_RB_SUCCESS;
}


static int inactive_ptn_mount(const char *dev_mnt_point, const char *pt_name, const char *mnt_point)
{
    int ret;
    char dev_mnt_ptn_path[DEV_MOUNT_PATH_SIZE] = {0, };

    sprintf(dev_mnt_ptn_path, "%s/%s", dev_mnt_point, pt_name);

    sleep(1);

	ret = mount(dev_mnt_ptn_path, mnt_point, "ext4", 0, "");
	if(ret < 0) {
        printf_dbg("[%s] : ret = %d, errno = %d\n", __func__, ret, errno);
        return -1;
	}

    return S_RB_SUCCESS;
}

void _do_ext4_inactive_pnt_fmt(const char *dev_mnt_point, const char *pt_name)
{
	int ret;
  char dev_mnt_ptn_path[DEV_MOUNT_PATH_SIZE];
  
  memset(dev_mnt_ptn_path, 0, DEV_MOUNT_PATH_SIZE);

  if(boot_active_partition == 0)
	  sprintf(dev_mnt_ptn_path, "%s/%s%s", dev_mnt_point, pt_name, "_mirror");
  else
	  sprintf(dev_mnt_ptn_path, "%s/%s", dev_mnt_point, pt_name);

  ret = execl("/system/bin/make_ext4fs", "make_ext4fs", dev_mnt_ptn_path, NULL);

  exit(ret);
}

void _do_dd_pt_erase(const char *pt_name)
{
    int ret; 
    char if_root[DEV_MOUNT_PATH_SIZE], of_root[DEV_MOUNT_PATH_SIZE];

    memset(if_root, 0, DEV_MOUNT_PATH_SIZE);
    memset(of_root, 0, DEV_MOUNT_PATH_SIZE);

    if(boot_active_partition == 0) {
        sprintf(of_root, "of=%s/%s%s", dev_blk_path, pt_name, "_mirror");
    } else {
        sprintf(of_root, "of=%s/%s", dev_blk_path, pt_name);
    }

    sprintf(if_root, "if=%s", "/dev/zero");

    ret = execl("/system/bin/dd", "dd", if_root, of_root, "bs=4096", NULL);

    exit(ret);
}

void _do_dd_pt_erase_copy(const char *pt_name)
{
    int ret;
    char if_root[DEV_MOUNT_PATH_SIZE], of_root[DEV_MOUNT_PATH_SIZE];

    memset(if_root, 0, DEV_MOUNT_PATH_SIZE);
    memset(of_root, 0, DEV_MOUNT_PATH_SIZE);

    if(boot_active_partition == 0) {
        sprintf(if_root, "if=%s/%s", dev_blk_path, pt_name);
        sprintf(of_root, "of=%s/%s%s", dev_blk_path, pt_name, "_mirror");
	} else {
        sprintf(if_root, "if=%s/%s%s", dev_blk_path, pt_name, "_mirror");
        sprintf(of_root, "of=%s/%s", dev_blk_path, pt_name);
    }

    ret = execl("/system/bin/dd", "dd", if_root, of_root, "bs=4096", NULL);

    exit(ret);
}

static void _do_copy_binary_data(const char *dev_mnt_point, const char *pt_name)
{
  int ret = 0;
  char if_root[DEV_MOUNT_PATH_SIZE], of_root[DEV_MOUNT_PATH_SIZE];

  memset(if_root, 0, DEV_MOUNT_PATH_SIZE);
  memset(of_root, 0, DEV_MOUNT_PATH_SIZE);

  if(boot_active_partition == 0)
  {
	  sprintf(if_root, "if=%s/%s", dev_mnt_point, pt_name);
	  sprintf(of_root, "of=%s/%s%s", dev_mnt_point, pt_name, "_mirror");
	}
  else
  {
	  sprintf(if_root, "if=%s/%s%s", dev_mnt_point, pt_name, "_mirror");
	  sprintf(of_root, "of=%s/%s", dev_mnt_point, pt_name);
  }

  ret = execl("/system/bin/dd", "dd", if_root, of_root, NULL);
  
  printf("%s: if_root(%s), of_root(%s), ret(%d)\n", __func__, if_root, of_root, ret);
  
  exit(ret);
}

int fs_image_path_open(const char *dev_mnt_point)
{
    int i;

    char dev_mnt_ptn_path[DEV_MOUNT_PATH_SIZE];

    memset(dev_mnt_ptn_path, 0, DEV_MOUNT_PATH_SIZE);

    for(i = 0; i < FS_IMAGE_MAX; i++) {
        sprintf(dev_mnt_ptn_path, "%s/%s", dev_mnt_point, fsImage_pt_name[i]);

        FS_ImgHandle[i] = open(dev_mnt_ptn_path, O_RDWR);
        
        if(FS_ImgHandle[i] < 0) {
            printf_dbg("[%s] Input file open failed : %s)\n", __func__, strerror(errno));
            return -1;
        }
    }

    return S_RB_SUCCESS;
}

int fs_image_path_close(void)
{
    int i, ret;

    for(i = 0; i < FS_IMAGE_MAX; i++) {
        ret = close(FS_ImgHandle[i]);

        if (ret < 0) {
            printf_dbg("[%s] open file close failed, ret : %d(%s)\n", __func__, ret, strerror(errno));
            return E_RB_CLOSEFILE_ERROR;
        }
    }

    return S_RB_SUCCESS;
}

static int check_mounted(const char *mounted_path)
{
  int ret;
	/* 1. checking for usb mount or umount */
	scan_mounted_volumes();
	const MountedVolume* mv = find_mounted_volume_by_mount_point(mounted_path);
	if(mv) 
	{
	  printf("%s : volume is mounted\n", mounted_path);
    return 0;
	}
	else
	{
	  printf("%s : volume is not mounted\n", mounted_path);
    return -1;
	}
}

void execute_unmount_pt(const char *mounted_name)
{
    if(0 == check_mounted(mounted_name)) {
        printf("%s : volume is unmount\n", mounted_name);
        umount(mounted_name);
    }
}

void fota_update_result_info(char *result_string)
{
    int fota_fd;
    char fota_buf[32] = {0,};
    int mask;
    
    mask = umask(0);
    
    fota_fd = open(REC_FOTA_RESULT_INFO, O_CREAT | O_RDWR, 0666);
    sprintf(fota_buf, "%s", result_string);
    write(fota_fd, fota_buf, sizeof(fota_buf));
    umask(mask);
    close(fota_fd);
}
void execute_after_update(int result)
{
    int ret;
    execute_unmount_pt(system_mnt_point);
    execute_unmount_pt(vr1_mnt_point);
    execute_unmount_pt(vr2_mnt_point);

    fs_image_path_close();
    RB_DeleteFile(NULL, REC_DELTA_FILE_PATH);

    if(result == 0)
    {
        printf_dbg("RB FOTA update Success\n");
        fota_update_result_info(REC_FOTA_UPATE_SUCCESS);
        boot_partition_set(boot_active_partition); // boot partition set

		/* Normal Reboot */
		android_reboot(ANDROID_RB_RESTART2, 0, "force_normal");
    }
    else
    {
        printf_dbg("RB FOTA update Fail\n");
        fota_update_result_info(REC_FOTA_UPATE_FAIL);
    }
}

//struct selabel_handle *sehandle;
static int _do_dd_copy_function(const char *pt_name)
{
    int pid = 0;
    int ret; 
    int status = 0;

    start_t = check_time();
    pid = fork();
    if(pid ==0) {
        _do_dd_pt_erase_copy(pt_name);
    }
  
    status = wait_child_ps(pid);
    if (status) {
        printf_dbg("[%s] error : %d\n", __func__, status);
        return -1;
    }

    end_t = check_time() - start_t;

    return 0;
}

static int _do_dd_erase_function(const char *pt_name)
{
  int pid = 0;
  int ret; 
  int status = 0;
  
  start_t = validity_clock();
  pid = fork();
  if(pid ==0) {
      _do_dd_pt_erase(pt_name);
  }
  
  status = wait_child_ps(pid);
  if (status) {
     printf_dbg("[_do_dd_pt_erase] error(%d)\n", status);
      return -1;
  }
  end_t = validity_clock() - start_t;

  return 0;
}

static int ready_for_update(void)
{
    int pid = 0;
    int ret;
    int status = 0;

	/* file open for fs_image partition */
	ret = fs_image_path_open(dev_blk_path);
	if(ret < 0) {
        printf_dbg("fs_image_path_open failed.\n");
        return -1;
	}

    sync();
    sync();
    sync();

    return S_RB_SUCCESS;
}

static int boot_partition_set(unsigned cur_boot)
{
	int  dev_1;
	int  retval;

	struct boot_partition   active_p;
	int  readcnt = 0;
	int  i=0;
	int  flashing;
	char str_boot_mode[16];

	printf("setting partitions read / write start\n");
	memset(&active_p, 0, sizeof(active_p));
	memset(str_boot_mode, 0, sizeof(str_boot_mode));

	if(cur_boot == 0)
    sprintf(str_boot_mode, "%s", "boot_mirror");
  else
    sprintf(str_boot_mode, "%s", "boot");

	strlcpy(active_p.active_partition, str_boot_mode, sizeof(active_p.active_partition));

	dev_1 = open(SETTINGS_PARTITION_PATH, O_RDWR);
	if(dev_1 < 0)
	{
	  printf("err dev_int\n");
	  return -1;
	}

	lseek(dev_1, OFFSET_BOOT_ACTIVE_SETTINGS, SEEK_SET);
	write(dev_1, &active_p, sizeof(active_p));

	close(dev_1);

	return 0;
}

static unsigned check_active_partition()
{
	int dev_1;
	int retval;
	unsigned int mode = 0;				// default value

	static unsigned char sbSet_ = false;
	struct boot_partition active_p;

	dev_1 = open(SETTINGS_PARTITION_PATH, O_RDWR);
	if(dev_1 < 0) {
		printf_dbg("err dev_int\n");
		return -1;
	}

	lseek(dev_1, OFFSET_BOOT_ACTIVE_SETTINGS, SEEK_SET);
	read(dev_1, &active_p, sizeof(active_p));

	if(false == sbSet_) {
		if(0 == strcmp("boot", active_p.active_partition)) {
			mode = 0;
			printf_dbg("read_active_partition_setting active_partition boot\n");
		} else if(0 == strcmp("boot_mirror", active_p.active_partition)) {
			mode = 1;
			printf_dbg("read_active_partition_setting active_partition boot_mirror\n");
        } else {
			mode = 0;
			printf_dbg("read_active_partition_setting NOT SET!!! active_partition boot\n");
        }
		sbSet_= true;
	}
    close(dev_1);
	return mode;
}

int erase_partition_img(const char *dest_pt)
{
	int open_handle;
	int writeCnt = 0, readCnt = 0;
	unsigned char buf[RW_BUFF_SIZE];
  char dev_mnt_ptn_path[DEV_MOUNT_BLK_SIZE];
	int ret = 0;

  memset(dev_mnt_ptn_path, 0x00, DEV_MOUNT_BLK_SIZE);
  memset(buf, 0x00, RW_BUFF_SIZE);

  if(boot_active_partition == 0)
	  sprintf(dev_mnt_ptn_path, "%s/%s%s", dev_blk_path, dest_pt, "_mirror");
  else
    sprintf(dev_mnt_ptn_path, "%s/%s", dev_blk_path, dest_pt);

  printf("==> open erase partition : %s\n",dev_mnt_ptn_path);

  open_handle = open(dev_mnt_ptn_path, O_RDWR);
	if(open_handle < 0)
	{
	  printf("==> path open error (%s), [%s]\n",dev_mnt_ptn_path, strerror(errno));
	  return -1;
	}

  ret = lseek(open_handle, 0L, SEEK_SET);
	if (ret == -1)
	{
		printf("lseek failed with return value: %ld errono (%d)\n",ret, strerror(errno));
		return -1;
	}

  printf("==> %s erase start\n", dest_pt);
  start_t = check_time();
	while((readCnt = read(open_handle, buf, RW_BUFF_SIZE))> 0)
	{
    memset(buf, 0x00, RW_BUFF_SIZE);
    writeCnt = write(open_handle, buf, readCnt);
		if (writeCnt != readCnt)
		{
			printf(" read %d, but write %d, abort.\n", readCnt, writeCnt);
			printf("==> write error (%s)\n", strerror(errno));
			ret = -1;
 			break;
		}
	}
  end_t = check_time() - start_t;
  printf("==> erase time : %3ld.%03ld,%03ld sec \n",  end_t/1000000, end_t/1000, end_t%1000);

  close(open_handle);
	return ret;

}


int write_raw_partition_img(const char *dest_pt)
{
	int ret = 0, i = 0;
	int readCnt, writeCnt;
	FILE *fin = NULL, *fout = NULL;
	unsigned char buffer[ERASE_BUF_SIZE_MAX];
	char image_path[IMAGE_NAME_SIZE];
	char dev_mnt_ptn_path[DEV_MOUNT_BLK_SIZE];

	memset(image_path, 0x00, IMAGE_NAME_SIZE);
	memset(dev_mnt_ptn_path, 0x00, DEV_MOUNT_BLK_SIZE);

	for(i=0; i < SYSTEM_UD_PT_SIZE; i++)
	{
	  if(0 == strcmp(dest_pt, write_pt_check[i].pt_name))
	  {
	    sprintf(image_path, "%s/%s", update_folder_path, write_pt_check[i].pt_image_file);
	    break;
	  }
	}
	fin = fopen(image_path, "rb");
	if(fin == NULL)
	{
	  printf_dbg("input file(%s) open failed\n", image_path);
	  return -1;
	}

	if(boot_active_partition == 0)
	  sprintf(dev_mnt_ptn_path, "%s/%s%s", dev_blk_path, dest_pt, "_mirror");
	else
	  sprintf(dev_mnt_ptn_path, "%s/%s", dev_blk_path, dest_pt);

	fout = fopen(dev_mnt_ptn_path, "wb");
	if(fout == NULL) 
	{
	  printf_dbg("output file open failed ret = %d, (%s)\n", ret, strerror(errno));
	  ret = -2;
	  goto part_write_error_in;
	}

	ret = fseek(fin, 0L, SEEK_SET);
	if(ret < 0) 
	{
	  printf_dbg("input file seek failed ret = %d, (%s)\n", ret, strerror(errno));
	  ret= -3;
	  goto part_write_error;
	}

	ret = fseek(fout, 0L, SEEK_SET);
	if(ret < 0) 
	{
	  printf_dbg("output file seek failed\n");
	  ret= -4;
	  goto part_write_error;
	}

	start_t = check_time();
	while(!feof(fin)) 
	{
	  readCnt = fread(buffer, 1, ERASE_BUF_SIZE_MAX, fin);
	  if(ferror(fin))
	  {
	    printf_dbg("input file read error (%s)\n",image_path);
	    ret -5;
	    goto part_write_error;
	  }

	  if(readCnt < 0) 
	  {
	    printf_dbg("input file failed\n");
	    ret = -6;
	    goto part_write_error;
	  }

	  if(readCnt != 0) 
	  {
	    writeCnt = fwrite(buffer, 1, readCnt, fout);
	    if(writeCnt < 0) 
	    {
	      printf_dbg(" output file write failed\n");
	      ret = -7;
	      goto part_write_error;
	    }
	  }
	}
	end_t = check_time() - start_t;
//	printf_dbg("==> write time : %3ld.%03ld,%03ld sec \n",  end_t/1000000, end_t/1000, end_t%1000);
//	printf_dbg("==> end current outputfile pointer = %lu <==\n", (long)ftell(fout));
//	printf_dbg("---------------%s Device Write END-----------------\n", dev_mnt_ptn_path);

	if(NULL != fin)
	    fclose(fin);
	if(NULL != fout)
	    fclose(fout);

	sync();

	return 0;

	part_write_error:
	if(NULL != fout)
	  fclose(fout);
	part_write_error_in:
	if(NULL != fin)
	  fclose(fin);

	return ret;
}

#ifdef PIPE_PRINT
int main(int argc, char *argv[])
#else //PIPE_PRINT
int main()
#endif //PIPE_PRINT
{
	int ret = 0;
	unsigned char* pRamMem =(unsigned char*)calloc(FS_U_RAM_SIZE,1);

	char del[RB_MAX_PATH] = {0, };
	char partition_name[STR_BUF_SIZE_MAX], mount_point[STR_BUF_SIZE_MAX], TempPath[STR_BUF_SIZE_MAX];
    char recovery_ptn_name[STR_BUF_SIZE_MAX], recovery_mount_point[STR_BUF_SIZE_MAX];
	vRM_DeviceData pDeviceDatum;
	RB_UINT32 ComponentInstallerType[1] = {0};

#ifdef CUSTOMER_MULTI_PARTITION
    static CustomerPartitionData pCustomerParttestData[RB_UPDATE_PATITION_NUM];
#else
	CustomerPartitionData pCustomerPartData;
#endif //CUSTOMER_MULTI_PARTITION

#ifdef PIPE_PRINT
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    int fd = atoi(argv[1]);
    g_cmd_pipe = fdopen(fd, "wb");
    setlinebuf(g_cmd_pipe);
#endif // PIPE_PRINT

    printf_dbg("===================================================================\n");
    printf_dbg("============            RedBand Update start 1          ===========\n");
    printf_dbg("===================================================================\n");

	strcpy(TempPath, TEMP_STORAGE_PATH);  //fail safe storage data
	memset(&pDeviceDatum, 0, sizeof(vRM_DeviceData));
	memset(&pCustomerParttestData[0], 0, sizeof(CustomerPartitionData));

    printf_dbg("[%s] data setting start\n", __func__);
	pCustomerParttestData[0].partition_name		= system_pt_name;
	pCustomerParttestData[0].rom_start_address	= 0;
	pCustomerParttestData[0].mount_point        = system_mnt_point;
	pCustomerParttestData[0].partition_type     = PT_FS;
	pCustomerParttestData[0].strSourcePath      = 0;
	pCustomerParttestData[0].strTargetPath      = 0;
	pCustomerParttestData[0].priv               = NULL;

	pCustomerParttestData[1].partition_name     = vr1_pt_name;
	pCustomerParttestData[1].rom_start_address  = 0;
	pCustomerParttestData[1].mount_point        = vr1_mnt_point;
	pCustomerParttestData[1].partition_type     = PT_FS;
	pCustomerParttestData[1].strSourcePath      = 0;
	pCustomerParttestData[1].strTargetPath      = 0;
	pCustomerParttestData[1].priv               = NULL;

	pCustomerParttestData[2].partition_name     = vr2_pt_name;
	pCustomerParttestData[2].rom_start_address  = 0;
	pCustomerParttestData[2].mount_point        = vr2_mnt_point;
	pCustomerParttestData[2].partition_type     = PT_FS;
	pCustomerParttestData[2].strSourcePath      = 0;
	pCustomerParttestData[2].strTargetPath      = 0;
	pCustomerParttestData[2].priv               = NULL;

	pCustomerParttestData[3].partition_name     = fs_image_name;
	pCustomerParttestData[3].rom_start_address  = 0;
	pCustomerParttestData[3].mount_point        = 0;
	pCustomerParttestData[3].partition_type     = PT_FS_IMAGE;
	pCustomerParttestData[3].strSourcePath      = 0;
	pCustomerParttestData[3].strTargetPath      = 0;
	pCustomerParttestData[3].priv               = NULL;

	pCustomerParttestData[4].partition_name     = "etc";
	pCustomerParttestData[4].rom_start_address  = 0;
	pCustomerParttestData[4].mount_point        = TEMP_STORAGE_PATH;
	pCustomerParttestData[4].partition_type     = PT_FS;
	pCustomerParttestData[4].strSourcePath      = 0;
	pCustomerParttestData[4].strTargetPath      = 0;
	pCustomerParttestData[4].priv               = NULL;

	pDeviceDatum.ui32Operation                  = 0;
	pDeviceDatum.pRam                           = pRamMem;
	pDeviceDatum.ui32RamSize                    = FS_U_RAM_SIZE;
	pDeviceDatum.pBufferBlocks                  = 0;
	pDeviceDatum.ui32NumberOfBuffers            = 1024;
#ifndef CUSTOMER_MULTI_PARTITION
	pDeviceDatum.ui32NumberOfPartitions         = 1;
	pDeviceDatum.pFirstPartitionData            = &pCustomerPartData;
#else
	pDeviceDatum.ui32NumberOfPartitions         = RB_UPDATE_PATITION_NUM;
	pDeviceDatum.pFirstPartitionData            = &pCustomerParttestData[0];
#endif //CUSTOMER_MULTI_PARTITION
	pDeviceDatum.pTempPath                      = TempPath;
	pDeviceDatum.pComponentInstallerTypes       = ComponentInstallerType;
	pDeviceDatum.ui32ComponentInstallerTypesNum	= 1;
	pDeviceDatum.enmUpdateType                  = UT_IN_PLACE;
	pDeviceDatum.pDeltaPath                     = del;
	pDeviceDatum.pbUserData                     = NULL;
	pDeviceDatum.ui32ComponentUpdateFlags       = 0xFFFFFFFF;
    printf_dbg("[%s] data setting end\n", __func__);

    /* partition block file open for image delta update */
    ret = ready_for_update();
    if(ret < 0) {
        printf_dbg("[%s] ready_for_partition_work failed.\n", __func__);
        printf_dbg("Failed\n");
        return E_RB_PARTITION_NAME_NOT_FOUND;
    }
    sleep(1);

	ret = RB_OpenFile(NULL, REC_DELTA_FILE_PATH, ONLY_R, &h_delta);
	if(ret != 0) {
		printf_dbg("[%s] open delta file %s failed.\n", __func__, REC_DELTA_FILE_PATH);
        printf_dbg("Failed\n");
		RB_CloseFile(NULL, h_delta);
		return E_RB_OPENFILE_ONLYR;
	}

	ret = RB_vRM_Update(&pDeviceDatum);
    if(0 > ret) {
        printf_dbg("[%s] RB UA update faeild\n", __func__);
        printf_dbg("Failed\n");
    }

    /* Close the open file and umount after update success or failed */
    execute_after_update(ret);

	RB_CloseFile(NULL, h_delta);
	free(pRamMem);
	printf("return value from RB_vRM_Update 0x%08x %d\n", ret, TEST_MAKERESULT(ret));

    printf_dbg("===================================================================\n");
    printf_dbg("===================================================================\n");
    printf_dbg("============                                            ===========\n");
    printf_dbg("============                                            ===========\n");
    printf_dbg("============                 New Image                  ===========\n");
    printf_dbg("============          RedBand Update Complete           ===========\n");
    printf_dbg("============                                            ===========\n");
    printf_dbg("============                                            ===========\n");
    printf_dbg("===================================================================\n");
    printf_dbg("===================================================================\n");

	return ret;
}

/******* START [ Multiprocess API sample implementation ]******/
void *RB_Malloc(unsigned long size)
{
	void *p = malloc(size);

	RB_Trace(0,"RB_Malloc was called.\n");
	
	if (p)
		memset(p, 0, size);

	return p;
}

void RB_Free(void *pMemBlock)
{
	RB_Trace(0,"RB_Free was called.\n");
	
	free(pMemBlock);
}

int RB_GetMaxNumProcess(void *user)
{
	RB_Trace(0,"RB_GetMaxNumProcess was called.\n");
	
	//return SAMPLE_PROCS_NUM_8;
	//return SAMPLE_PROCS_NUM_4;
	//return SAMPLE_PROCS_NUM_2;
	return 0;
}

void* RB_WaitForProcess(const void *handle, unsigned long* process_exit_code)
{
	pid_t pid;
	*process_exit_code = 0;

	RB_Trace(0,"RB_WaitForProcess was called.\n");
	
	// processes
	if (handle) {
#ifdef _NOEXEC_
		RB_Trace(0,"RB_WaitForProcess was called - waitpid\n");
		pid = waitpid((pid_t)handle, (int *)process_exit_code, 0);
#else
		pid = wait((int*)process_exit_code);
#endif //_NOEXEC_
	} else {
		RB_Trace(0, "RB_WaitForProcess was called - wait(else).\n");
		pid = wait((int*)process_exit_code);
	}
	
	if (pid < 0)
		return NULL;
	
	if (!WIFEXITED(*process_exit_code)) {
		*process_exit_code = (char)WTERMSIG(*process_exit_code);
		RB_Trace(0, "Wait Error.\n");
	} else
		*process_exit_code = (char)WEXITSTATUS(*process_exit_code);
	
	return (void*)pid;
}
unsigned long RB_GetMaxProcRamSize(void *user)
{
	RB_Trace(0,"RB_GetMaxProcRamSize was called.\n");

	return 0x12C00000; //300M (300*1024*1024)
         
}
#define EXTRA_ARGS		(3)
#define HANDLE_RUN_PROC		"handle_run_process"
#define IDENT_PLATFORM     "data"
#define DELTA_PLATFORM     "/cache/delta.fs"
#define TEMP_PLATFORM      "/cache"
#define DEVEMMC_PLATFORM  "/"

/* static struct ua_partition_info_t ga_partitions_fs = {IDENT_PLATFORM, 0, DELTA_PLATFORM,	DEVEMMC_PLATFORM, TEMP_PLATFORM}; */
void* RB_RunProcess(void *user, int argc, char* argv[])
{	
	pid_t child_pid;

#ifdef _NOEXEC_
	RB_Trace(0,"RB_RunProcess was called - fork\n");
	child_pid = fork();
#else
	child_pid = vfork();
#endif

	if (child_pid == -1) 
	{      
		RB_Trace(0,"Fork failed.\n");
		return (void *)-1; 
	}
		
	// This is the child 
	if (child_pid == 0)
	{
#ifdef _NOEXEC_ 
		struct ua_data_t ua_data_;
/*		struct ua_partition_info_t *ua_parti_ = &ga_partitions_fs;*/

/*		ua_data_.parti_info = ua_parti_;*/
		ua_data_.ui_progress = NULL;
		ua_data_.ua_op_mode = 0;	

		RB_Trace(0,"RB_HandleProcessRequest. PID = [%d] \n", child_pid);
		RB_HandleProcessRequest(&ua_data_, argc, argv);
		exit(0);
#else
		char **params = NULL;
		int i;

		params = (char **)RB_Malloc((argc+EXTRA_ARGS) *sizeof(char*));
		if (!params)
		{
			return NULL;
		}
		// prepare child data, as it's forbidden to change data after vfork
#if 0 //dennis
		params[0] = strdup(((sample_userdata*)user)->exec_path); 
		params[1] = strdup("handle_run_process");
		params[2] = strdup(((sample_userdata*)user)->delta_path);
#else
		params[0] = strdup("/sbin/rb_ua"); 
		params[1] = strdup("handle_run_process");
		params[2] = strdup("/cache/delta_debug");
#endif

		RB_Trace(0,"argc = [%d]\n", argc); //dennis

		for (i=0; i < argc; i++)
		{
			params[i+EXTRA_ARGS] = strdup(argv[i]);
		}

		for (i=0; i < argc+EXTRA_ARGS; i++) //dennis
		{
		}

		// no need to free allocated memory - execv takes care of it
		execv(params[0], (char**)params);
		//execv(params[0], (char**)argv);
		//execv(argv_test[0], argv_test); //dennis for test
		_exit(-1); // if we're here, execv has failed
#endif
	} 
	else
	{
		RB_Trace(0,"child_pid is not 0\n");
		return (void *)child_pid;
	}
}
