/******************************************************************************
*  Copyright 2017 - 2018, Opulinks Technology Ltd.
*  ---------------------------------------------------------------------------
*  Statement:
*  ----------
*  This software is protected by Copyright and the information contained
*  herein is confidential. The software may not be copied and the information
*  contained herein may not be used or disclosed except with the written
*  permission of Opulinks Technology Ltd. (C) 2018
******************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "cmsis_os.h"
#include "event_loop.h"
#include "wifi_api.h"
#include "wifi_event.h"
#include "wifi_event_handler.h"
#include "lwip_helper.h"
#include "httpclient.h"
#include "mw_ota_def.h"
#include "mw_ota.h"
#include "hal_system.h"
#include "blewifi_configuration.h"
#include "blewifi_common.h"
#include "iot_ota_http.h"
#include "lwip/etharp.h"
#include "iot_configuration.h"
#include "app_ctrl.h"
#include "blewifi_wifi_api.h"
#include "controller_wifi.h"
#include "controller_wifi_com.h"

#if (IOT_WIFI_OTA_FUNCTION_EN == 1)
/****************************************************************************
 * Static variables
 ****************************************************************************/
osThreadId   g_tIotHttpOtaTaskId;
osMessageQId g_tIotHttpOtaQueueId;
osTimerId    g_tIotHttpOtaSwReset_timer = NULL;

static httpclient_t g_tIotOta_Httpclient = {0};
static const char *TAG = "ota";
#define HTTPCLIENT_TIMEOUT_TIME  (5000)

static int _IoT_OTA_HTTP_Retrieve_Offset_Server_Version(httpclient_t *client, httpclient_data_t *client_data, int offset, int parse_hdr, uint16_t *uwfid)
{
    int ret = HTTPCLIENT_ERROR_CONN;
    uint8_t ota_hdr1_start = 0;
    T_MwOtaFlashHeader *ota_hdr = NULL;

    ota_hdr1_start = parse_hdr;

    do
    {
        // Receive response from server
        ret = httpclient_recv_response(client, client_data);
        if (ret < 0)
        {
            LOG_E(TAG, "http client recv response error, ret = %d \r\n", ret);
            return ret;
        }

        if (ota_hdr1_start == 1)
        {
            BLEWIFI_DUMP(TAG, (uint8_t*)client_data->response_buf, IOT_OTA_HTTP_BUF_SIZE-1);

            ota_hdr = (T_MwOtaFlashHeader*)client_data->response_buf;
            LOG_E(TAG, "proj_id=%d, chip_id=%d, fw_id=%d, checksum=%d, total_len=%d\r\n",
                  ota_hdr->uwProjectId, ota_hdr->uwChipId, ota_hdr->uwFirmwareId, ota_hdr->ulImageSum, ota_hdr->ulImageSize);

            ota_hdr1_start = 0;

            if (ota_hdr->uwFirmwareId == 0)
                *uwfid = 0;
            else
                *uwfid = ota_hdr->uwFirmwareId;
        }

        if ( (client_data->response_content_len - client_data->retrieve_len) == offset )
            break;

    }
    while (ret == HTTPCLIENT_RETRIEVE_MORE_DATA);

    return HTTPCLIENT_OK;
}

static int _IoT_OTA_HTTP_Retrieve_Offset(httpclient_t *client, httpclient_data_t *client_data, int offset, int parse_hdr)
{
    int ret = HTTPCLIENT_ERROR_CONN;
    uint8_t ota_hdr1_start = 0;
    T_MwOtaFlashHeader *ota_hdr = NULL;

    ota_hdr1_start = parse_hdr;

    do
    {
        // Receive response from server
        ret = httpclient_recv_response(client, client_data);
        if (ret < 0)
        {
            LOG_E(TAG, "http client recv response error, ret = %d \r\n", ret);
            return ret;
        }

        if (ota_hdr1_start == 1)
        {
            BLEWIFI_DUMP(TAG, (uint8_t*)client_data->response_buf, IOT_OTA_HTTP_BUF_SIZE-1);

            ota_hdr = (T_MwOtaFlashHeader*)client_data->response_buf;
            LOG_E(TAG, "proj_id=%d, chip_id=%d, fw_id=%d, checksum=%d, total_len=%d\r\n",
                  ota_hdr->uwProjectId, ota_hdr->uwChipId, ota_hdr->uwFirmwareId, ota_hdr->ulImageSum, ota_hdr->ulImageSize);

            if (MwOta_Prepare(ota_hdr->uwProjectId, ota_hdr->uwChipId, ota_hdr->uwFirmwareId, ota_hdr->ulImageSize, ota_hdr->ulImageSum) != MW_OTA_OK)
            {
                LOG_I(TAG, "MwOta_Prepare fail\r\n");
                return -1;
            }
            ota_hdr1_start = 0;
        }

        if ( (client_data->response_content_len - client_data->retrieve_len) == offset )
            break;

    }
    while (ret == HTTPCLIENT_RETRIEVE_MORE_DATA);

    return HTTPCLIENT_OK;
}

