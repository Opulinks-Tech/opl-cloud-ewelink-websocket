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
#include "cmsis_os.h"
#include "sys_os_config.h"

#include "iot_data.h"
#include "blewifi_common.h"
#include "blewifi_configuration.h"
#include "iot_configuration.h"
#include "app_ctrl.h"
#include "iot_rb_data.h"
#include "iot_handle.h"
#include "blewifi_wifi_api.h"
#include "etharp.h"
#include "coolkit_websocket.h"
#include "cloud_http.h"
#include "hal_system.h"

osTimerId g_tmr_req_date = NULL; //For Keep alive
osTimerId g_iot_tx_wait_timeout_timer = NULL;
osTimerId g_iot_tx_ack_post_timeout_timer = NULL;
osTimerId g_iot_tx_sw_reset_timeout_timer = NULL;  // TX
osTimerId g_iot_tx_cloud_connect_retry_timer = NULL;  // cloud reconnect retry interval timer

#define CLOUD_CONNECT_RETRY_INTERVAL_TBL_NUM (6)
uint32_t g_u32aCloudRetryIntervalTbl[CLOUD_CONNECT_RETRY_INTERVAL_TBL_NUM] =
{
    0,
    30000,
    30000,
    60000,
    60000,
    600000,
};
uint8_t g_u8CloudRetryIntervalIdx = 0;

#if (IOT_DEVICE_DATA_TX_EN == 1)
osThreadId g_tIotDataTxTaskId;
osMessageQId g_tIotDataTxQueueId;

// Queue priority g_stCloudRspQ > g_stKeepAliveQ > g_stIotRbData
IoT_Ring_buffer_t g_stCloudRspQ = {0};
IoT_Ring_buffer_t g_stKeepAliveQ = {0};
IoT_Ring_buffer_t g_stIotRbData = {0};
uint8_t g_u8WaitingRspType = IOT_DATA_WAITING_TYPE_NONE;
uint8_t g_u8PostRetry_KeepAlive_Cnt = 0;
uint8_t g_u8PostRetry_KeepAlive_Fail_Round = 0;
uint8_t g_u8PostRetry_IotRbData_Cnt = 0;
#endif

#if (IOT_DEVICE_DATA_RX_EN == 1)
osThreadId g_tIotDataRxTaskId;
#endif

extern EventGroupHandle_t g_tAppCtrlEventGroup;

extern httpclient_t g_client;

#if (IOT_DEVICE_DATA_TX_EN == 1)
static void Iot_Data_TxTaskEvtHandler_CloudInit(uint32_t evt_type, void *data, int len);
static void Iot_Data_TxTaskEvtHandler_CloudBinding(uint32_t evt_type, void *data, int len);
static void Iot_Data_TxTaskEvtHandler_CloudConnection(uint32_t evt_type, void *data, int len);
static void Iot_Data_TxTaskEvtHandler_CloudKeepAlive(uint32_t evt_type, void *data, int len);
static void Iot_Data_TxTaskEvtHandler_CloudDisconnection(uint32_t evt_type, void *data, int len);
static void Iot_Data_TxTaskEvtHandler_CloudWaitRxRspTimeout(uint32_t evt_type, void *data, int len);
static void Iot_Data_TxTaskEvtHandler_CloudPost(uint32_t evt_type, void *data, int len);
static void Iot_Data_TxTaskEvtHandler_AckPostTimeout(uint32_t evt_type, void *data, int len);

static T_IoT_Data_EvtHandlerTbl g_tIotDataTxTaskEvtHandlerTbl[] =
{

    {IOT_DATA_TX_MSG_CLOUD_INIT,                Iot_Data_TxTaskEvtHandler_CloudInit},
    {IOT_DATA_TX_MSG_CLOUD_BINDING,             Iot_Data_TxTaskEvtHandler_CloudBinding},
    {IOT_DATA_TX_MSG_CLOUD_CONNECTION,          Iot_Data_TxTaskEvtHandler_CloudConnection},
    {IOT_DATA_TX_MSG_CLOUD_KEEP_ALIVE,          Iot_Data_TxTaskEvtHandler_CloudKeepAlive},
    {IOT_DATA_TX_MSG_CLOUD_DISCONNECTION,       Iot_Data_TxTaskEvtHandler_CloudDisconnection},
    {IOT_DATA_TX_MSG_CLOUD_WAIT_RX_RSP_TIMEOUT, Iot_Data_TxTaskEvtHandler_CloudWaitRxRspTimeout},
    {IOT_DATA_TX_MSG_CLOUD_POST,                Iot_Data_TxTaskEvtHandler_CloudPost},
    {IOT_DATA_TX_MSG_CLOUD_POST_ACK_TIMEOUT,    Iot_Data_TxTaskEvtHandler_AckPostTimeout},

    {0xFFFFFFFF,                            NULL}
};

