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

#include "blewifi_ble_api.h"
#include "blewifi_server_app.h"
#include "blewifi_data.h"
#include "sys_common_api.h"
#include "blewifi_ble_FSM.h"
#include "blewifi_configuration.h"
#include "app_configuration.h"
#include "cmsis_os.h"
#include "app_ctrl.h"
#include "blewifi_handle.h"

/******************************************************
 *                      Macros
 ******************************************************/

/******************************************************
 *                    Constants
 ******************************************************/

/******************************************************
 *                   Enumerations
 ******************************************************/

/******************************************************
 *                 Type Definitions
 ******************************************************/

/******************************************************
 *                    Structures
 ******************************************************/

/******************************************************
 *               Static Function Declarations
 ******************************************************/

/******************************************************
 *               Variable Definitions
 ******************************************************/

/******************************************************
 *               Function Definitions
 ******************************************************/
static uint8_t g_u8BwBleMode = BLEWIFI_BLE_NORMAL_MODE; // BLEWIFI_BLE_NORMAL_MODE , BLEWIFI_BLE_SMART_SLEEP_MODE

osMessageQId g_tBleCMDQ_Id;
osSemaphoreId g_tBleInternalSemaphoreId;

uint8_t BleWifi_Ble_Init(void)
{
    osMessageQDef_t stBleCMDQ = {0};
    osSemaphoreDef_t tSemaphoreDef;
    uint16_t u16BleInterval = 0;

    u16BleInterval = BLEWIFI_BLE_ADVERTISEMENT_INTERVAL_PS_MIN;

    ///[TEMP_CODE] get from sleep module ///
    g_u8BwBleMode = BLEWIFI_BLE_SMART_SLEEP_MODE;
    /////////////////////////////

    /* Create message queue*/
    stBleCMDQ.item_sz = sizeof(BleWifi_BLE_FSM_CmdQ_t);
    stBleCMDQ.queue_sz = BLEWIFI_BLE_CMDQ_SIZE;
    g_tBleCMDQ_Id = osMessageCreate(&stBleCMDQ , NULL);
    if(g_tBleCMDQ_Id == NULL)
    {
        printf("create g_tBleCMDQ_Id fail \r\n");
        return 0;
    }

    // create the semaphore . If need access g_tBleCMDQ_Id , it must to lock.
    tSemaphoreDef.dummy = 0;                            // reserved, it is no used
    g_tBleInternalSemaphoreId = osSemaphoreCreate(&tSemaphoreDef, 1);
    if (g_tBleInternalSemaphoreId == NULL)
    {
        printf("BLE: create the semaphore fail \r\n");
    }

    BleWifi_Ble_FSM_Init();

    osSemaphoreWait(g_tBleInternalSemaphoreId, osWaitForever);

    /* BLE Init Step 1: Do BLE initialization first */
    BleWifi_Ble_PushToFSMCmdQ(BW_BLE_FSM_INITIALIZING , NULL , 0 , NULL);

    if(g_u8BwBleMode == BLEWIFI_BLE_SMART_SLEEP_MODE)
    {
        BleWifi_Ble_PushToFSMCmdQ(BW_BLE_FSM_ADVERTISING_TIME_CHANGE , (uint8_t *)&u16BleInterval , sizeof(u16BleInterval) , NULL);
        BleWifi_Ble_PushToFSMCmdQ(BW_BLE_FSM_ADVERTISING_START , NULL , 0 , BleWifi_Ble_Init_Done_CB);
    }
    else
    {
        BleWifi_Ble_PushToFSMCmdQ(BW_BLE_FSM_ADVERTISING_TIME_CHANGE , (uint8_t *)&u16BleInterval , sizeof(u16BleInterval) , BleWifi_Ble_Init_Done_CB);
    }

    osSemaphoreRelease(g_tBleInternalSemaphoreId);

    return 0;
}
uint8_t BleWifi_Ble_Start(uint16_t u16Interval)
{
    uint16_t u16BleInterval = 0;

    osSemaphoreWait(g_tBleInternalSemaphoreId, osWaitForever);

    if((u16Interval >= 0x20) && (u16Interval <= 0x4000))
    {
        u16BleInterval = (u16Interval / 0.625);
    }
    else
    {
        printf("u16Interval = %u is invalid , use default 100ms\r\n" , u16Interval);
        u16BleInterval = 0x640;
    }

    if(g_u8BwBleMode == BLEWIFI_BLE_NORMAL_MODE)
    {
        BleWifi_Ble_PushToFSMCmdQ(BW_BLE_FSM_ADVERTISING_TIME_CHANGE , (uint8_t *)&u16BleInterval , sizeof(u16BleInterval) , NULL);
        BleWifi_Ble_PushToFSMCmdQ(BW_BLE_FSM_ADVERTISING_START , NULL , 0 , BleWifi_Ble_Start_Cfm_CB);
    }
    else
    {
        BleWifi_Ble_PushToFSMCmdQ(BW_BLE_FSM_ADVERTISING_TIME_CHANGE , (uint8_t *)&u16BleInterval , sizeof(u16BleInterval) , BleWifi_Ble_Start_Cfm_CB);
    }

    osSemaphoreRelease(g_tBleInternalSemaphoreId);

    return 0;
}
uint8_t BleWifi_Ble_Stop(void)
{
    uint16_t u16BleInterval = 0;

    osSemaphoreWait(g_tBleInternalSemaphoreId, osWaitForever);

    BwBleFlushCMD(g_tBleCMDQ_Id);

    if(g_u8BwBleMode == BLEWIFI_BLE_NORMAL_MODE)
    {
        BleWifi_Ble_PushToFSMCmdQ(BW_BLE_FSM_ADVERTISING_EXIT , NULL , 0 , NULL);

    }
    else if(g_u8BwBleMode == BLEWIFI_BLE_SMART_SLEEP_MODE)
    {
        u16BleInterval = BLEWIFI_BLE_ADVERTISEMENT_INTERVAL_PS_MIN ;
        BleWifi_Ble_PushToFSMCmdQ(BW_BLE_FSM_ADVERTISING_TIME_CHANGE , (uint8_t *)&u16BleInterval , sizeof(u16BleInterval) , NULL);
    }
    BleWifi_Ble_PushToFSMCmdQ(BW_BLE_FSM_DISCONNECT , NULL , 0 , NULL);

    BleWifi_Ble_PushToFSMCmdQ(BW_BLE_FSM_BLE_STOP , NULL , 0 , BleWifi_Ble_Stop_Cfm_CB);

    osSemaphoreRelease(g_tBleInternalSemaphoreId);

    return 0;
}