static int _IoT_OTA_HTTP_Retrieve_Get(char* get_url, char* buf, uint32_t len)
{
    int ret = HTTPCLIENT_ERROR_CONN;
    httpclient_data_t client_data = {0};
    uint32_t write_count = 0;
    uint32_t data_recv = 0;
    uint32_t recv_temp = 0;
    uint32_t data_len = 0;

    client_data.response_buf = buf;
    client_data.response_buf_len = len;

    // Send request to server
    ret = httpclient_send_request(&g_tIotOta_Httpclient, get_url, HTTPCLIENT_GET, &client_data);
    if (ret < 0)
    {
        LOG_E(TAG, "http client fail to send request \r\n");
        return ret;
    }

    // Response body start

    // skip 2nd boot agent,  0x00000000 ~ 0x00003000 : 12 KB,
    ret = _IoT_OTA_HTTP_Retrieve_Offset(&g_tIotOta_Httpclient, &client_data, MW_OTA_HEADER_ADDR_1, 0);
    if (ret < 0)
    {
        LOG_E(TAG, "http retrieve offset error, ret = %d \r\n", ret);
        return ret;
    }

    // parse 1st OTA header, 0x00003000 ~ 0x00004000 : 4 KB
    ret = _IoT_OTA_HTTP_Retrieve_Offset(&g_tIotOta_Httpclient, &client_data, MW_OTA_HEADER_ADDR_2, 1);
    if (ret < 0)
    {
        LOG_E(TAG, "http retrieve offset error, ret = %d \r\n", ret);
        return ret;
    }

    // skip 2st OTA header,  0x00003000 ~ 0x00004000 : 4 KB
    ret = _IoT_OTA_HTTP_Retrieve_Offset(&g_tIotOta_Httpclient, &client_data, MW_OTA_IMAGE_ADDR_1, 0);
    if (ret < 0)
    {
        LOG_E(TAG, "http retrieve offset error, ret = %d \r\n", ret);
        return ret;
    }

    recv_temp = client_data.retrieve_len;
    data_recv = client_data.response_content_len - client_data.retrieve_len;
    do
    {
        // Receive response from server
        ret = httpclient_recv_response(&g_tIotOta_Httpclient, &client_data);
        if (ret < 0)
        {
            LOG_E(TAG, "http client recv response error, ret = %d \r\n", ret);
            return ret;
        }

        data_len = recv_temp - client_data.retrieve_len;
        if (MwOta_DataIn((uint8_t*)client_data.response_buf, data_len) != MW_OTA_OK)
        {
            LOG_I(TAG, "MwOta_DataIn fail\r\n");
            return -1;
        }

        write_count += data_len;
        data_recv += data_len;
        recv_temp = client_data.retrieve_len;
        LOG_I(TAG, "have written image length %d\r\n", write_count);
        LOG_I(TAG, "download progress = %u \r\n", data_recv * 100 / client_data.response_content_len);

    }
    while (ret == HTTPCLIENT_RETRIEVE_MORE_DATA);

    LOG_I(TAG, "total write data length : %d\r\n", write_count);
    LOG_I(TAG, "data received length : %d\r\n", data_recv);

    if (data_recv != client_data.response_content_len || httpclient_get_response_code(&g_tIotOta_Httpclient) != 200)
    {
        LOG_E(TAG, "data received not completed, or invalid error code \r\n");
        return -1;
    }
    else if (data_recv == 0)
    {
        LOG_E(TAG, "receive length is zero, file not found \n");
        return -2;
    }
    else
    {
        LOG_I(TAG, "download success \n");
        return ret;
    }
}