void Iot_Data_TxTaskEvtHandler_CloudConnectRetry(void)
{
    if(0 == g_u32aCloudRetryIntervalTbl[g_u8CloudRetryIntervalIdx])
    {
        Iot_Data_TxTask_MsgSend(IOT_DATA_TX_MSG_CLOUD_CONNECTION, NULL, 0);
    }
    else
    {
        osTimerStop(g_iot_tx_cloud_connect_retry_timer);
        osTimerStart(g_iot_tx_cloud_connect_retry_timer, g_u32aCloudRetryIntervalTbl[g_u8CloudRetryIntervalIdx]);
    }
    printf("cloud retry wait %u\r\n",g_u32aCloudRetryIntervalTbl[g_u8CloudRetryIntervalIdx]);
    if(g_u8CloudRetryIntervalIdx < CLOUD_CONNECT_RETRY_INTERVAL_TBL_NUM - 1)
    {
        g_u8CloudRetryIntervalIdx++;
    }
}

static void Iot_Data_TxTaskEvtHandler_CloudInit(uint32_t evt_type, void *data, int len)
{
    //handle Cloud Init
    printf("Cloud Initialization\n");

}

static void Iot_Data_TxTaskEvtHandler_CloudBinding(uint32_t evt_type, void *data, int len)
{
    //handle Cloud Binding
    printf("Cloud Binding\n");
}

static void Iot_Data_TxTaskEvtHandler_CloudConnection(uint32_t evt_type, void *data, int len)
{
    //handle Cloud Connection
    printf("Cloud Connection\n");
    int ret = 0;
    uint8_t u8Discard = false;

    if (true == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_WIFI_GOT_IP))
    {
        if (false == BleWifi_COM_EventStatusGet(g_tIotDataEventGroup , IOT_DATA_EVENT_BIT_CLOUD_CONNECTED))
        {
            ret = Connect_coolkit_http();

            if( 0 == ret)
            {
                ret = Connect_coolkit_wss();
                if(ret == COOLKIT_REG_SUCCESS)
                {
                    //reg success
                }
                else if(ret == COOLKIT_NOT_REG)
                {
                    //device No Register, return to idle
                }
                else
                {
                    goto fail;
                }
            }
            else
            {
                goto fail;
            }
        }
    }
    //return to idle
    return;

fail:
    if (true == BleWifi_COM_EventStatusGet(g_tIotDataEventGroup , IOT_DATA_EVENT_BIT_POST_FAIL_RECONNECT))
    {
        uint16_t u16QueueCount = 0;
        IoT_Properity_t ptProperity = {0};

        IoT_Ring_Buffer_GetQueueCount(&g_stIotRbData , &u16QueueCount);

        osSemaphoreWait(g_tAppSemaphoreId, osWaitForever);
        g_u8PostRetry_IotRbData_Cnt ++;
        osSemaphoreRelease(g_tAppSemaphoreId);

        if(IOT_DATA_QUEUE_LAST_DATA_CNT == u16QueueCount) // last data
        {
            if(g_u8PostRetry_IotRbData_Cnt >= IOT_DATA_LAST_DATA_POST_RETRY_MAX)
            {
                u8Discard = true;
                printf("post cnt = %u discard\r\n" , g_u8PostRetry_IotRbData_Cnt);
                if (IOT_RB_DATA_OK != IoT_Ring_Buffer_CheckEmpty(&g_stIotRbData))
                {
                    IoT_Ring_Buffer_Pop(&g_stIotRbData , &ptProperity);
                    IoT_Ring_Buffer_ReadIdxUpdate(&g_stIotRbData);

                    if(ptProperity.ubData!=NULL)
                        free(ptProperity.ubData);
                }
                osSemaphoreWait(g_tAppSemaphoreId, osWaitForever);
                g_u8PostRetry_IotRbData_Cnt = 0;
                osSemaphoreRelease(g_tAppSemaphoreId);

                BleWifi_COM_EventStatusSet(g_tIotDataEventGroup, IOT_DATA_EVENT_BIT_POST_FAIL_RECONNECT, false);
                Iot_Data_TxTaskEvtHandler_CloudConnectRetry(); // do normal connection retry
            }
            else
            {
                osDelay(100);
                Iot_Data_TxTask_MsgSend(IOT_DATA_TX_MSG_CLOUD_CONNECTION, NULL, 0);  //connect immediately
            }

            if(false == u8Discard)
            {
                printf("post cnt = %u continues\r\n" , g_u8PostRetry_IotRbData_Cnt);
            }
        }
        else //not last data
        {
            if(g_u8PostRetry_IotRbData_Cnt >= IOT_DATA_POST_RETRY_MAX)
            {
                u8Discard = true;
                printf("post cnt = %u discard\r\n" , g_u8PostRetry_IotRbData_Cnt);
                if (IOT_RB_DATA_OK != IoT_Ring_Buffer_CheckEmpty(&g_stIotRbData))
                {
                    IoT_Ring_Buffer_Pop(&g_stIotRbData , &ptProperity);
                    IoT_Ring_Buffer_ReadIdxUpdate(&g_stIotRbData);

                    if(ptProperity.ubData!=NULL)
                        free(ptProperity.ubData);
                }
                osSemaphoreWait(g_tAppSemaphoreId, osWaitForever);
                g_u8PostRetry_IotRbData_Cnt = 0;
                osSemaphoreRelease(g_tAppSemaphoreId);
            }
            osDelay(100);
            Iot_Data_TxTask_MsgSend(IOT_DATA_TX_MSG_CLOUD_CONNECTION, NULL, 0);  //connect immediately

            if(false == u8Discard)
            {
                printf("post cnt = %u continues\r\n" , g_u8PostRetry_IotRbData_Cnt);
            }
        }
    }
    else
    {
        Iot_Data_TxTaskEvtHandler_CloudConnectRetry();
    }
}

