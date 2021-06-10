/**
 *******************************************************************************
 * \mainpage
 * <p> 
 * Copyright (c) 1999-2017 Redbend. All Rights Reserved.
 *******************************************************************************
 */

/*!
 *******************************************************************************
 * \file	RB_MultiProcessUpdate.h
 *
 * \brief	UPI MultiProcess API
 *
 * Multi-process updates can work on platforms that support multi-processing.
 * To use the UPI with multi-process support you should implement the following
 * Porting Layer functions:
 *		RB_Malloc
 * 		RB_Free
 * 		RB_WaitForProcess
 * 		RB_RunProcess
 * 		RB_GetMaxNumProcess
 * 		RB_GetMaxProcRamSize
 *
 * The function RB_RunProcess must run a sub-process (optionally the same
 * executable running the main process) that is also integrated with the UPI.
 * This process must call the UPI API function RB_HandleProcessRequest.
 * <p>
 * The initial call to RB_vRM_Update does not change. The first time vRapid 
 * Mobile calls RB_GetMaxNumProcess to see if multi-processing is supported 
 * and see how many processes are supported. If a positive number is returned,
 * the UPI will run parts of the update in sub-processes.
 * In addition, right before every RB_RunProcess is called, vRapidMobile will call 
 * RB_GetMaxNumProcess to update the allowed number of processes. If the new 
 * value is less than the number of running processes, the new 
 * process will not start unless the number of running processes will decrease 
 * (gracefully - the processes will not be killed). 
 * In such scenario, if this happens and the number of running 
 * processes allows running another process,  RB_GetMaxNumProcess will
 * be called again before the process is started (an so on). This allows dynamic
 * process control, which prevents starting a new process while making sure that
 * a new allowed limit did not decrease.
 * <p>
 * The UPI creates a sub-process by calling RB_RunProcess with the arguments
 * for the request. The new process must call RB_HandleProcessRequest to
 * handle the request. The UPI manages processes using RB_WaitForProcess. 
 * Each sub-process allocates and frees memories using RB_Malloc and RB_Free.
 * <p>
 * Notes:
 * \li	In the function RB_RunProcess it is recommended to flush output streams
 *		(like stdout or log files) before creating the child process to prevent
 *		buffer duplication in the child process.
 * \li	RB_HandleProcessRequests starts a UPI operation that requires all
 *		standard and common IPL functions in RB_vRM_FileSystemUpdate.h and
 *		RB_vRM_Update.h
 * \li	If your implementation of these functions requires any preparations,
 *		such as opening the update file for RB_GetDelta or opening a log file
 *		for RB_Trace, these preparations must be done before calling
 *		RB_HandleProcessRequests.
 * \li  To distinguish between processes when running in Multi-Process Update mode,
 * 		it is recommended that you add the Process ID (i.e. getpid()) to the log
 * 		output in RB_Trace. Alternatively, create a log file for each sub-process
 * 		named log_file.[pid]
 * \li	If your implementation uses the optional opaque data structure ('user'
 *		in the functions below), you may also have to pass it to sub-processes.
 *		You must implement this functionality.
 *******************************************************************************
 */
#ifndef _REDBEND_MULTIPROCESS_UPDATE_H_
#define _REDBEND_MULTIPROCESS_UPDATE_H_

#include "RB_vRM_Update.h"
#include "RB_vRM_Errors.h"

#ifdef __cplusplus
extern "C" {
#endif	/* __cplusplus */

/**
 *******************************************************************************
 * Allocate a memory block. 
 *
 * \param	size	Memory block size, in blocks
 * 
 * \return	A pointer to the memory block on success, NULL on failure
 *******************************************************************************
 */
void *RB_Malloc(RB_UINT32 size);

/**
 *******************************************************************************
 * Free a memory block.
 *
 * \param	pMemBlock 	Pointer to the memory block
 *******************************************************************************
 */
void RB_Free(void *pMemBlock);

/**
 *******************************************************************************
 * Wait for a process to complete.
 * 
 * \param	handle				Process handle. If NULL, wait for any process to
 *								complete. 
 * \param	process_exit_code	Exit code of the completed process
 *
 * \return	Handle of the completed process or NULL on error
 *******************************************************************************
 */
void* RB_WaitForProcess(const void *handle, RB_UINT32* process_exit_code);

/**
 *******************************************************************************
 * Create a new process.
 *
 * The new process must call \ref RB_HandleProcessRequest with the same
 * arguments.
 * 
 * \param	user	Optional opaque data-structure passed in during
 *					initialization
 * \param	argc	Number of \a argv arguments
 * \param	argv	Arguments to pass to the new process
 *
 * \return	Handle to the new process or NULL on error
 *******************************************************************************
 */
void* RB_RunProcess(void *user, int argc, char*argv[]);

/**
 *******************************************************************************
 * Get the maximum number of processes that are allowed to run.
 *
 * \param	user	Optional opaque data-structure passed in during
 *					initialization
 *
 * \return	The number of allowed processes or 0 if multiple processes are not
 *			allowed
 *******************************************************************************
 */
int RB_GetMaxNumProcess(void *user);
/**
 *******************************************************************************
 * Get the maximum available memory for processes to use.
 *
 * \param	user	Optional opaque data-structure passed in during
 *					initialization
 *
 * \return	The memory amount available for processes or 0 if there is no available memory
 *******************************************************************************
 */
RB_UINT32 RB_GetMaxProcRamSize(void *user);

/**
 *******************************************************************************
 * Initialize a new process and handle the request sent from an external
 * process.
 *
 * Called from the main function of the new process with the parameters passed to
 * \ref RB_RunProcess.
 * 
 * \param	user	Optional opaque data-structure passed in during
 *					initialization
 * \param	argc	Number of \a argv arguments
 * \param	argv	Arguments to pass to the new process
 *
 * \return	S_RB_SUCCESS on success or an error code
 *******************************************************************************
 */
long RB_HandleProcessRequest(void *user, int argc, char *argv[]);


#ifdef __cplusplus
	}
#endif	/* __cplusplus */

#endif