static int _IoT_OTA_HTTP_Retrieve_Get_Server_Version(char* get_url, char* buf, uint32_t len, uint16_t *uwfid)
{
    int ret = HTTPCLIENT_ERROR_CONN;
    httpclient_data_t client_data = {0};

    client_data.response_buf = buf;
    client_data.response_buf_len = len;

    // Send request to server
    ret = httpclient_send_request(&g_tIotOta_Httpclient, get_url, HTTPCLIENT_GET, &client_data);
    if (ret < 0)
    {
        LOG_E(TAG, "http client fail to send request \r\n");
        return ret;
    }

    // Response body start

    // skip 2nd boot agent,  0x00000000 ~ 0x00003000 : 12 KB,
    ret = _IoT_OTA_HTTP_Retrieve_Offset_Server_Version(&g_tIotOta_Httpclient, &client_data, MW_OTA_HEADER_ADDR_1, 0, uwfid);
    if (ret < 0)
    {
        LOG_E(TAG, "http retrieve offset error, ret = %d \r\n", ret);
        return ret;
    }

    // parse 1st OTA header, 0x00003000 ~ 0x00004000 : 4 KB
    ret = _IoT_OTA_HTTP_Retrieve_Offset_Server_Version(&g_tIotOta_Httpclient, &client_data, MW_OTA_HEADER_ADDR_2, 1, uwfid);
    if (ret < 0)
    {
        LOG_E(TAG, "http retrieve offset error, ret = %d \r\n", ret);
        return ret;
    }

    // skip 2st OTA header,  0x00003000 ~ 0x00004000 : 4 KB
    ret = _IoT_OTA_HTTP_Retrieve_Offset_Server_Version(&g_tIotOta_Httpclient, &client_data, MW_OTA_IMAGE_ADDR_1, 0, uwfid);
    if (ret < 0)
    {
        LOG_E(TAG, "http retrieve offset error, ret = %d \r\n", ret);
        return ret;
    }

    LOG_I(TAG, "download success \n");
    return ret;

}

int IoT_OTA_HTTP_Get_Server_Version(char *param, uint16_t *uwfid)
{
    char get_url[IOT_OTA_HTTP_URL_BUF_LEN];
    int32_t ret = HTTPCLIENT_ERROR_CONN;
    uint32_t len_param = strlen(param);
    uint16_t retry_count = 0;
    uint16_t pid;
    uint16_t cid;
    uint16_t fid;

    if (len_param < 1)
    {
        return -1;
    }

    memset(get_url, 0, IOT_OTA_HTTP_URL_BUF_LEN);
    LOG_I(TAG, "url length: %d\n", strlen(param));
    strcpy(get_url, param);

    char* buf = malloc(IOT_OTA_HTTP_BUF_SIZE);
    if (buf == NULL)
    {
        LOG_E(TAG, "buf malloc failed.\r\n");

        return -1;
    }

    lwip_auto_arp_enable(1, 0);

    // Connect to server
    do
    {
        ret = httpclient_connect(&g_tIotOta_Httpclient, get_url);
        if (!ret)
        {
            LOG_I(TAG, "connect to http server 123");
            break;
        }
        else
        {
            LOG_I(TAG, "connect to http server failed! retry again");
            osDelay(1000);
            retry_count++;
            continue;
        }
    }
    while(retry_count < 3);

    MwOta_VersionGet(&pid, &cid, &fid);
    //LOG_I(TAG, "pid=%d, cid=%d, fid=%d\r\n", pid, cid, fid);

    ret = _IoT_OTA_HTTP_Retrieve_Get_Server_Version(get_url, buf, IOT_OTA_HTTP_BUF_SIZE, uwfid);
    if (ret < 0)
    {
        if (MwOta_DataGiveUp() != MW_OTA_OK)
        {
            LOG_E(TAG, "MwOta_DataGiveUp error.\r\n");
        }
    }
    else
    {
        if (MwOta_DataFinish() != MW_OTA_OK)
        {
            LOG_E(TAG, "MwOta_DataFinish error.\r\n");
        }
    }

// fail:
    LOG_I(TAG, "download result = %d \r\n", (int)ret);

    // Close http connection
    httpclient_close(&g_tIotOta_Httpclient);
    free(buf);
    buf = NULL;

    if (HTTPCLIENT_OK == ret)
        return 0;
    else
        return -1;
}

