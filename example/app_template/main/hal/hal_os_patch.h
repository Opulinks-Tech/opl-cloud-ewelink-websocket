/* *****************************************************************************
 *  Copyright 2019, Opulinks Technology Ltd.
 *  ---------------------------------------------------------------------------
 *  Statement:
 *  ----------
 *  This software is protected by Copyright and the information contained
 *  herein is confidential. The software may not be copied and the information
 *  contained herein may not be used or disclosed except with the written
 *  permission of Opulinks Technology Ltd. (C) 2018
 *
 *******************************************************************************
 *
 *  @file hal_os_patch.h
 *
 *  @brief
 *
 ******************************************************************************/

#ifndef _CMSIS_OS_PATCH_H_
#define _CMSIS_OS_PATCH_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
 *************************************************************************
 *                          Include files
 *************************************************************************
 */
#include "cmsis_os.h"
/*
 *************************************************************************
 *                          Definitions and Macros
 *************************************************************************
 */


/*
 *************************************************************************
 *                          Typedefs and Structures
 *************************************************************************
 */

typedef osStatus (*T_osMessagePutFrontFp) (osMessageQId queue_id, uint32_t info, uint32_t millisec);
extern T_osMessagePutFrontFp osMessagePutFront;

typedef osEvent (*T_osMessagePeekFp) (osMessageQId queue_id, uint32_t millisec);
extern T_osMessagePeekFp osMessagePeek;


/*
 *************************************************************************
 *                          Public Variables
 *************************************************************************
 */


/*
 *************************************************************************
 *                          Public Functions
 *************************************************************************
 */

void osPatchInit(void);


#ifdef __cplusplus
}
#endif
#endif  /* _CMSIS_OS_PATCH_H_ */