static void Iot_Data_TxTaskEvtHandler_CloudKeepAlive(uint32_t evt_type, void *data, int len)
{
    IoT_Properity_t stKeppAlive = {0}; //Keep alive has no data

    IoT_Ring_Buffer_Push(&g_stKeepAliveQ, &stKeppAlive);
    Iot_Data_TxTask_MsgSend(IOT_DATA_TX_MSG_CLOUD_POST, NULL, 0);
}

static void Iot_Data_CloudDataPost(void)
{
    IoT_Properity_t tProperity = {0};
    uint32_t u32Ret;
    blewifi_wifi_set_dtim_t stSetDtim = {0};

    // send the data to cloud


    //1. add check if cloud connection or not
    if (true == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_WIFI_GOT_IP) &&
        true == BleWifi_COM_EventStatusGet(g_tIotDataEventGroup, IOT_DATA_EVENT_BIT_CLOUD_CONNECTED))
    {
        //2. check ringbuffer is empty or not, get data from ring buffer
        if (IOT_RB_DATA_OK == IoT_Ring_Buffer_CheckEmpty(&g_stIotRbData))
            return;

        if (IOT_RB_DATA_OK != IoT_Ring_Buffer_Pop(&g_stIotRbData, &tProperity))
            return;
        // check need waiting rx rsp
        if (true == BleWifi_COM_EventStatusGet(g_tIotDataEventGroup, IOT_DATA_EVENT_BIT_WAITING_RX_RSP))
            return;

        //3. contruct post data
        //4. send to Cloud
        stSetDtim.u32DtimValue = 0;
        stSetDtim.u32DtimEventBit = BW_WIFI_DTIM_EVENT_BIT_TX_USE;
        BleWifi_Wifi_Set_Config(BLEWIFI_WIFI_SET_DTIM , (void *)&stSetDtim);

        lwip_one_shot_arp_enable();

        u32Ret = Iot_Contruct_Post_Data_And_Send(&tProperity);

        //5. free the tx data from ring buffer
        if ((u32Ret == IOT_DATA_POST_RET_CONTINUE_DELETE) || (u32Ret == IOT_DATA_POST_RET_STOP_DELETE))
        {
            IoT_Ring_Buffer_ReadIdxUpdate(&g_stIotRbData);

            if (tProperity.ubData != NULL)
                free(tProperity.ubData);
        }

        //6. trigger the next Tx data post
        if ((u32Ret == IOT_DATA_POST_RET_CONTINUE_DELETE) || (u32Ret == IOT_DATA_POST_RET_CONTINUE_KEEP))
        {
            Iot_Data_TxTask_MsgSend(IOT_DATA_TX_MSG_CLOUD_POST, NULL, 0);
        }
    }
}

