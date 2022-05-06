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

#include "blewifi_wifi_api.h"
#include "blewifi_data.h"
#include "blewifi_configuration.h"
#include "app_configuration.h"
#include "blewifi_server_app.h"
#include "blewifi_ble_api.h"
#include "controller_wifi.h"

#include "cmsis_os.h"
#include "wifi_api.h"
#include "wifi_nvm.h"
#include "event_loop.h"
#include "lwip_helper.h"
#include "sys_common_api.h"
#ifdef __BLEWIFI_TRANSPARENT__
#include "at_cmd_app_patch.h"
#endif
#include "ssidpwd_proc.h"
#include "app_ctrl.h"
#include "wpa_cli.h"
#include "blewifi_handle.h"

extern uint8_t g_ubBleWifiCtrlRequestRetryTimes;
osTimerId    g_tAppCtrlAutoConnectTriggerTimer;
EventGroupHandle_t g_tWifiFsmEventGroup;

osSemaphoreId g_tWifiInternalSemaphoreId;

//dtim event group
EventGroupHandle_t g_tDtimEventGroup;

uint32_t g_ulBleWifi_Wifi_BeaconTime;

extern uint8_t g_wifi_disconnectedDoneForAppDoWIFIScan;
extern EventGroupHandle_t g_tBleWifiCtrlEventGroup;
extern uint32_t g_BwWifiReqConnRetryTimes;

extern uint32_t g_u32TotalRetryTime;
extern uint32_t g_u32ConnectionStartTime;
extern uint32_t g_u32MaxOnceFailTime;
extern uint32_t g_u32WifiConnTimeout;
extern EventGroupHandle_t g_tAppCtrlEventGroup;
extern uint32_t g_ulAppCtrlDoAutoConnectCumulativeTime;
extern osMessageQId g_tBwWifiFsmCmdQueueId;


uint32_t g_u32BwWifiDtimSetting = 0;

static int BleWifi_Wifi_EventHandler_Start(wifi_event_id_t event_id, void *data, uint16_t length);
static int BleWifi_Wifi_EventHandler_Connected(wifi_event_id_t event_id, void *data, uint16_t length);
static int BleWifi_Wifi_EventHandler_Disconnected(wifi_event_id_t event_id, void *data, uint16_t length);
static int BleWifi_Wifi_EventHandler_ScanComplete(wifi_event_id_t event_id, void *data, uint16_t length);
static int BleWifi_Wifi_EventHandler_GotIp(wifi_event_id_t event_id, void *data, uint16_t length);
static int BleWifi_Wifi_EventHandler_ConnectionFailed(wifi_event_id_t event_id, void *data, uint16_t length);
static T_BleWifi_Wifi_EventHandlerTbl g_tWifiEventHandlerTbl[] =
{
    {WIFI_EVENT_STA_START,              BleWifi_Wifi_EventHandler_Start},
    {WIFI_EVENT_STA_CONNECTED,          BleWifi_Wifi_EventHandler_Connected},
    {WIFI_EVENT_STA_DISCONNECTED,       BleWifi_Wifi_EventHandler_Disconnected},
    {WIFI_EVENT_SCAN_COMPLETE,          BleWifi_Wifi_EventHandler_ScanComplete},
    {WIFI_EVENT_STA_GOT_IP,             BleWifi_Wifi_EventHandler_GotIp},
    {WIFI_EVENT_STA_CONNECTION_FAILED,  BleWifi_Wifi_EventHandler_ConnectionFailed},

    {0xFFFFFFFF,                        NULL}
};

static int _BleWifi_Wifi_GetManufName(uint8_t *pu8Name)
{
    uint16_t u16Ret = false;

    if (pu8Name == NULL)
        return u16Ret;

    memset(pu8Name, 0, STA_INFO_MAX_MANUF_NAME_SIZE);
    u16Ret = wifi_nvm_sta_info_read(WIFI_NVM_STA_INFO_MANUFACTURE_NAME, STA_INFO_MAX_MANUF_NAME_SIZE, pu8Name);

    return u16Ret;
}

