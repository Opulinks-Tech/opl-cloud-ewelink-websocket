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

#include <stdlib.h>
#include <string.h>
#include "iot_rb_data.h"
#include "cmsis_os.h"

void IoT_Ring_Buffer_Init(IoT_Ring_buffer_t *pstRbData, uint8_t u8MaxQueueSize)
{
    osSemaphoreDef_t tSemaphoreDef;

    // create the semaphore
    tSemaphoreDef.dummy = 0;                            // reserved, it is no used
    pstRbData->tRBSemaphoreId = osSemaphoreCreate(&tSemaphoreDef, 1);
    if (pstRbData->tRBSemaphoreId == NULL)
    {
        printf("To create the semaphore for Iot RB is fail.\n");
    }

    osSemaphoreWait(pstRbData->tRBSemaphoreId, osWaitForever);
    pstRbData->u16QueueCount = 0;
    pstRbData->ulReadIdx = 0;
    pstRbData->ulWriteIdx = 0;
    pstRbData->u8MaxQueueSize = u8MaxQueueSize;
    pstRbData->taProperity = (IoT_Properity_t *)malloc(sizeof(IoT_Properity_t)*u8MaxQueueSize);
    osSemaphoreRelease(pstRbData->tRBSemaphoreId);
}

uint8_t IoT_Ring_Buffer_Push(IoT_Ring_buffer_t *pstRbData, IoT_Properity_t *ptProperity)
{
    uint8_t bRet = IOT_RB_DATA_FAIL;
    uint32_t ulWriteNext;

    osSemaphoreWait(pstRbData->tRBSemaphoreId, osWaitForever);

    if (ptProperity == NULL)
    {
        goto done;
    }

    // full, ulWriteIdx + 1 == ulReadIdx
    ulWriteNext = (pstRbData->ulWriteIdx + 1) % pstRbData->u8MaxQueueSize;

    // Read index always prior to write index
    if (ulWriteNext == pstRbData->ulReadIdx)
    {
#if 1
        // discard the oldest data, and read index move forware one step.
        free(pstRbData->taProperity[pstRbData->ulReadIdx].ubData);
        pstRbData->ulReadIdx = (pstRbData->ulReadIdx + 1) % pstRbData->u8MaxQueueSize;
#else
        goto done;
#endif
    }

    // update the temperature data to write index
	memcpy(&(pstRbData->taProperity[pstRbData->ulWriteIdx]), ptProperity, sizeof(IoT_Properity_t));
    pstRbData->ulWriteIdx = ulWriteNext;
    pstRbData->u16QueueCount++;

    bRet = IOT_RB_DATA_OK;

done:
    osSemaphoreRelease(pstRbData->tRBSemaphoreId);
    return bRet;
}

uint8_t IoT_Ring_Buffer_Pop(IoT_Ring_buffer_t *pstRbData, IoT_Properity_t *ptProperity)
{
    uint8_t bRet = IOT_RB_DATA_FAIL;

    osSemaphoreWait(pstRbData->tRBSemaphoreId, osWaitForever);

    if (ptProperity == NULL)
    {
        goto done;
    }

    // empty, ulWriteIdx == ulReadIdx
    if (pstRbData->ulWriteIdx == pstRbData->ulReadIdx)
    {
        goto done;
    }

	memcpy(ptProperity, &(pstRbData->taProperity[pstRbData->ulReadIdx]), sizeof(IoT_Properity_t));

    bRet = IOT_RB_DATA_OK;

done:
    osSemaphoreRelease(pstRbData->tRBSemaphoreId);
    return bRet;
}

uint8_t IoT_Ring_Buffer_CheckEmpty(IoT_Ring_buffer_t *pstRbData)
{
    uint8_t bRet = IOT_RB_DATA_FAIL;

    osSemaphoreWait(pstRbData->tRBSemaphoreId, osWaitForever);

    // empty, ulWriteIdx == ulReadIdx
    if (pstRbData->ulWriteIdx == pstRbData->ulReadIdx)
    {
        bRet = IOT_RB_DATA_OK;
    }

//done:
    osSemaphoreRelease(pstRbData->tRBSemaphoreId);
    return bRet;
}

void IoT_Ring_Buffer_ResetBuffer(IoT_Ring_buffer_t *pstRbData)
{
    IoT_Properity_t ptProperity;

    while (IOT_RB_DATA_OK != IoT_Ring_Buffer_CheckEmpty(pstRbData))
    {
        IoT_Ring_Buffer_Pop(pstRbData , &ptProperity);
        IoT_Ring_Buffer_ReadIdxUpdate(pstRbData);

        if(ptProperity.ubData!=NULL)
            free(ptProperity.ubData);
    }
    pstRbData->u16QueueCount = 0;
}

uint8_t IoT_Ring_Buffer_ReadIdxUpdate(IoT_Ring_buffer_t *pstRbData)
{
    osSemaphoreWait(pstRbData->tRBSemaphoreId, osWaitForever);
    pstRbData->ulReadIdx = (pstRbData->ulReadIdx + 1) % pstRbData->u8MaxQueueSize;
    pstRbData->u16QueueCount --;
    osSemaphoreRelease(pstRbData->tRBSemaphoreId);
    return IOT_RB_DATA_OK;
}

uint8_t IoT_Ring_Buffer_CheckFull(IoT_Ring_buffer_t *pstRbData)
{
    uint8_t bRet = IOT_RB_DATA_FAIL;
    uint32_t ulWriteNext;

    osSemaphoreWait(pstRbData->tRBSemaphoreId, osWaitForever);

    // full, ulWriteIdx + 1 == ulReadIdx
    ulWriteNext = (pstRbData->ulWriteIdx + 1) % pstRbData->u8MaxQueueSize;

    // Read index always prior to write index
    if (ulWriteNext == pstRbData->ulReadIdx)
    {
        bRet = IOT_RB_DATA_OK;
    }

//done:
    osSemaphoreRelease(pstRbData->tRBSemaphoreId);
    return bRet;
}

uint8_t IoT_Ring_Buffer_GetQueueCount(IoT_Ring_buffer_t *pstRbData ,uint16_t *u16QueueCount)
{
    osSemaphoreWait(pstRbData->tRBSemaphoreId, osWaitForever);
    *u16QueueCount = pstRbData->u16QueueCount;
    osSemaphoreRelease(pstRbData->tRBSemaphoreId);
    return IOT_RB_DATA_OK;
}