static void Iot_Data_TxTaskEvtHandler_CloudDisconnection(uint32_t evt_type, void *data, int len)
{
    osSemaphoreWait(g_tAppSemaphoreId, osWaitForever);
    osTimerStop(g_tmr_req_date);
    if ((g_tcp_hdl_ID!=-1)
        && (true == BleWifi_COM_EventStatusGet(g_tIotDataEventGroup, IOT_DATA_EVENT_BIT_CLOUD_CONNECTED)))
    {
        int Res = ws_close();
        printf("rd: ws_close(ret=%d)\n", Res);

        g_tx_ID = -1;
        g_rx_ID = -1;
        g_tcp_hdl_ID = -1;
        BleWifi_COM_EventStatusSet(g_tIotDataEventGroup, IOT_DATA_EVENT_BIT_CLOUD_CONNECTED, false);
        osSemaphoreRelease(g_tAppSemaphoreId);
        //Iot_Data_TxTask_MsgSend(IOT_DATA_TX_MSG_CLOUD_CONNECTION, NULL, 0);
    }
    else
    {
        osSemaphoreRelease(g_tAppSemaphoreId);
    }
}

static void Iot_Data_TxTaskEvtHandler_CloudWaitRxRspTimeout(uint32_t evt_type, void *data, int len)
{
    blewifi_wifi_set_dtim_t stSetDtim = {0};
    IoT_Properity_t ptProperity = {0};
    uint16_t u16QueueCount = 0;
    uint8_t u8Discard = false;

    printf("Wait Rx Timeout\r\n");

    App_Ctrl_MsgSend(APP_CTRL_MSG_DATA_POST_FAIL , NULL , 0);

    //timeout
    BleWifi_COM_EventStatusSet(g_tIotDataEventGroup, IOT_DATA_EVENT_BIT_WAITING_RX_RSP, false);

    if(IOT_DATA_WAITING_TYPE_KEEPALIVE == g_u8WaitingRspType)
    {
        osSemaphoreWait(g_tAppSemaphoreId, osWaitForever);
        g_u8PostRetry_KeepAlive_Cnt ++;
        osSemaphoreRelease(g_tAppSemaphoreId);

        if(g_u8PostRetry_KeepAlive_Cnt >= IOT_DATA_KEEP_ALIVE_RETRY_MAX)
        {
            osSemaphoreWait(g_tAppSemaphoreId, osWaitForever);
            g_u8PostRetry_KeepAlive_Fail_Round++;
            osSemaphoreRelease(g_tAppSemaphoreId);

            if (IOT_RB_DATA_OK != IoT_Ring_Buffer_CheckEmpty(&g_stKeepAliveQ))
            {
                IoT_Ring_Buffer_Pop(&g_stKeepAliveQ , &ptProperity);
                IoT_Ring_Buffer_ReadIdxUpdate(&g_stKeepAliveQ);

                if(ptProperity.ubData!=NULL)
                    free(ptProperity.ubData);
            }
            osSemaphoreWait(g_tAppSemaphoreId, osWaitForever);
            g_u8PostRetry_KeepAlive_Cnt = 0;
            osSemaphoreRelease(g_tAppSemaphoreId);

            if(g_u8PostRetry_KeepAlive_Fail_Round >= IOT_DATA_KEEP_ALIVE_FAIL_ROUND_MAX)
            {
                printf("keep alive fail round >= %u , cloud disconnect\r\n" , IOT_DATA_KEEP_ALIVE_FAIL_ROUND_MAX);

                osSemaphoreWait(g_tAppSemaphoreId, osWaitForever);
                g_u8PostRetry_KeepAlive_Fail_Round = 0; //reset
                if(true == BleWifi_COM_EventStatusGet(g_tIotDataEventGroup, IOT_DATA_EVENT_BIT_CLOUD_CONNECTED))
                {
                    printf("disconnect by self\r\n");
                    Iot_Data_TxTask_MsgSend(IOT_DATA_TX_MSG_CLOUD_DISCONNECTION, NULL, 0);
                    Iot_Data_TxTask_MsgSend(IOT_DATA_TX_MSG_CLOUD_CONNECTION, NULL, 0);
                }
                osSemaphoreRelease(g_tAppSemaphoreId);
            }
        }
    }
    else if(IOT_DATA_WAITING_TYPE_DATA_POST == g_u8WaitingRspType)
    {
        osSemaphoreWait(g_tAppSemaphoreId, osWaitForever);
        g_u8PostRetry_IotRbData_Cnt ++;
        osSemaphoreRelease(g_tAppSemaphoreId);

        IoT_Ring_Buffer_GetQueueCount(&g_stIotRbData , &u16QueueCount);

        //printf("post retry cnt = %u\r\n" , g_u8PostRetry_IotRbData_Cnt);

        if(IOT_DATA_QUEUE_LAST_DATA_CNT == u16QueueCount) // last data
        {
            if(g_u8PostRetry_IotRbData_Cnt == IOT_DATA_POST_RETRY_MAX)
            {
                osSemaphoreWait(g_tAppSemaphoreId, osWaitForever);
                BleWifi_COM_EventStatusSet(g_tIotDataEventGroup, IOT_DATA_EVENT_BIT_POST_FAIL_RECONNECT, true);
                if(true == BleWifi_COM_EventStatusGet(g_tIotDataEventGroup, IOT_DATA_EVENT_BIT_CLOUD_CONNECTED))
                {
//                    printf("disconnect by self\r\n");
//                    Iot_Data_TxTask_MsgSend(IOT_DATA_TX_MSG_CLOUD_DISCONNECTION, NULL, 0);
//                    Iot_Data_TxTask_MsgSend(IOT_DATA_TX_MSG_CLOUD_CONNECTION, NULL, 0);
                }
                osSemaphoreRelease(g_tAppSemaphoreId);
            }
            else if(g_u8PostRetry_IotRbData_Cnt >= IOT_DATA_LAST_DATA_POST_RETRY_MAX)
            {
                u8Discard = true;
                printf("post cnt = %u discard\r\n" , g_u8PostRetry_IotRbData_Cnt);
                if (IOT_RB_DATA_OK != IoT_Ring_Buffer_CheckEmpty(&g_stIotRbData))
                {
                    IoT_Ring_Buffer_Pop(&g_stIotRbData , &ptProperity);
                    IoT_Ring_Buffer_ReadIdxUpdate(&g_stIotRbData);

                    if(ptProperity.ubData!=NULL)
                        free(ptProperity.ubData);
                }
                osSemaphoreWait(g_tAppSemaphoreId, osWaitForever);
                g_u8PostRetry_IotRbData_Cnt = 0;
                osSemaphoreRelease(g_tAppSemaphoreId);
            }

            if(false == u8Discard)
            {
                printf("post cnt = %u continues\r\n" , g_u8PostRetry_IotRbData_Cnt);
            }
        }
        else //not last data
        {
            if(g_u8PostRetry_IotRbData_Cnt >= IOT_DATA_POST_RETRY_MAX)
            {
                u8Discard = true;
                printf("post cnt = %u discard\r\n" , g_u8PostRetry_IotRbData_Cnt);
                if (IOT_RB_DATA_OK != IoT_Ring_Buffer_CheckEmpty(&g_stIotRbData))
                {
                    IoT_Ring_Buffer_Pop(&g_stIotRbData , &ptProperity);
                    IoT_Ring_Buffer_ReadIdxUpdate(&g_stIotRbData);

                    if(ptProperity.ubData!=NULL)
                        free(ptProperity.ubData);
                }
                osSemaphoreWait(g_tAppSemaphoreId, osWaitForever);
                g_u8PostRetry_IotRbData_Cnt = 0;
                BleWifi_COM_EventStatusSet(g_tIotDataEventGroup, IOT_DATA_EVENT_BIT_POST_FAIL_RECONNECT, true);
                if(true == BleWifi_COM_EventStatusGet(g_tIotDataEventGroup, IOT_DATA_EVENT_BIT_CLOUD_CONNECTED))
                {
//                    printf("disconnect by self\r\n");
//                    Iot_Data_TxTask_MsgSend(IOT_DATA_TX_MSG_CLOUD_DISCONNECTION, NULL, 0);
//                    Iot_Data_TxTask_MsgSend(IOT_DATA_TX_MSG_CLOUD_CONNECTION, NULL, 0);
                }
                osSemaphoreRelease(g_tAppSemaphoreId);
            }

            if(false == u8Discard)
            {
                printf("post cnt = %u continues\r\n" , g_u8PostRetry_IotRbData_Cnt);
            }
        }
    }
    osSemaphoreWait(g_tAppSemaphoreId, osWaitForever);
    g_u8WaitingRspType = IOT_DATA_WAITING_TYPE_NONE;
    osSemaphoreRelease(g_tAppSemaphoreId);

    Iot_Data_TxTask_MsgSend(IOT_DATA_TX_MSG_CLOUD_POST, NULL, 0);

    stSetDtim.u32DtimValue = BleWifi_Wifi_GetDtimSetting();
    stSetDtim.u32DtimEventBit = BW_WIFI_DTIM_EVENT_BIT_TX_USE;
    BleWifi_Wifi_Set_Config(BLEWIFI_WIFI_SET_DTIM , (void *)&stSetDtim);
}