static int _BleWifi_Wifi_SetManufName(uint8_t *pu8Name)
{
    uint16_t u16Ret = false;
    uint8_t u8Len;

    if (pu8Name == NULL)
        return u16Ret;

    u8Len = strlen((char *)pu8Name);
    if (u8Len > STA_INFO_MAX_MANUF_NAME_SIZE)
        u8Len = STA_INFO_MAX_MANUF_NAME_SIZE;

    u16Ret = wifi_nvm_sta_info_write(WIFI_NVM_STA_INFO_MANUFACTURE_NAME, u8Len, pu8Name);

    return u16Ret;
}

uint8_t BleWifi_Wifi_AutoConnectListNum(void)
{
    uint8_t ubApNum = 0;

    wifi_auto_connect_get_saved_ap_num(&ubApNum);
    return ubApNum;
}

static int _BleWifi_Wifi_Rssi(int8_t *ps8Rssi)
{
	int sRet = 0;
	sRet = wifi_connection_get_rssi(ps8Rssi);
	*ps8Rssi += BLEWIFI_COM_RF_RSSI_ADJUST;
	return sRet;
}
uint32_t BleWifi_Wifi_GetDtimSetting(void)
{
    return g_u32BwWifiDtimSetting;
}

int _BleWifi_Wifi_SetDTIM(blewifi_wifi_set_dtim_t *pstDtimSetting)
{
    static uint32_t g_u32DtimPrevious = 255;
    uint32_t u32DtimInterval;

    if(pstDtimSetting == NULL)
    {
        printf("pstDtimSetting is NULL\r\n");
        return 0;
    }

    if(pstDtimSetting->u32DtimEventBit == BW_WIFI_DTIM_EVENT_BIT_APP_AT_USE)
    {
        g_u32BwWifiDtimSetting = pstDtimSetting->u32DtimValue;
    }
    else
    {
        //set the dtim using owner
        if(pstDtimSetting->u32DtimValue == 0)
        {
            BleWifi_COM_EventStatusSet(g_tDtimEventGroup, pstDtimSetting->u32DtimEventBit , true);
        }
        else
        {
            BleWifi_COM_EventStatusSet(g_tDtimEventGroup, pstDtimSetting->u32DtimEventBit , false);

            if((true == BleWifi_COM_EventStatusGet(g_tDtimEventGroup, BW_WIFI_DTIM_EVENT_BIT_TX_USE))
                ||(true == BleWifi_COM_EventStatusGet(g_tDtimEventGroup, BW_WIFI_DTIM_EVENT_BIT_RX_USE))
                ||(true == BleWifi_COM_EventStatusGet(g_tDtimEventGroup, BW_WIFI_DTIM_EVENT_BIT_OTA_USE))
                ||(true == BleWifi_COM_EventStatusGet(g_tDtimEventGroup, BW_WIFI_DTIM_EVENT_BIT_DHCP_USE))
                ||(true == BleWifi_COM_EventStatusGet(g_tDtimEventGroup, BW_WIFI_DTIM_EVENT_BIT_TX_CLOUD_ACK_POST))
                ||(true == BleWifi_COM_EventStatusGet(g_tDtimEventGroup, BW_WIFI_DTIM_EVENT_BIT_TCP_ACK)))
            {
                printf("Dtim no change\r\n");
                return 0;
            }
        }
    }

    // DTIM interval
    u32DtimInterval = (pstDtimSetting->u32DtimValue + (g_ulBleWifi_Wifi_BeaconTime / 2)) / g_ulBleWifi_Wifi_BeaconTime;
    // the max is 8 bits
    if (u32DtimInterval > 255)
        u32DtimInterval = 255;

    // no change
    if (g_u32DtimPrevious == u32DtimInterval)
    {
        printf("Dtim no change 2\r\n");
        return 0;
    }

    g_u32DtimPrevious = u32DtimInterval;

    /* DTIM: skip n-1 */
    if (u32DtimInterval == 0)
    {
        printf("[ATS] Set Skip DTIM count = %u\r\n", 0);
        wifi_config_set_skip_dtim(0, false);

        return 0;
    }
    else
    {
        printf("[ATS] Set Skip DTIM count = %u\r\n", u32DtimInterval - 1);
        wifi_config_set_skip_dtim(u32DtimInterval - 1, false);

        return 0;
    }
}