int IoT_OTA_HTTP_Download(char *param)
{
    char get_url[IOT_OTA_HTTP_URL_BUF_LEN];
    int32_t ret = HTTPCLIENT_ERROR_CONN;
    uint32_t len_param = strlen(param);
    uint16_t retry_count = 0;
    uint16_t pid;
    uint16_t cid;
    uint16_t fid;

    if (len_param < 1)
    {
        return -1;
    }

    memset(get_url, 0, IOT_OTA_HTTP_URL_BUF_LEN);
    LOG_I(TAG, "url length: %d\n", strlen(param));
    strcpy(get_url, param);

    char* buf = malloc(IOT_OTA_HTTP_BUF_SIZE);
    if (buf == NULL)
    {
        LOG_E(TAG, "buf malloc failed.\r\n");
        return -1;
    }

    lwip_auto_arp_enable(1, 0);

    g_tIotOta_Httpclient.timeout_ms = HTTPCLIENT_TIMEOUT_TIME;

    // Connect to server
    do
    {
        ret = httpclient_connect(&g_tIotOta_Httpclient, get_url);
        if (!ret)
        {
            LOG_I(TAG, "connect to http server");
            break;
        }
        else
        {
            LOG_I(TAG, "connect to http server failed! retry again");
            osDelay(1000);
            retry_count++;
            continue;
        }
    }
    while(retry_count < 3);

    MwOta_VersionGet(&pid, &cid, &fid);

    LOG_I(TAG, "pid=%d, cid=%d, fid=%d\r\n", pid, cid, fid);

    ret = _IoT_OTA_HTTP_Retrieve_Get(get_url, buf, IOT_OTA_HTTP_BUF_SIZE);
    if (ret < 0)
    {
        if (MwOta_DataGiveUp() != MW_OTA_OK)
        {
            LOG_E(TAG, "MwOta_DataGiveUp error.\r\n");
        }
    }
    else
    {
        if (MwOta_DataFinish() != MW_OTA_OK)
        {
            LOG_E(TAG, "MwOta_DataFinish error.\r\n");
        }
    }

// fail:
    LOG_I(TAG, "download result = %d \r\n", (int)ret);

    // Close http connection
    httpclient_close(&g_tIotOta_Httpclient);
    free(buf);
    buf = NULL;

    if (HTTPCLIENT_OK == ret)
        return 0;
    else
        return -1;
}

void IoT_OTA_HTTP_Task_Evt_Handler(uint32_t u32EventType, void *pData, uint32_t u32Len)
{
    blewifi_wifi_set_dtim_t stSetDtim = {0};

    switch (u32EventType)
    {
        case IOT_OTA_HTTP_MSG_TRIG:
            BLEWIFI_INFO("BLEWIFI: MSG BLEWIFI_CTRL_HTTP_OTA_MSG_TRIG \r\n");
            App_Ctrl_MsgSend(APP_CTRL_MSG_OTHER_OTA_ON, NULL, 0);

            stSetDtim.u32DtimValue = 0;
            stSetDtim.u32DtimEventBit = BW_WIFI_DTIM_EVENT_BIT_OTA_USE;
            BleWifi_Wifi_Set_Config(BLEWIFI_WIFI_SET_DTIM , (void *)&stSetDtim);

            lwip_one_shot_arp_enable();
            CtrlWifi_PsStateForce(STA_PS_AWAKE_MODE, 0);

            if (IoT_OTA_HTTP_Download(pData) != 0)
            {
                stSetDtim.u32DtimValue = BleWifi_Wifi_GetDtimSetting();
                stSetDtim.u32DtimEventBit = BW_WIFI_DTIM_EVENT_BIT_OTA_USE;
                BleWifi_Wifi_Set_Config(BLEWIFI_WIFI_SET_DTIM , (void *)&stSetDtim);
                CtrlWifi_PsStateForce(STA_PS_NONE, 0);

                App_Ctrl_MsgSend(APP_CTRL_MSG_OTHER_OTA_OFF_FAIL, NULL, 0);
            }
            else
            {
                stSetDtim.u32DtimValue = BleWifi_Wifi_GetDtimSetting();
                stSetDtim.u32DtimEventBit = BW_WIFI_DTIM_EVENT_BIT_OTA_USE;
                BleWifi_Wifi_Set_Config(BLEWIFI_WIFI_SET_DTIM , (void *)&stSetDtim);
                CtrlWifi_PsStateForce(STA_PS_NONE, 0);

                App_Ctrl_MsgSend(APP_CTRL_MSG_OTHER_OTA_OFF, NULL, 0);
            }
            break;

        default:
            break;
    }
}