static void Iot_Data_TxTaskEvtHandler_CloudPost(uint32_t evt_type, void *data, int len)
{
    uint8_t u8NeedColudConnectionCheck = 0;

    if(NULL != data)
    {
        u8NeedColudConnectionCheck = *(uint8_t *)data;
    }

    if(IOT_DATA_POST_CHECK_CLOUD_CONNECTION == u8NeedColudConnectionCheck)
    {
        printf("cloud retry table idx reset\r\n");
        g_u8CloudRetryIntervalIdx = 0;
        if (false == BleWifi_COM_EventStatusGet(g_tIotDataEventGroup , IOT_DATA_EVENT_BIT_CLOUD_CONNECTED))
        {
            Iot_Data_TxTask_MsgSend(IOT_DATA_TX_MSG_CLOUD_CONNECTION, NULL, 0);
        }
    }

    if (true == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_WIFI_GOT_IP) &&
        true == BleWifi_COM_EventStatusGet(g_tIotDataEventGroup, IOT_DATA_EVENT_BIT_CLOUD_CONNECTED))
    {

        //Pri g_stCloudRspQ > g_stKeepAliveQ > g_stIotRbData
        if(IOT_RB_DATA_FAIL == IoT_Ring_Buffer_CheckEmpty(&g_stCloudRspQ))
        {
            Iot_Handle_Send_CloudRsp();
        }
        else if (false == BleWifi_COM_EventStatusGet(g_tIotDataEventGroup, IOT_DATA_EVENT_BIT_WAITING_RX_RSP))
        {
            if(IOT_RB_DATA_FAIL == IoT_Ring_Buffer_CheckEmpty(&g_stKeepAliveQ))
            {
                Iot_Handle_KeepAlive();
            }
            else if(IOT_RB_DATA_FAIL == IoT_Ring_Buffer_CheckEmpty(&g_stIotRbData))
            {
                Iot_Data_CloudDataPost();
            }
        }
    }
}