void BleWifi_Wifi_UpdateBeaconInfo(void)
{
    wifi_scan_info_t tInfo;

    // get the information of Wifi AP
    wifi_sta_get_ap_info(&tInfo);

    // beacon time (ms)
    g_ulBleWifi_Wifi_BeaconTime = tInfo.beacon_interval * tInfo.dtim_period;

    // error handle
    if (g_ulBleWifi_Wifi_BeaconTime == 0)
        g_ulBleWifi_Wifi_BeaconTime = 100;
}

static int BleWifi_Wifi_EventHandler_Start(wifi_event_id_t event_id, void *data, uint16_t length)
{
    blewifi_wifi_set_dtim_t stSetDtim = {0};

    printf("\r\nWi-Fi Start \r\n");

    /* Tcpip stack and net interface initialization,  dhcp client process initialization. */
    lwip_network_init(WIFI_MODE_STA);

    /* DTIM */
    stSetDtim.u32DtimValue = 0;
    stSetDtim.u32DtimEventBit = BW_WIFI_DTIM_EVENT_BIT_DHCP_USE;
    BleWifi_Wifi_Set_Config(BLEWIFI_WIFI_SET_DTIM , (void *)&stSetDtim);

    BleWifi_Wifi_FSM_MsgSend(BW_WIFI_FSM_MSG_WIFI_INIT_COMPLETE, NULL, 0, NULL, BLEWIFI_QUEUE_BACK);

//    BleWifi_Ctrl_MsgSend(BLEWIFI_CTRL_MSG_WIFI_INIT_COMPLETE, NULL, 0);

    return 0;
}

static int BleWifi_Wifi_EventHandler_Connected(wifi_event_id_t event_id, void *data, uint16_t length)
{
    uint8_t reason = *((uint8_t*)data);

    printf("\r\nWi-Fi Connected, reason %d \r\n", reason);
//    lwip_net_start(WIFI_MODE_STA);
    BleWifi_Wifi_FSM_MsgSend(BW_WIFI_FSM_MSG_WIFI_CONNECTION_SUCCESS_IND, NULL, 0, NULL, BLEWIFI_QUEUE_BACK);
//    BleWifi_Ctrl_MsgSend(BLEWIFI_CTRL_MSG_WIFI_CONNECTION_IND, NULL, 0);

    return 0;
}

static int BleWifi_Wifi_EventHandler_Disconnected(wifi_event_id_t event_id, void *data, uint16_t length)
{
    uint8_t reason = *((uint8_t*)data);

    printf("\r\nWi-Fi Disconnected , reason %d\r\n", reason);
    BleWifi_Wifi_FSM_MsgSend(BW_WIFI_FSM_MSG_WIFI_DISCONNECTION_IND, &reason, sizeof(uint8_t) , NULL, BLEWIFI_QUEUE_BACK);
//    g_wifi_disconnectedDoneForAppDoWIFIScan = 1;

//    if ( reason == 255 ) {
//        /* Reset the beacon time (ms) */
//        g_ulBleWifi_Wifi_BeaconTime = 100;

//        /* DTIM */
//        BleWifi_Wifi_SetDTIM(0);

//        BleWifi_Ctrl_MsgSend(BLEWIFI_CTRL_MSG_WIFI_RESET_DEFAULT_IND, NULL, 0);
//    }
//    else
//    {
//        BleWifi_Ctrl_MsgSend(BLEWIFI_CTRL_MSG_WIFI_DISCONNECTION_IND, NULL, 0);
//    }

    return 0;
}

