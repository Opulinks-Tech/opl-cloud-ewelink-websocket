/******************************************************************************
*  Copyright 2017 - 2018, Opulinks Technology Ltd.
*  ----------------------------------------------------------------------------
*  Statement:
*  ----------
*  This software is protected by Copyright and the information contained
*  herein is confidential. The software may not be copied and the information
*  contained herein may not be used or disclosed except with the written
*  permission of Opulinks Technology Ltd. (C) 2018
******************************************************************************/

#ifndef __IOT_RB_DATA_H__
#define __IOT_RB_DATA_H__

#include <stdint.h>
#include <stdbool.h>
#include "cmsis_os.h"

#define IOT_RB_DATA_OK      0
#define IOT_RB_DATA_FAIL    1

#define IOT_RB_COUNT                  (32)
#define IOT_RB_KEEP_ALIVE_COUNT       (2)
#define IOT_RB_CLOUD_RSP_COUNT        (10)

typedef struct{
    uint8_t *ubData;
    uint32_t ulLen;
} IoT_Properity_t;

typedef struct
{
    uint32_t ulReadIdx;
    uint32_t ulWriteIdx;
    IoT_Properity_t *taProperity;
    osSemaphoreId tRBSemaphoreId;
    uint32_t u8MaxQueueSize;
    uint16_t u16QueueCount;
} IoT_Ring_buffer_t;

void IoT_Ring_Buffer_Init(IoT_Ring_buffer_t *pstRbData, uint8_t u8MaxQueueSize);
uint8_t IoT_Ring_Buffer_Push(IoT_Ring_buffer_t *pstRbData, IoT_Properity_t *ptProperity);
uint8_t IoT_Ring_Buffer_Pop(IoT_Ring_buffer_t *pstRbData, IoT_Properity_t *ptProperity);
uint8_t IoT_Ring_Buffer_CheckEmpty(IoT_Ring_buffer_t *pstRbData);
void IoT_Ring_Buffer_ResetBuffer(IoT_Ring_buffer_t *pstRbData);
uint8_t IoT_Ring_Buffer_ReadIdxUpdate(IoT_Ring_buffer_t *pstRbData);
uint8_t IoT_Ring_Buffer_CheckFull(IoT_Ring_buffer_t *pstRbData);
uint8_t IoT_Ring_Buffer_GetQueueCount(IoT_Ring_buffer_t *pstRbData ,uint16_t *u16QueueCount);

#endif // __IOT_RB_DATA_H__