static void Iot_Data_TxTaskEvtHandler_AckPostTimeout(uint32_t evt_type, void *data, int len)
{
    blewifi_wifi_set_dtim_t stSetDtim = {0};

    stSetDtim.u32DtimValue = BleWifi_Wifi_GetDtimSetting();
    stSetDtim.u32DtimEventBit = BW_WIFI_DTIM_EVENT_BIT_TX_CLOUD_ACK_POST;
    BleWifi_Wifi_Set_Config(BLEWIFI_WIFI_SET_DTIM , (void *)&stSetDtim);
}

void Iot_Data_TxTaskEvtHandler(uint32_t evt_type, void *data, int len)
{
    uint32_t i = 0;

    while (g_tIotDataTxTaskEvtHandlerTbl[i].ulEventId != 0xFFFFFFFF)
    {
        // match
        if (g_tIotDataTxTaskEvtHandlerTbl[i].ulEventId == evt_type)
        {
            g_tIotDataTxTaskEvtHandlerTbl[i].fpFunc(evt_type, data, len);
            break;
        }

        i++;
    }

    // not match
    if (g_tIotDataTxTaskEvtHandlerTbl[i].ulEventId == 0xFFFFFFFF)
    {
    }
}

void Iot_Data_TxTask(void *args)
{
    osEvent rxEvent;
    xIotDataMessage_t *rxMsg;

    while (1)
    {
        /* Wait event */
        rxEvent = osMessageGet(g_tIotDataTxQueueId, osWaitForever);
        if(rxEvent.status != osEventMessage)
            continue;

        rxMsg = (xIotDataMessage_t *)rxEvent.value.p;

        osTimerStop(g_iot_tx_sw_reset_timeout_timer);
        osTimerStart(g_iot_tx_sw_reset_timeout_timer, SW_RESET_TIME);

        Iot_Data_TxTaskEvtHandler(rxMsg->event, rxMsg->ucaMessage, rxMsg->length);

        /* Release buffer */
        if (rxMsg != NULL)
            free(rxMsg);

        osTimerStop(g_iot_tx_sw_reset_timeout_timer);
    }
}