static int BleWifi_Wifi_EventHandler_ScanComplete(wifi_event_id_t event_id, void *data, uint16_t length)
{
    printf("\r\nWi-Fi Scan Done \r\n");
//    BleWifi_Ctrl_MsgSend(BLEWIFI_CTRL_MSG_WIFI_SCAN_DONE_IND, NULL, 0);
    BleWifi_Wifi_FSM_MsgSend(BW_WIFI_FSM_MSG_WIFI_SCAN_DONE_IND, NULL, 0, NULL, BLEWIFI_QUEUE_BACK);

    return 0;
}

static int BleWifi_Wifi_EventHandler_GotIp(wifi_event_id_t event_id, void *data, uint16_t length)
{
    printf("\r\nWi-Fi Got IP \r\n");
    BleWifi_Wifi_FSM_MsgSend(BW_WIFI_FSM_MSG_WIFI_GOT_IP_IND, NULL, 0, NULL, BLEWIFI_QUEUE_BACK);
//    BleWifi_Ctrl_MsgSend(BLEWIFI_CTRL_MSG_WIFI_GOT_IP_IND, NULL, 0);

    return 0;
}

static int BleWifi_Wifi_EventHandler_ConnectionFailed(wifi_event_id_t event_id, void *data, uint16_t length)
{
    uint8_t reason = *((uint8_t*)data);

    printf("\r\nWi-Fi Connected failed, reason %d\r\n", reason);
    BleWifi_Wifi_FSM_MsgSend(BW_WIFI_FSM_MSG_WIFI_CONNECTION_FAIL_IND, &reason, sizeof(uint8_t) , NULL, BLEWIFI_QUEUE_BACK);

    return 0;
}

// it is used in the Wifi task
int BleWifi_Wifi_EventHandlerCb(wifi_event_id_t event_id, void *data, uint16_t length)
{
    uint32_t i = 0;
    int lRet = 0;

    osSemaphoreWait(g_tWifiInternalSemaphoreId, osWaitForever);

#ifdef __BLEWIFI_TRANSPARENT__
    at_wifi_event_update_status(event_id, data, length);
#endif

    while (g_tWifiEventHandlerTbl[i].ulEventId != 0xFFFFFFFF)
    {
        // match
        if (g_tWifiEventHandlerTbl[i].ulEventId == event_id)
        {
            lRet = g_tWifiEventHandlerTbl[i].fpFunc(event_id, data, length);
            break;
        }

        i++;
    }

    // not match
    if (g_tWifiEventHandlerTbl[i].ulEventId == 0xFFFFFFFF)
    {
        printf("\r\n Unknown Event %d \r\n", event_id);
        lRet = 1;
    }

    osSemaphoreRelease(g_tWifiInternalSemaphoreId);

    return lRet;
}

