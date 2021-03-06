/*
 *******************************************************************************
 * Copyright (c) 1999-2017 Redbend. All Rights Reserved.
 *******************************************************************************
 */
/*!
 *******************************************************************************
 * \file	RB_ImageUpdate.h
 *
 * \brief	UPI Image Update API
 *******************************************************************************
 */
#ifndef	__RB_IMAGEUPDATE_H__
#define	__RB_IMAGEUPDATE_H__

#include "RB_vRM_Update.h"
#include "RB_vRM_Errors.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 *******************************************************************************
 * Get data from temporary storage.
 *
 * \param	pbUserData		Optional opaque data-structure to pass to IPL
 *							functions
 * \param	pbBuffer		(out) Buffer to store the data
 * \param	dwBlockAddress	Location of data
 * \param	dwSize			Size of data
 *
 * \return	S_RB_SUCCESS on success or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */
long RB_ReadBackupBlock(
	void *pbUserData,
	unsigned char *pbBuffer,
	RB_UINT32 dwBlockAddress,
	RB_UINT32 dwSize);

/**
 *******************************************************************************
 * Erase sector from temporary storage.<p>
 *
 * A default implementation is included. You must use the default implementation
 * if both of the following are true:
 * \li	<b>InstantFailSafe</b> is set to false
 * \li	Only Image updates are used OR the backup is stored in flash and not on
 *		the file system
 * <p>The address must be aligned to the sector size.
 *
 * \param	pbUserData		Optional opaque data-structure to pass to IPL
 *							functions
 * \param	dwStartAddress	Sector address to erase
 *
 * \return	S_RB_SUCCESS on success or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */
long RB_EraseBackupBlock(
	void *pbUserData,
	RB_UINT32 dwStartAddress);

/**
 *******************************************************************************
 * Write complete sector to temporary storage.<p>
 *
 * Must overwrite entire sector. To write part of a sector, see
 * \ref RB_WriteBackupPartOfBlock.
 *
 * \param	pbUserData			Optional opaque data-structure to pass to IPL
 *								functions
 * \param	dwBlockStartAddress	Address to which to write
 * \param	pbBuffer			Data to write
 *
 * \return	S_RB_SUCCESS on success or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */
long RB_WriteBackupBlock(
	void *pbUserData,
	RB_UINT32 dwBlockStartAddress,
	unsigned char *pbBuffer);

/**
 *******************************************************************************
 * Write data to temporary storage.<p>
 *
 * Used to write part of a sector that has already been written to by
 * \ref RB_WriteBackupBlock. Must NOT overwrite entire sector.<p>
 *
 * A default implementation is included. You must use the default implementation
 * if both of the following are true:
 * \li	<b>InstantFailSafe</b> is set to false
 * \li	Only Image updates are used OR the backup is stored in flash and not on
 *		the file system
 *
 * \param	pbUserData		Optional opaque data-structure to pass to IPL
 *							functions
 * \param	dwStartAddress	Address to which to write
 * \param	dwSize			Size of \a pbBuffer
 * \param	pbBuffer		Data to write
 *
 * \return	S_RB_SUCCESS on success or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */
long RB_WriteBackupPartOfBlock(
	void *pbUserData,
	RB_UINT32 dwStartAddress,
	RB_UINT32 dwSize,
	unsigned char* pbBuffer);

/**
 *******************************************************************************
 * Write sector to flash.<p>
 *
 * Must overwrite a complete sector.
 *
 * \param	pbUserData		Optional opaque data-structure to pass to IPL
 *							functions
 * \param	dwBlockAddress	Address to which to write
 * \param	pbBuffer		Data to write
 *
 * \return	S_RB_SUCCESS on success, E_RB_WRITE_ERROR if there is a write error,
 *			or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */
long RB_WriteBlock(
	void *pbUserData,
	RB_UINT32 dwBlockAddress,
	unsigned char *pbBuffer);

/**
 *******************************************************************************
 * Read data from flash.<p>
 *
 * See \ref RB_ReadImageNewKey.
 *
 * \param	pbUserData		Optional opaque data-structure to pass to IPL
 *							functions
 * \param	pbBuffer		(out) Buffer to store data
 * \param	dwStartAddress	Address from which to read
 * \param	dwSize			Size of data to read (must be less than a sector); 0
 *							must immediately return success
 *
 * \return	S_RB_SUCCESS on success or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */
long RB_ReadImage(
	void *pbUserData,
	unsigned char *pbBuffer,			/* pointer to user buffer */
    RB_UINT32 dwStartAddress,		/* memory address to read from */
    RB_UINT32 dwSize);				/* number of bytes to copy */

/**
 *******************************************************************************
 * Read data from flash.<p>
 *
 * As opposed to \ref RB_ReadImage, this function is intended to read already
 * updated data and is used for encrypted images. If no part of the image is
 * encrypted, simply call \ref RB_ReadImage.
 *
 * Used by the UPI when resuming an interrupted update.
 *
 * \param	pbUserData		Optional opaque data-structure to pass to IPL
 *							functions
 * \param	pbBuffer		(out) Buffer to store data
 * \param	dwStartAddress	Address from which to read
 * \param	dwSize			Size of data to read (must be less than a sector); 0
 *							must immediately return success
 *
 * \return	S_RB_SUCCESS on success or an \ref RB_vRM_Errors.h error code
 *******************************************************************************
 */
long RB_ReadImageNewKey(
	void *pbUserData,
	unsigned char *pbBuffer,
	RB_UINT32 dwStartAddress,
	RB_UINT32 dwSize);

#if defined(M_LOADER_GEN) || defined(RB_PCUA)
// for PT_FS_IMAGE simreset
void RB_BeginFileUpdate(int id);
#endif

/**
 *******************************************************************************
 * Get sector size.<p>
 *
 * \param	pbUserData		Optional opaque data-structure to pass to IPL
 *							functions
 *
 * \return	Sector size, in bytes
 *******************************************************************************
 */
long RB_GetBlockSize(void *pbUserData);

#ifdef __cplusplus
	}
#endif  /* __cplusplus */

#endif	/*	__RB_UPDATE_IMAGE_H__	*/