int Iot_Data_TxTask_MsgSend(int msg_type, uint8_t *data, int data_len)
{
    xIotDataMessage_t *pMsg = 0;

	if (NULL == g_tIotDataTxQueueId)
	{
        BLEWIFI_ERROR("BLEWIFI: No IoT Tx task queue \r\n");
        return -1;
	}

    /* Mem allocate */
    pMsg = malloc(sizeof(xIotDataMessage_t) + data_len);
    if (pMsg == NULL)
	{
        BLEWIFI_ERROR("BLEWIFI: IoT Tx task pmsg allocate fail \r\n");
	    goto error;
    }

    pMsg->event = msg_type;
    pMsg->length = data_len;
    if (data_len > 0)
    {
        memcpy(pMsg->ucaMessage, data, data_len);
    }

    if (osMessagePut(g_tIotDataTxQueueId, (uint32_t)pMsg, 0) != osOK)
    {
        printf("BLEWIFI: IoT Tx task message send fail \r\n");
        goto error;
    }

    return 0;

error:
	if (pMsg != NULL)
	{
	    free(pMsg);
    }

	return -1;
}

void Iot_Data_TxInit(void)
{
    osThreadDef_t task_def;
    osMessageQDef_t queue_def;

    /* Create IoT Tx data ring buffer */
    IoT_Ring_Buffer_Init(&g_stCloudRspQ , IOT_RB_CLOUD_RSP_COUNT);
    IoT_Ring_Buffer_Init(&g_stKeepAliveQ , IOT_RB_KEEP_ALIVE_COUNT);
    IoT_Ring_Buffer_Init(&g_stIotRbData , IOT_RB_COUNT);

    /* Create IoT Tx message queue*/
    queue_def.item_sz = sizeof(xIotDataMessage_t);
    queue_def.queue_sz = IOT_DEVICE_DATA_QUEUE_SIZE_TX;
    g_tIotDataTxQueueId = osMessageCreate(&queue_def, NULL);
    if(g_tIotDataTxQueueId == NULL)
    {
        BLEWIFI_ERROR("BLEWIFI: IoT Tx create queue fail \r\n");
    }

    /* Create IoT Tx task */
    task_def.name = "iot tx";
    task_def.stacksize = IOT_DEVICE_DATA_TASK_STACK_SIZE_TX;
    task_def.tpriority = IOT_DEVICE_DATA_TASK_PRIORITY_TX;
    task_def.pthread = Iot_Data_TxTask;
    g_tIotDataTxTaskId = osThreadCreate(&task_def, (void*)NULL);
    if(g_tIotDataTxTaskId == NULL)
    {
        BLEWIFI_INFO("BLEWIFI: IoT Tx task create fail \r\n");
    }
    else
    {
        BLEWIFI_INFO("BLEWIFI: IoT Tx task create successful \r\n");
    }
}
#endif  // end of #if (IOT_DEVICE_DATA_TX_EN == 1)

#if (IOT_DEVICE_DATA_RX_EN == 1)

int8_t Iot_Recv_Data_from_Cloud(void)
{
    uint8_t u8RecvBuf[BUFFER_SIZE] = {0};
    uint32_t ulRecvlen = 0;
    int8_t s8Ret = -1;

    //1. Check Cloud conection or not
    if (true == BleWifi_COM_EventStatusWait(g_tIotDataEventGroup, IOT_DATA_EVENT_BIT_CLOUD_CONNECTED, 0xFFFFFFFF))
    {
        //2. Recv data from cloud
        s8Ret = Coolkit_Cloud_Recv(u8RecvBuf, &ulRecvlen);
        if(s8Ret<0)
        {
            printf("Recv data from Cloud fail.(ret=%d)\n", s8Ret);
            goto fail;
        }

        //3. Data parser
        s8Ret = Iot_Data_Parser(u8RecvBuf, ulRecvlen);
        if(s8Ret<0)
        {
            printf("Iot_Data_Parser failed\n");
            goto fail;
        }
    }

    s8Ret = 0;

fail:
    return s8Ret;
}


void Iot_Data_RxTask(void *args)
{
    int8_t s8Ret = 0;

    // do the rx behavior
    while (1)
    {
        if (true == BleWifi_COM_EventStatusWait(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_WIFI_GOT_IP , 0xFFFFFFFF))
        {
            // rx behavior
            s8Ret = Iot_Recv_Data_from_Cloud();
            if(s8Ret<0)
            {
                printf("Recv data from Cloud fail.\n");
                osDelay(2000); // if do nothing for rx behavior, the delay must be exist.
                               // if do something for rx behavior, the delay could be removed.
            }
        }
    }
}