int BleWifi_Wifi_Query_Status(uint32_t u32QueryType , void *pu8QueryData)
{
    blewifi_wifi_get_config_source_t *pstGetConfigSource = NULL;
    blewifi_wifi_get_ap_record_t *pstGetapRecord = NULL;
    blewifi_wifi_get_auto_conn_ap_info_t *pstGetAutoConnApInfo = NULL;
    wifi_scan_list_t *p_scan_list = NULL;

    switch(u32QueryType)
    {
        case BLEWIFI_WIFI_QUERY_AP_INFO:
            wpa_cli_getssid(((wifi_ap_record_t *)pu8QueryData)->ssid);
            wpa_cli_getbssid(((wifi_ap_record_t *)pu8QueryData)->bssid);
//            wifi_sta_get_ap_info((wifi_ap_record_t *)pu8QueryData);
            break;
        case BLEWIFI_WIFI_QUERY_GET_MAC_ADDR:
            wifi_config_get_mac_address(WIFI_MODE_STA , (uint8_t *)pu8QueryData);
            break;
        case BLEWIFI_WIFI_QUERY_GET_MANUFACTURER_NAME:
            _BleWifi_Wifi_GetManufName((uint8_t *)pu8QueryData);
            break;
        case BLEWIFI_WIFI_GET_CONFIG_SOURCE:
            pstGetConfigSource = (blewifi_wifi_get_config_source_t *)pu8QueryData;
            mac_addr_get_config_source(pstGetConfigSource->iface , pstGetConfigSource->type);
            break;
        case BLEWIFI_WIFI_QUERY_GET_RSSI:
            _BleWifi_Wifi_Rssi((int8_t *)pu8QueryData);
            break;
        case BLEWIFI_WIFI_GET_AP_NUM:
            wifi_scan_get_ap_num((uint16_t *)pu8QueryData);
            break;
        case BLEWIFI_WIFI_GET_AP_RECORD:
            pstGetapRecord = (blewifi_wifi_get_ap_record_t *)pu8QueryData;
            wifi_scan_get_ap_records(pstGetapRecord->pu16apCount, pstGetapRecord->pstScanInfo);
            break;
        case BLEWIFI_WIFI_GET_AUTO_CONN_AP_NUM:
            wifi_auto_connect_get_ap_num((uint8_t *)pu8QueryData);
            break;
        case BLEWIFI_WIFI_GET_AUTO_CONN_AP_INFO:
            pstGetAutoConnApInfo = (blewifi_wifi_get_auto_conn_ap_info_t *)pu8QueryData;
            wifi_auto_connect_get_ap_info(pstGetAutoConnApInfo->u8Index , pstGetAutoConnApInfo->pstAutoConnInfo);
            break;
        case BLEWFII_WIFI_GET_AUTO_CONN_LIST_NUM:
            wifi_auto_connect_get_saved_ap_num((uint8_t *)pu8QueryData);
            break;
        case BLEWFII_WIFI_GET_SCAN_LIST:
            p_scan_list = (wifi_scan_list_t*) pu8QueryData;
            wifi_scan_get_ap_list(p_scan_list);
        default:
            break;
    }
    return 0;
}

int BleWifi_Wifi_Set_Config(uint32_t u32SetType , void *pu8SetData)
{
    blewifi_wifi_set_config_source_t *pstSetConfigSource;

    switch(u32SetType)
    {
        case BLEWIFI_WIFI_SET_MAC_ADDR:
            wifi_config_set_mac_address(WIFI_MODE_STA, (uint8_t *)pu8SetData);
            break;
        case BLEWIFI_WIFI_SET_MANUFACTURER_NAME:
            _BleWifi_Wifi_SetManufName((uint8_t *)pu8SetData);
            break;
        case BLEWIFI_WIFI_SET_CONFIG_SOURCE:
            pstSetConfigSource = (blewifi_wifi_set_config_source_t *)pu8SetData;
            mac_addr_set_config_source(pstSetConfigSource->iface, pstSetConfigSource->type);
            break;
        case BLEWIFI_WIFI_SET_DTIM:
            _BleWifi_Wifi_SetDTIM((blewifi_wifi_set_dtim_t *)pu8SetData);
            break;
        default:
            break;
    }
    return 0;
}


void BleWifi_Wifi_Init(void)
{
    osSemaphoreDef_t tSemaphoreDef;
    osTimerDef_t timer_auto_connect_def;

    // create the semaphore . If need access g_tBwWifiFsmMsgQueueId , it must to lock.
    tSemaphoreDef.dummy = 0;                            // reserved, it is no used
    g_tWifiInternalSemaphoreId = osSemaphoreCreate(&tSemaphoreDef, 1);
    if (g_tWifiInternalSemaphoreId == NULL)
    {
        printf("wifi: create the semaphore fail \r\n");
    }

    /* WiFi FSM */
    BleWifi_Wifi_FSM_Init();

    /* create timer to trig auto connect */
    timer_auto_connect_def.ptimer = BwWifiFsm_AutoConnectTrigger;
    g_tAppCtrlAutoConnectTriggerTimer = osTimerCreate(&timer_auto_connect_def, osTimerOnce, NULL);
    if(g_tAppCtrlAutoConnectTriggerTimer == NULL)
    {
        BLEWIFI_ERROR("BLEWIFI: create auto-connection timer fail \r\n");
    }

    if (false == BleWifi_COM_EventCreate(&g_tWifiFsmEventGroup))
    {
        BLEWIFI_ERROR("BLEWIFI: ctrl task create wifi fsm event group fail \r\n");
    }

    //create wifi dtim event group
    if (false == BleWifi_COM_EventCreate(&g_tDtimEventGroup))
    {
        BLEWIFI_ERROR("create dtim event group fail \r\n");
    }

    osSemaphoreWait(g_tWifiInternalSemaphoreId, osWaitForever);

    BleWifi_Wifi_FSM_CmdPush_Without_Check(BW_WIFI_FSM_MSG_WIFI_REQ_INIT, NULL , 0 , BleWifi_Wifi_Init_Done_CB, BLEWIFI_QUEUE_BACK);

    BleWifi_Wifi_FSM_MsgSend(BW_WIFI_FSM_MSG_WIFI_EXEC_CMD, NULL, 0, NULL, BLEWIFI_QUEUE_BACK);

    osSemaphoreRelease(g_tWifiInternalSemaphoreId);

    /* Init the DTIM time (ms) */
    g_u32BwWifiDtimSetting = BLEWIFI_WIFI_DTIM_INTERVAL;
}