uint8_t	BleWifi_Ble_AdvertisingIntervalChange(uint16_t u16Interval)
{
    uint16_t u16BleInterval = 0;

    osSemaphoreWait(g_tBleInternalSemaphoreId, osWaitForever);

    if((u16Interval >= 0x20) && (u16Interval <= 0x4000))
    {
        u16BleInterval = (u16Interval / 0.625);
    }
	else if(u16Interval == 0xFFFF)
    {
        u16BleInterval = 0xFFFF;
    }
    else
    {
        printf("u16Interval = %u is invalid , use default 100ms\r\n" , u16Interval);
        u16BleInterval = 0x640;
    }

    BleWifi_Ble_PushToFSMCmdQ(BW_BLE_FSM_ADVERTISING_TIME_CHANGE , (uint8_t *)&u16BleInterval , sizeof(u16BleInterval) , BleWifi_Ble_Start_Cfm_CB);

    osSemaphoreRelease(g_tBleInternalSemaphoreId);

    return 0;
}

uint8_t	BleWifi_Ble_AdvertisingDataChange(uint8_t *pu8Data, uint32_t u32DataLen)
{
    osSemaphoreWait(g_tBleInternalSemaphoreId, osWaitForever);

    BleWifi_Ble_PushToFSMCmdQ(BW_BLE_FSM_CHANGE_ADVERTISING_DATA , pu8Data , u32DataLen , NULL);

    osSemaphoreRelease(g_tBleInternalSemaphoreId);

    return 0;
}

uint8_t BleWifi_Ble_ScanDataChange(uint8_t *pu8Data, uint32_t u32DataLen)
{
    osSemaphoreWait(g_tBleInternalSemaphoreId, osWaitForever);

    BleWifi_Ble_PushToFSMCmdQ(BW_BLE_FSM_CHANGE_SCAN_DATA , pu8Data , u32DataLen , NULL);

    osSemaphoreRelease(g_tBleInternalSemaphoreId);

    return 0;
}

uint8_t BleWifi_Ble_MacAddrWrite(uint8_t *data)
{
    uint8_t ubaMacAddr[6];

    ubaMacAddr[5] = data[0];
    ubaMacAddr[4] = data[1];
    ubaMacAddr[3] = data[2];
    ubaMacAddr[2] = data[3];
    ubaMacAddr[1] = data[4];
    ubaMacAddr[0] = data[5];

    ble_set_config_bd_addr(ubaMacAddr);

    return 0;
}

uint8_t BleWifi_Ble_MacAddrRead(uint8_t *data)
{
    uint8_t ubaMacAddr[6];

    ble_get_config_bd_addr(ubaMacAddr);

    data[5] = ubaMacAddr[0];
    data[4] = ubaMacAddr[1];
    data[3] = ubaMacAddr[2];
    data[2] = ubaMacAddr[3];
    data[1] = ubaMacAddr[4];
    data[0] = ubaMacAddr[5];

    return 0;
}