void IoT_OTA_HTTP_Task(void *args)
{
    osEvent rxEvent;
    xIotOtaHttpMessage_t *rxMsg;

    for(;;)
    {
        /* Wait event */
        rxEvent = osMessageGet(g_tIotHttpOtaQueueId, osWaitForever);
        if(rxEvent.status != osEventMessage)
            continue;

        osTimerStop(g_tIotHttpOtaSwReset_timer);
        osTimerStart(g_tIotHttpOtaSwReset_timer, SW_RESET_TIME);

        rxMsg = (xIotOtaHttpMessage_t *)rxEvent.value.p;
        IoT_OTA_HTTP_Task_Evt_Handler(rxMsg->u32Event, rxMsg->u8aMessage, rxMsg->u32Length);

        /* Release buffer */
        if (rxMsg != NULL)
        {
            free(rxMsg);
        }

        osTimerStop(g_tIotHttpOtaSwReset_timer);
    }
}

static void IoT_OTA_SwReset_TimeOutCallBack(void const *argu)
{
    tracer_drct_printf("OTA sw reset\r\n");
    Hal_Sys_SwResetAll();
}

int IoT_OTA_HTTP_Msg_Send(uint32_t u32MsgType, uint8_t *pu8Data, uint32_t u32DataLen)
{
    int iRet = -1;
    xIotOtaHttpMessage_t *pMsg = NULL;

    /* Mem allocate */
    pMsg = malloc(sizeof(xIotOtaHttpMessage_t) + u32DataLen);
    if (pMsg == NULL)
    {
        LOG_E(TAG, "http ota task message allocate fail \r\n");
        goto done;
    }

    pMsg->u32Event = u32MsgType;
    pMsg->u32Length = u32DataLen;
    if (u32DataLen > 0)
    {
        memcpy(pMsg->u8aMessage, pu8Data, u32DataLen);
    }

    if (osMessagePut(g_tIotHttpOtaQueueId, (uint32_t)pMsg, 0) != osOK)
    {
        LOG_E(TAG, "http ota task message send fail \r\n");
        goto done;
    }

    iRet = 0;

done:
    if(iRet)
    {
        if (pMsg != NULL)
        {
            free(pMsg);
        }
    }

    return iRet;
}

void IoT_OTA_HTTP_TrigReq(uint8_t *data)
{
    // data length = string length + 1 (\n)
    IoT_OTA_HTTP_Msg_Send(IOT_OTA_HTTP_MSG_TRIG, data, strlen((char*)data) + 1);
}

void IoT_OTA_HTTP_Init(void)
{
    osThreadDef_t task_def;
    osMessageQDef_t queue_def;
    osTimerDef_t tTimerDef;

    /* Create message queue*/
    queue_def.item_sz = sizeof(xIotOtaHttpMessage_t);
    queue_def.queue_sz = IOT_OTA_HTTP_QUEUE_SIZE;
    g_tIotHttpOtaQueueId = osMessageCreate(&queue_def, NULL);
    if(g_tIotHttpOtaQueueId == NULL)
    {
		LOG_E(TAG, "create queue fail \r\n");
    }

    /* Create iot ota task */
    task_def.name = "iot http ota";
    task_def.stacksize = IOT_OTA_HTTP_STACK_SIZE;
    task_def.tpriority = IOT_OTA_HTTP_TASK_PRIORITY;
    task_def.pthread = IoT_OTA_HTTP_Task;
    g_tIotHttpOtaTaskId = osThreadCreate(&task_def, (void*)NULL);
    if(g_tIotHttpOtaTaskId == NULL)
    {
        LOG_E(TAG, "create task fail \r\n");
    }

    /* create OTA sw reset timeout timer */
    tTimerDef.ptimer = IoT_OTA_SwReset_TimeOutCallBack;
    g_tIotHttpOtaSwReset_timer = osTimerCreate(&tTimerDef, osTimerOnce, NULL);
    if (g_tIotHttpOtaSwReset_timer == NULL)
    {
        BLEWIFI_ERROR("BLEWIFI: create g_tIotHttpOtaSwReset_timer timer fail \r\n");
    }
}

#endif /* #if (IOT_WIFI_OTA_FUNCTION_EN == 1) */