int32_t BleWifi_Wifi_Scan_Req(wifi_scan_config_t *scan_config)
{
    osSemaphoreWait(g_tWifiInternalSemaphoreId, osWaitForever);

    printf("BleWifi_Wifi_Scan_Req\n");
    BleWifi_Wifi_FSM_CmdPush_Without_Check(BW_WIFI_FSM_MSG_WIFI_REQ_SCAN, (uint8_t*)scan_config, sizeof(wifi_scan_config_t), BleWifi_Wifi_Scan_Done_CB, BLEWIFI_QUEUE_BACK);

    BleWifi_Wifi_FSM_MsgSend(BW_WIFI_FSM_MSG_WIFI_EXEC_CMD, NULL, 0, NULL, BLEWIFI_QUEUE_BACK);

    osSemaphoreRelease(g_tWifiInternalSemaphoreId);

    return 0;
}

int32_t BleWifi_Wifi_Start_Conn(wifi_conn_config_t *pstWifiConnConfig)
{
    uint8_t u8Reason = BW_WIFI_STOP_CONNECT;
    uint8_t u8OriUsed = false;

    osSemaphoreWait(g_tWifiInternalSemaphoreId, osWaitForever);

    u8OriUsed = BleWifi_COM_EventStatusGet(g_tWifiFsmEventGroup, BW_WIFI_FSM_EVENT_BIT_WIFI_USED);

    BleWifi_COM_EventStatusSet(g_tWifiFsmEventGroup, BW_WIFI_FSM_EVENT_BIT_WIFI_USED, true);

    g_u32TotalRetryTime = 0;
    g_u32MaxOnceFailTime = 0;
    g_u32ConnectionStartTime = osKernelSysTick();
    g_ulAppCtrlDoAutoConnectCumulativeTime = 0;

    if(0==pstWifiConnConfig->conn_timeout || pstWifiConnConfig == NULL)
    {
        g_u32WifiConnTimeout = WIFI_CONNECTION_TIMEOUT;
    }
    else
    {
        g_u32WifiConnTimeout = pstWifiConnConfig->conn_timeout;
    }

    printf("BleWifi_Wifi_Start_Conn = 0x%02x, %d\n", BW_WIFI_FSM_MSG_WIFI_REQ_CONNECT, g_u32WifiConnTimeout);

    if(NULL != pstWifiConnConfig)
    {
        g_BwWifiReqConnRetryTimes = 0;
        BleWifi_Wifi_FSM_CmdPush_Without_Check(BW_WIFI_FSM_MSG_WIFI_REQ_DISCONNECT, &u8Reason, sizeof(uint8_t), NULL , BLEWIFI_QUEUE_BACK);

        BleWifi_Wifi_FSM_CmdPush_Without_Check(BW_WIFI_FSM_MSG_WIFI_REQ_CONNECT, (uint8_t*)pstWifiConnConfig, sizeof(wifi_conn_config_t), NULL, BLEWIFI_QUEUE_BACK);
        BleWifi_Wifi_FSM_MsgSend(BW_WIFI_FSM_MSG_WIFI_EXEC_CMD, NULL, 0, NULL, BLEWIFI_QUEUE_BACK);
    }
    else
    {
        //Execute Auto conn here
        g_u32AppCtrlAutoConnectIntervalSelect = 0;

        // not first time
        if (u8OriUsed == true)
        {
            // reset timer
            if (true == BleWifi_COM_EventStatusGet(g_tWifiFsmEventGroup, BW_WIFI_FSM_EVENT_BIT_EXEC_AUTO_CONN))
            {
                osTimerStop(g_tAppCtrlAutoConnectTriggerTimer);
                BleWifi_COM_EventStatusSet(g_tWifiFsmEventGroup, BW_WIFI_FSM_EVENT_BIT_EXEC_AUTO_CONN, false);

                BleWifi_Wifi_FSM_CmdPush_Without_Check(BW_WIFI_FSM_MSG_WIFI_REQ_AUTO_CONNECT, NULL, 0, NULL, BLEWIFI_QUEUE_BACK);
                BleWifi_Wifi_FSM_MsgSend(BW_WIFI_FSM_MSG_WIFI_EXEC_CMD, NULL, 0, NULL, BLEWIFI_QUEUE_BACK);
            }
        }
        // the first time
        else
        {
            BleWifi_Wifi_FSM_CmdPush_Without_Check(BW_WIFI_FSM_MSG_WIFI_REQ_AUTO_CONNECT, NULL, 0, NULL, BLEWIFI_QUEUE_BACK);
            BleWifi_Wifi_FSM_MsgSend(BW_WIFI_FSM_MSG_WIFI_EXEC_CMD, NULL, 0, NULL, BLEWIFI_QUEUE_BACK);
        }
    }

    osSemaphoreRelease(g_tWifiInternalSemaphoreId);

    return 0;
}
int32_t BleWifi_Wifi_Stop_Conn(void)
{
    uint8_t u8Reason = BW_WIFI_STOP_CONNECT;

    printf("BleWifi_Wifi_Stop_Conn = 0x%02x\n", BW_WIFI_FSM_MSG_WIFI_REQ_DISCONNECT);

    osSemaphoreWait(g_tWifiInternalSemaphoreId, osWaitForever);

    BleWifi_COM_EventStatusSet(g_tWifiFsmEventGroup, BW_WIFI_FSM_EVENT_BIT_WIFI_USED, false);

    BwWifiFlushCMD(g_tBwWifiFsmCmdQueueId);

    BleWifi_Wifi_FSM_CmdPush_Without_Check(BW_WIFI_FSM_MSG_WIFI_REQ_DISCONNECT, &u8Reason, sizeof(uint8_t), BleWifi_Wifi_Req_Disconnected_CB, BLEWIFI_QUEUE_BACK);

    BleWifi_Wifi_FSM_CmdPush_Without_Check(BW_WIFI_FSM_MSG_WIFI_STOP, NULL , 0 , BleWifi_Wifi_Stop_CB, BLEWIFI_QUEUE_BACK);

    BleWifi_Wifi_FSM_MsgSend(BW_WIFI_FSM_MSG_WIFI_EXEC_CMD, NULL, 0, NULL, BLEWIFI_QUEUE_BACK);

    osSemaphoreRelease(g_tWifiInternalSemaphoreId);

    return 0;
}

int32_t BleWifi_Wifi_Reset_Req(void)
{
    int sRet = 0;

    osSemaphoreWait(g_tWifiInternalSemaphoreId, osWaitForever);

    SsidPwdClear();

    printf("BleWifi_Wifi_Reset_Req \n");

    sRet = wifi_auto_connect_reset();
    BleWifi_Ble_SendResponse(BLEWIFI_RSP_RESET, sRet);

    osSemaphoreRelease(g_tWifiInternalSemaphoreId);

    BleWifi_Wifi_Stop_Conn();

    return 0;
}