void Iot_Data_RxInit(void)
{
    osThreadDef_t task_def;

    /* Create IoT Rx task */
    task_def.name = "iot rx";
    task_def.stacksize = IOT_DEVICE_DATA_TASK_STACK_SIZE_RX;
    task_def.tpriority = IOT_DEVICE_DATA_TASK_PRIORITY_RX;
    task_def.pthread = Iot_Data_RxTask;
    g_tIotDataRxTaskId = osThreadCreate(&task_def, (void*)NULL);
    if(g_tIotDataRxTaskId == NULL)
    {
        BLEWIFI_INFO("BLEWIFI: IoT Rx task create fail \r\n");
    }
    else
    {
        BLEWIFI_INFO("BLEWIFI: IoT Rx task create successful \r\n");
    }
}
#endif  // end of #if (IOT_DEVICE_DATA_RX_EN == 1)

static void Iot_data_Tx_Wait_TimeOutCallBack(void const *argu)
{
    Iot_Data_TxTask_MsgSend(IOT_DATA_TX_MSG_CLOUD_WAIT_RX_RSP_TIMEOUT, NULL, 0);
}

static void Iot_data_Tx_AckPost_TimeOutCallBack(void const *argu)
{
    Iot_Data_TxTask_MsgSend(IOT_DATA_TX_MSG_CLOUD_POST_ACK_TIMEOUT, NULL, 0);
}

static void Iot_data_SwReset_TimeOutCallBack(void const *argu)
{
    tracer_drct_printf("Iot sw reset\r\n");
    Hal_Sys_SwResetAll();
}

static void Iot_data_cloud_connect_retry_TimeOutCallBack(void const *argu)
{
    Iot_Data_TxTask_MsgSend(IOT_DATA_TX_MSG_CLOUD_CONNECTION, NULL, 0);
}

#if (IOT_DEVICE_DATA_TX_EN == 1) || (IOT_DEVICE_DATA_RX_EN == 1)
EventGroupHandle_t g_tIotDataEventGroup;

void Iot_Data_Init(void)
{
    osTimerDef_t tTimerDef;

    /* Create the event group */
    if (false == BleWifi_COM_EventCreate(&g_tIotDataEventGroup))
    {
        BLEWIFI_ERROR("IoT: create event group fail \r\n");
    }

#if (IOT_DEVICE_DATA_TX_EN == 1)
    Iot_Data_TxInit();
#endif

#if (IOT_DEVICE_DATA_RX_EN == 1)
    Iot_Data_RxInit();
#endif

    ws_init();

    tTimerDef.ptimer = Coolkit_Req_Date;
    g_tmr_req_date = osTimerCreate(&tTimerDef, osTimerPeriodic, NULL);//osTimerOnce osTimerPeriodic
    if (g_tmr_req_date == NULL)
    {
        printf("To create the timer for req date.\n");
    }

    /* create iot tx wait timeout timer */
    tTimerDef.ptimer = Iot_data_Tx_Wait_TimeOutCallBack;
    g_iot_tx_wait_timeout_timer = osTimerCreate(&tTimerDef, osTimerOnce, NULL);
    if (g_iot_tx_wait_timeout_timer == NULL)
    {
        BLEWIFI_ERROR("BLEWIFI: create g_iot_tx_wait_timeout_timer timeout timer fail \r\n");
    }

    /* create iot tx cloud rsp post timeout timer */
    tTimerDef.ptimer = Iot_data_Tx_AckPost_TimeOutCallBack;
    g_iot_tx_ack_post_timeout_timer = osTimerCreate(&tTimerDef, osTimerOnce, NULL);
    if (g_iot_tx_ack_post_timeout_timer == NULL)
    {
        BLEWIFI_ERROR("BLEWIFI: create g_iot_tx_ack_post_timeout_timer timeout timer fail \r\n");
    }

    /* create iot tx sw reset timeout timer */
    tTimerDef.ptimer = Iot_data_SwReset_TimeOutCallBack;
    g_iot_tx_sw_reset_timeout_timer = osTimerCreate(&tTimerDef, osTimerOnce, NULL);
    if (g_iot_tx_sw_reset_timeout_timer == NULL)
    {
        BLEWIFI_ERROR("BLEWIFI: create g_iot_tx_sw_reset_timeout_timer timeout timer fail \r\n");
    }

    /* create iot tx sw reset timeout timer */
    tTimerDef.ptimer = Iot_data_cloud_connect_retry_TimeOutCallBack;
    g_iot_tx_cloud_connect_retry_timer = osTimerCreate(&tTimerDef, osTimerOnce, NULL);
    if (g_iot_tx_cloud_connect_retry_timer == NULL)
    {
        BLEWIFI_ERROR("BLEWIFI: create g_iot_tx_cloud_connect_retry_timer timeout timer fail \r\n");
    }
}
#endif  // end of #if (IOT_DEVICE_DATA_TX_EN == 1) || (IOT_DEVICE_DATA_RX_EN == 1)
