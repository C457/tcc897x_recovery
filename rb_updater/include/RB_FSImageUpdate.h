/*
 *******************************************************************************
 * Copyright (c) 1999-2017 Redbend. All Rights Reserved.
 *******************************************************************************
 */
/*!
 *******************************************************************************
 * \file	RB_FsImageUpdate.h
 *
 * \brief	UPI FsImage Update API
 *******************************************************************************
 */
#ifndef	__RB_FS_IMAGE_UPDATE_H
#define	__RB_FS_IMAGE_UPDATE_H

#include "RB_vRM_Update.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 *******************************************************************************
 * Sync Image Writes.<p>
 *
 * \ref RB_SyncImage.
 *
 * \param	pbUserData			Optional opaque data-structure to pass to IPL
 *								functions
 * \return	S_RB_SUCCESS on success or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */
long RB_SyncImage(
	void *pbUserData);

/**
 *******************************************************************************
 * Sync Backup Writes.<p>
 *
 * \ref RB_SyncBackup.
 *
 * \param	pbUserData			Optional opaque data-structure to pass to IPL
 *								functions
 * \return	S_RB_SUCCESS on success or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */
long RB_SyncBackup(
	void *pbUserData);

/**
 *******************************************************************************
 * Read data from flash.<p>
 *
 * See \ref RB_ReadImageNewKey.
 *
 * \param	pbUserData		Optional opaque data-structure to pass to IPL
 *							functions
 * \param	pbBuffer		(out) Buffer to store data
 * \param	dwStartAddress	64 bit address from which to read
 * \param	dwSize			Size of data to read (must be less than a sector); 0
 *							must immediately return success
 *
 * \return	S_RB_SUCCESS on success or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */
long RB_ReadImage64(
	void *pbUserData,
	unsigned char *pbBuffer,			/* pointer to user buffer */
    RB_UINT64 llStartAddress,		/* memory address to read from */
    RB_UINT32 dwSize);				/* number of bytes to copy */

/**
 *******************************************************************************
 * Get data from temporary storage with 64bit offset
 *
 * \param	pbUserData		Optional opaque data-structure to pass to IPL
 *							functions
 * \param	pbBuffer		(out) Buffer to store the data
 * \param	llAddress		Location of data
 * \param	dwSize			Size of data
 *
 * \return	S_RB_SUCCESS on success or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */
long RB_ReadBackup64(
		void* pbUserData,
		unsigned char* pbBuffer,
		RB_UINT64 llAddress,
		RB_UINT32 dwSize);

/**
 *******************************************************************************
 * Write data to temporary storage with 64bit offset.<p>
 * No need for sync unless RB_SyncBackup is called.
 *
 * \param	pbUserData			Optional opaque data-structure to pass to IPL
 *								functions
 * \param	pbBuffer			Data to write
 * \param	llAddress			Address to which to write
 * \param	dwSize				Size of data
 *
 * \return	S_RB_SUCCESS on success or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */
long RB_WriteBackup64(
		void* pbUserData,
		unsigned char* pbBuffer,
		RB_UINT64 llAddress,
		RB_UINT32 dwSize);
/**
 *******************************************************************************
 * Write sector to flash.<p>
 * No need for sync unless RB_SyncImage is called.
 *
 * \param	pbUserData		Optional opaque data-structure to pass to IPL
 *							functions
 * \param	pbBuffer		Data to write
 * \param	llAddress		Address to which to write
 * \param	dwSize			Size of data
 *
 * \return	S_RB_SUCCESS on success, E_RB_WRITE_ERROR if there is a write error,
 *			or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */
long RB_WriteImage64(
		void* pbUserData,
		unsigned char* pbBuffer,
		RB_UINT64 llAddress,
		RB_UINT32 dwSize);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __RB_FS_IMAGE_UPDATE_H
