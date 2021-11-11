/******************************************************************************
*  Copyright 2017 - 2020, Opulinks Technology Ltd.
*  ---------------------------------------------------------------------------
*  Statement:
*  ----------
*  This software is protected by Copyright and the information contained
*  herein is confidential. The software may not be copied and the information
*  contained herein may not be used or disclosed except with the written
*  permission of Opulinks Technology Ltd. (C) 2020
******************************************************************************/
#include "app_ctrl.h"
#include "blewifi_wifi_FSM.h"
#include "blewifi_wifi_api.h"
#include "blewifi_data.h"
#include "blewifi_common.h"
#include "ble_msg.h"
#include "wifi_api.h"
#include "lwip_helper.h"
#include "ssidpwd_proc.h"
#include "event_loop.h"
#include "cmsis_os.h"
#include "hal_os_patch.h"
#include "ssidpwd_proc.h"
#include "hal_wdt.h"
#include "blewifi_handle.h"
#include "app_configuration.h"
#include "hal_system.h"

osThreadId   g_tBwWifiFsmTaskId;
osMessageQId g_tBwWifiFsmMsgQueueId;
osMessageQId g_tBwWifiFsmCmdQueueId;


uint8_t g_u8CurrFSMState = BW_WIFI_FSM_STATE_INIT;
uint8_t *g_keepReqData = NULL;
uint8_t g_keepReqDataLen = 0;

//extern T_MwFim_GP11_Ssid_PWD g_taSsidPwd[MW_FIM_GP11_SSID_PWD_NUM];
//extern T_MwFim_GP11_Ssid_PWD g_taFixedSsidPwd;
extern EventGroupHandle_t g_tWifiFsmEventGroup;
extern osTimerId    g_tAppCtrlAutoConnectTriggerTimer;
extern uint32_t g_ulBleWifi_Wifi_BeaconTime;
uint32_t g_u32AppCtrlAutoConnectIntervalSelect = 0;
uint32_t g_BwWifiReqConnRetryTimes = 0;
uint8_t g_u8WifiDisReaCode = 0;

//////  Connection Retry handle  //////////
uint32_t g_u32TotalRetryTime = 0;
uint32_t g_u32MaxOnceFailTime = 0;
uint32_t g_u32ConnectionStartTime = 0;
uint32_t g_u32WifiConnTimeout=0;
uint32_t g_ulAppCtrlDoAutoConnectCumulativeTime = 0;

///////////////////////////////////////////
osTimerId    g_tAppCtrlDHCPTimer;

wifi_config_t g_wifi_config_req_connect = {0};

uint8_t g_iIdxOfApInfo; //Record Idx of AP for doing connection if connction failed
extern const T_MwFim_GP11_Ssid_PWD g_tMwFimDefaultGp11SsidPwd;
extern const T_MwFim_GP11_Ssid_PWD g_tMwFimDefaultGp11FixedSsidPwd;
extern uint32_t g_RoamingApInfoTotalCnt;
extern apinfo_t g_apInfo[MW_FIM_GP11_FIXED_SSID_PWD_NUM+MW_FIM_GP11_SSID_PWD_NUM];

uint8_t g_u8WifiCmdWaitingFlag = BLEWIFI_WIFI_EXEC_CMD_CONTINUE; //BLEWIFI_WIFI_EXEC_CMD_WAITING : Waiting , BLEWIFI_WIFI_EXEC_CMD_CONTINUE: execute

#ifndef CUS_AUTO_CONNECT_TABLE
#define AUTO_CONNECT_INTERVAL_TABLE_SIZE  (16) // The table size is equal to interval num
uint32_t g_u32AutoConnIntervalTable[AUTO_CONNECT_INTERVAL_TABLE_SIZE] =
{
    5000,
    6000,
    7000,
    8000,
    9000,
    10000,
    11000,
    12000,
    13000,
    14000,
    15000,
    16000,
    17000,
    18000,
    19000,
    20000
};
#endif

static int32_t BwWifiFsm_TaskEvtHandler_WifiCommand_Exec(uint32_t u32EventId, uint8_t *pu8Data, uint32_t u32DataLen);
static int32_t BwWifiFsm_TaskEvtHandler_DHCPTimeout(uint32_t u32EventId, uint8_t *pu8Data, uint32_t u32DataLen);
//static int32_t BwWifiFsm_TaskEvtHandler_WifiAutoConnectInd(uint32_t u32EventId, uint8_t *pu8Data, uint32_t u32DataLen);
static T_Bw_Wifi_FsmEvtHandlerTbl g_tBwWifiFsmEvtHandlerTbl[] =
{
    {BW_WIFI_FSM_MSG_WIFI_EXEC_CMD,                    BwWifiFsm_TaskEvtHandler_WifiCommand_Exec},
    {BW_WIFI_FSM_MSG_DHCP_TIMEOUT,                     BwWifiFsm_TaskEvtHandler_DHCPTimeout},
    {BW_WIFI_FSM_MSG_HANDLER_TBL_END,                  NULL}
};

static int32_t BwWifiFsm_TaskEvtHandler_WifiScanDoneInd(uint32_t u32EventId, uint8_t *pu8Data, uint32_t u32DataLen);
static int32_t BwWifiFsm_TaskEvtHandler_WifiConnectionSuccessInd(uint32_t u32EventId, uint8_t *pu8Data, uint32_t u32DataLen);
static int32_t BwWifiFsm_TaskEvtHandler_WifiConnectionFailInd(uint32_t u32EventId, uint8_t *pu8Data, uint32_t u32DataLen);
static int32_t BwWifiFsm_TaskEvtHandler_WifiDisconnectionInd(uint32_t u32EventId, uint8_t *pu8Data, uint32_t u32DataLen);
static int32_t BwWifiFsm_TaskEvtHandler_WifiGotIpInd(uint32_t u32EventId, uint8_t *pu8Data, uint32_t u32DataLen);
static int32_t BwWifiFsm_TaskEvtHandler_WifiInit_Req(uint32_t u32EventId, uint8_t *pu8Data, uint32_t u32DataLen);
static int32_t BwWifiFsm_TaskEvtHandler_Scan_Req(uint32_t u32EventId, uint8_t *pu8Data, uint32_t u32DataLen);
static int32_t BwWifiFsm_TaskEvtHandler_Connect_Req(uint32_t u32EventId, uint8_t *pu8Data, uint32_t u32DataLen);
static int32_t BwWifiFsm_TaskEvtHandler_Stop_Req(uint32_t u32EventId, uint8_t *pu8Data, uint32_t u32DataLen);
static int32_t BwWifiFsm_TaskEvtHandler_Auto_Connect_Start_Req(uint32_t u32EventId, uint8_t *pu8Data, uint32_t u32DataLen);
static int32_t BwWifiFsm_TaskEvtHandler_Auto_Connect_Scan_Req(uint32_t u32EventId, uint8_t *pu8Data, uint32_t u32DataLen);
static int32_t BwWifiFsm_TaskEvtHandler_Auto_Connect_Req(uint32_t u32EventId, uint8_t *pu8Data, uint32_t u32DataLen);
#ifdef TBD
static int32_t BwWifiFsm_TaskEvtHandler_WifiStatus_Req(uint32_t u32EventId, uint8_t *pu8Data, uint32_t u32DataLen);
#endif
//static int32_t BwWifiFsm_TaskEvtHandler_Reset_Req(uint32_t u32EventId, uint8_t *pu8Data, uint32_t u32DataLen);
static int32_t BwWifiFsm_TaskEvtHandler_Disconnect_Req(uint32_t u32EventId, uint8_t *pu8Data, uint32_t u32DataLen);
static int32_t BwWifiFsm_TaskEvtHandler_WifiInitComplete(uint32_t u32EventId, uint8_t *pu8Data, uint32_t u32DataLen);

static T_Bw_Wifi_FsmEvtHandlerTbl g_tBwWifiFsmEvtTbl_Init[] =
{
    {BW_WIFI_FSM_MSG_WIFI_REQ_INIT,                      BwWifiFsm_TaskEvtHandler_WifiInit_Req},

    {BW_WIFI_FSM_MSG_WIFI_INIT_COMPLETE,                 BwWifiFsm_TaskEvtHandler_WifiInitComplete},

    {BW_WIFI_FSM_MSG_HANDLER_TBL_END,                    NULL}
};

static T_Bw_Wifi_FsmEvtHandlerTbl g_tBwWifiFsmEvtTbl_Idle[] =
{
    {BW_WIFI_FSM_MSG_WIFI_REQ_SCAN,                      BwWifiFsm_TaskEvtHandler_Scan_Req},
    {BW_WIFI_FSM_MSG_WIFI_REQ_CONNECT,                   BwWifiFsm_TaskEvtHandler_Connect_Req},
    {BW_WIFI_FSM_MSG_WIFI_STOP,                          BwWifiFsm_TaskEvtHandler_Stop_Req},
    {BW_WIFI_FSM_MSG_WIFI_REQ_AUTO_CONNECT_START,        BwWifiFsm_TaskEvtHandler_Auto_Connect_Start_Req},
    {BW_WIFI_FSM_MSG_WIFI_REQ_AUTO_CONNECT_SCAN,         BwWifiFsm_TaskEvtHandler_Auto_Connect_Scan_Req},
    {BW_WIFI_FSM_MSG_WIFI_REQ_AUTO_CONNECT,              BwWifiFsm_TaskEvtHandler_Auto_Connect_Req},
    {BW_WIFI_FSM_MSG_WIFI_REQ_ROAMING_SCAN,              BwWifiFsm_TaskEvtHandler_Scan_Req},    //do the same thing with BW_WIFI_FSM_MSG_WIFI_REQ_SCAN
    {BW_WIFI_FSM_MSG_WIFI_REQ_ROAMING_CONNECT,           BwWifiFsm_TaskEvtHandler_Connect_Req}, //do the same thing with BW_WIFI_FSM_MSG_WIFI_REQ_CONNECT

    {BW_WIFI_FSM_MSG_WIFI_SCAN_DONE_IND,                 BwWifiFsm_TaskEvtHandler_WifiScanDoneInd},
    {BW_WIFI_FSM_MSG_WIFI_CONNECTION_SUCCESS_IND,        BwWifiFsm_TaskEvtHandler_WifiConnectionSuccessInd},
    {BW_WIFI_FSM_MSG_WIFI_CONNECTION_FAIL_IND,           BwWifiFsm_TaskEvtHandler_WifiConnectionFailInd},
    {BW_WIFI_FSM_MSG_WIFI_DISCONNECTION_IND,             BwWifiFsm_TaskEvtHandler_WifiDisconnectionInd},

    {BW_WIFI_FSM_MSG_HANDLER_TBL_END,                    NULL}

};

static T_Bw_Wifi_FsmEvtHandlerTbl g_tBwWifiFsmEvtTbl_Connected[] =
{
    {BW_WIFI_FSM_MSG_WIFI_REQ_SCAN,                      BwWifiFsm_TaskEvtHandler_Scan_Req},
    {BW_WIFI_FSM_MSG_WIFI_REQ_DISCONNECT,                BwWifiFsm_TaskEvtHandler_Disconnect_Req},
    {BW_WIFI_FSM_MSG_WIFI_STOP,                          BwWifiFsm_TaskEvtHandler_Stop_Req},

    {BW_WIFI_FSM_MSG_WIFI_SCAN_DONE_IND,                 BwWifiFsm_TaskEvtHandler_WifiScanDoneInd},
    {BW_WIFI_FSM_MSG_WIFI_DISCONNECTION_IND,             BwWifiFsm_TaskEvtHandler_WifiDisconnectionInd},
    {BW_WIFI_FSM_MSG_WIFI_GOT_IP_IND,                    BwWifiFsm_TaskEvtHandler_WifiGotIpInd},

    {BW_WIFI_FSM_MSG_HANDLER_TBL_END,                    NULL}
};

static T_Bw_Wifi_FsmEvtHandlerTbl g_tBwWifiFsmEvtTbl_Ready[] =
{
    {BW_WIFI_FSM_MSG_WIFI_REQ_SCAN,                      BwWifiFsm_TaskEvtHandler_Scan_Req},
    {BW_WIFI_FSM_MSG_WIFI_REQ_DISCONNECT,                BwWifiFsm_TaskEvtHandler_Disconnect_Req},
    {BW_WIFI_FSM_MSG_WIFI_STOP,                          BwWifiFsm_TaskEvtHandler_Stop_Req},

    {BW_WIFI_FSM_MSG_WIFI_SCAN_DONE_IND,                 BwWifiFsm_TaskEvtHandler_WifiScanDoneInd},
    {BW_WIFI_FSM_MSG_WIFI_DISCONNECTION_IND,             BwWifiFsm_TaskEvtHandler_WifiDisconnectionInd},

    {BW_WIFI_FSM_MSG_HANDLER_TBL_END,                    NULL}
};
int32_t  _BwWifiFSMCmdCopy(BleWifi_Wifi_FSM_CmdQ_t *pstSrc , BleWifi_Wifi_FSM_CmdQ_t *pstDst)
{
    if(pstSrc == NULL || pstDst == NULL)
    {
        printf("pointer is null\r\n");
        return false;
    }

    pstDst->u32EventId = pstSrc->u32EventId;
    pstDst->u32Length = pstSrc->u32Length;
    pstDst->fpCtrlAppCB = pstSrc->fpCtrlAppCB;
    if (pstDst->u32Length > 0)
    {
        memcpy(pstDst->u8aData, pstSrc->u8aData, pstSrc->u32Length);
    }
    return true;
}

int32_t _BwWifiClearQueue(osMessageQId tBwWifiCmdQueueId)
{
    osEvent rxEvent;

    while(1)
    {
        rxEvent = osMessageGet(tBwWifiCmdQueueId , 0); // pop from queue
        if (rxEvent.status == osEventMessage)
        {
            /*
            // for debug
            BleWifi_Wifi_FSM_CmdQ_t *pstTmpCmd = NULL;
            pstTmpCmd = (BleWifi_Wifi_FSM_CmdQ_t *)rxEvent.value.p;
            printf("u32EventId = 0x%x\r\n",pstTmpCmd->u32EventId);
            printf("u32Length = %u\r\n",pstTmpCmd->u32Length);
            printf("fpCtrlAppCB = 0x%x\r\n",pstTmpCmd->fpCtrlAppCB);
            for(int i = 0 ; i < pstTmpCmd->u32Length ; i++)
            {
                printf("u8aData[%u] = 0x%x\r\n", i ,pstTmpCmd->u8aData[i]);
            }
            */
            free(rxEvent.value.p);  //free the data of queue
        }
        else
        {
            break;
        }
    }
    return 0;
}

int32_t BwWifiFlushCMD(osMessageQId tBwWifiCmdQueueId)
{
    osEvent rxEvent;
    BleWifi_Wifi_FSM_CmdQ_t *pstPeekCmd = NULL;
    BleWifi_Wifi_FSM_CmdQ_t *pstTmpCmd = NULL;

    if(g_u8WifiCmdWaitingFlag == BLEWIFI_WIFI_EXEC_CMD_WAITING)    //keep executing cmd.
    {
        rxEvent = osMessagePeek(tBwWifiCmdQueueId , 0);

        if(rxEvent.status == osEventMessage) //Queue has least one CMD
        {
            pstPeekCmd = (BleWifi_Wifi_FSM_CmdQ_t *)rxEvent.value.p;
            if(pstPeekCmd == NULL)
            {
                printf("Error BwWifiFlushCMD cmd is null!\r\n");
                goto done;
            }
            else
            {
                /* Mem allocate */
                pstTmpCmd = malloc(sizeof(BleWifi_Wifi_FSM_CmdQ_t) + pstPeekCmd->u32Length);
                if (pstTmpCmd == NULL)
                {
                    printf("pstTmpCmd allocate fail\r\n");
                    goto done;
                }
                //copy pstPeekCmd to  pstTmpCmd , because pstPeekCmd will free
                pstTmpCmd->u32EventId = pstPeekCmd->u32EventId;
                pstTmpCmd->u32Length = pstPeekCmd->u32Length;
                pstTmpCmd->fpCtrlAppCB = pstPeekCmd->fpCtrlAppCB;
                if (pstTmpCmd->u32Length > 0)
                {
                    memcpy(pstTmpCmd->u8aData, pstPeekCmd->u8aData, pstPeekCmd->u32Length);
                }

                // clear queue
                _BwWifiClearQueue(tBwWifiCmdQueueId);

                //restore the waiting cmd into queue
                BleWifi_Wifi_FSM_CmdPush_Without_Check(pstTmpCmd->u32EventId , pstTmpCmd->u8aData , pstTmpCmd->u32Length , pstTmpCmd->fpCtrlAppCB , BLEWIFI_QUEUE_BACK);

                //free the temp cmd
                free(pstTmpCmd);
            }
        }
        else // error case , if g_u8WifiCmdWaitingFlag is waiting
        {
            printf("Error BwWifiFlushCMD no cmd!\r\n");
            goto done;
        }
    }
    else
    {
        _BwWifiClearQueue(tBwWifiCmdQueueId);
    }

done:

    return 0;
}

static int32_t BleWifi_Wifi_IsQueueEmpty(osMessageQId tBwWifiCmdQueueId)
{
    osEvent rxEvent;

    // check queue is empty or not
    rxEvent = osMessagePeek(tBwWifiCmdQueueId , 0);

    if(rxEvent.status == osEventMessage) // queue is not empty
    {
        return false;
    }
    else // queue is empty
    {
        return true;
    }
}

static void _BwWifiFsm_Remove_Pair_CMD(uint16_t u16CheckReq)
{
    osEvent rxEvent;
    BleWifi_Wifi_FSM_CmdQ_t *pstQueryCmd;

    rxEvent = osMessagePeek(g_tBwWifiFsmCmdQueueId , 0);

    if(rxEvent.status == osEventMessage)
    {
        pstQueryCmd = (BleWifi_Wifi_FSM_CmdQ_t *)rxEvent.value.p;
        if((pstQueryCmd->u32EventId == u16CheckReq) && (g_u8WifiCmdWaitingFlag ==BLEWIFI_WIFI_EXEC_CMD_WAITING))
        {
            //execution CB
            if(pstQueryCmd->fpCtrlAppCB != NULL)
            {
                pstQueryCmd->fpCtrlAppCB();
            }

            memset(&rxEvent , 0 , sizeof(osEvent));
            rxEvent = osMessageGet(g_tBwWifiFsmCmdQueueId , 0); // pop from queue

            free(rxEvent.value.p);

            g_u8WifiCmdWaitingFlag = BLEWIFI_WIFI_EXEC_CMD_CONTINUE;
            if(BleWifi_Wifi_IsQueueEmpty(g_tBwWifiFsmCmdQueueId) == false)  // cmdq is not empty
            {
                BleWifi_Wifi_FSM_MsgSend(BW_WIFI_FSM_MSG_WIFI_EXEC_CMD, NULL, 0, NULL, BLEWIFI_QUEUE_BACK);
            }
        }
    }
}

static void _BwWifiFsm_DoAutoConnect_Retry()
{
    /*if(g_ulAppCtrlDoAutoConnectCumulativeTime >= BLEWIFI_WIFI_AUTO_CONNECT_LIFETIME_MAX)
    {
        g_ulAppCtrlDoAutoConnectCumulativeTime = 0;
        BleWifi_Wifi_Disconnected_CB(g_u8WifiDisReaCode);
        return;
    }*/

    //do auto connect again
    osTimerStop(g_tAppCtrlAutoConnectTriggerTimer);
    osTimerStart(g_tAppCtrlAutoConnectTriggerTimer, g_u32AutoConnIntervalTable[g_u32AppCtrlAutoConnectIntervalSelect]);
    BleWifi_COM_EventStatusSet(g_tWifiFsmEventGroup, BW_WIFI_FSM_EVENT_BIT_EXEC_AUTO_CONN, true);

    printf("Auto connect interval = %u\r\n" , g_u32AutoConnIntervalTable[g_u32AppCtrlAutoConnectIntervalSelect]);

    if(g_u32AppCtrlAutoConnectIntervalSelect < (AUTO_CONNECT_INTERVAL_TABLE_SIZE - 1))
    {
        g_u32AppCtrlAutoConnectIntervalSelect++;
    }
}

void BwWifiFsm_AutoConnectTrigger(void const *argu)
{
    BleWifi_COM_EventStatusSet(g_tWifiFsmEventGroup, BW_WIFI_FSM_EVENT_BIT_EXEC_AUTO_CONN, false);

    BleWifi_Wifi_FSM_CmdPush(BW_WIFI_FSM_MSG_WIFI_REQ_AUTO_CONNECT , NULL , sizeof(uint8_t), NULL, BLEWIFI_QUEUE_BACK);
    BleWifi_Wifi_FSM_MsgSend(BW_WIFI_FSM_MSG_WIFI_EXEC_CMD, NULL, 0, NULL, BLEWIFI_QUEUE_BACK);
}

int BleWifi_Wifi_Get_BSsid(apinfo_t *tApInfo)
{
    wifi_scan_info_t *ap_list = NULL;
    uint16_t apCount = 0;
    int8_t ubAppErr = -1;
    int32_t i = 0;
    int32_t idx = 0;
    int32_t MaxRssiIdx=0;
    blewifi_wifi_get_ap_record_t stAPRecord;

    //printf("[Kevin]A\n\n");
    BleWifi_Wifi_Query_Status(BLEWIFI_WIFI_GET_AP_NUM , &apCount);
    //wifi_scan_get_ap_num(&apCount);
    //printf("[Kevin]B\n\n");
    if(apCount == 0)
    {
        printf("No AP found\r\n");
        goto err;
    }
    BLEWIFI_ERROR("ap num = %d\n", apCount);
    ap_list = (wifi_scan_info_t *)malloc(sizeof(wifi_scan_info_t) * apCount);

    if (!ap_list) {
        printf("Get_BSsid: malloc fail\n");
        goto err;
    }

    stAPRecord.pu16apCount = &apCount;
    stAPRecord.pstScanInfo = ap_list;
    BleWifi_Wifi_Query_Status(BLEWIFI_WIFI_GET_AP_RECORD , (void *)&stAPRecord);
    //wifi_scan_get_ap_records(&apCount, ap_list);
    //printf("[Kevin]C\n\n");
    /* build blewifi ap list */
    for(i = 0; i < apCount; i++)
    {
        if(0 == memcmp(ap_list[i].ssid, tApInfo->tSsidPwd.ssid, sizeof(ap_list[i].ssid)))
        {
            //printf("[Kevin]D, index=%d\n\n", i);
            ubAppErr = 0;
            MaxRssiIdx = i;
            for(idx = i; idx < apCount; idx++)
            {
                //if there are 2 or more same ssid, find max rssi to connect.
                if(!memcmp(ap_list[idx].ssid, tApInfo->tSsidPwd.ssid, sizeof(ap_list[idx].ssid)))
                {
                    //printf("[Kevin]E\n\n");
                    if( ap_list[idx].rssi > ap_list[i].rssi )
                    {
                        MaxRssiIdx = idx;
                    }
                }
            }
            break;
        }
    }

    if( 0 == ubAppErr )
    {
        memcpy(tApInfo->bssid, ap_list[MaxRssiIdx].bssid, 6);
    }


err:
    if (ap_list)
        free(ap_list);

    return ubAppErr;

}

static int32_t BwWifiFsm_TaskEvtHandler_WifiInitComplete(uint32_t u32EventId, uint8_t *pu8Data, uint32_t u32DataLen)
{
//    printf("BLEWIFI: MSG BLEWIFI_CTRL_MSG_WIFI_INIT_COMPLETE \r\n");
    printf("[ATS]WIFI init complete\r\n");
    g_u8CurrFSMState = BW_WIFI_FSM_STATE_IDLE;

    _BwWifiFsm_Remove_Pair_CMD(BW_WIFI_FSM_MSG_WIFI_REQ_INIT);

    return BW_WIFI_CMD_FINISH;
}

static int32_t BwWifiFsm_TaskEvtHandler_WifiScanDoneInd(uint32_t u32EventId, uint8_t *pu8Data, uint32_t u32DataLen)
{
    uint8_t u8IsUpdate;
    osEvent rxEvent;
    BleWifi_Wifi_FSM_CmdQ_t *pstPeekCmd = NULL;
    BleWifi_Wifi_FSM_CmdQ_t *pstQueryCmd = NULL;
    int ret;
    int iIdx=0;
    wifi_conn_config_t stConnConfig = {0};

//    printf("BLEWIFI: MSG BLEWIFI_CTRL_MSG_WIFI_SCAN_DONE_IND \r\n");
    printf("[ATS]WIFI scan done\r\n");
    rxEvent = osMessagePeek(g_tBwWifiFsmCmdQueueId , 0);

    BleWifi_Wifi_UpdateScanInfoToAutoConnList(&u8IsUpdate);

    if(rxEvent.status == osEventMessage)
    {
        pstPeekCmd = (BleWifi_Wifi_FSM_CmdQ_t *)rxEvent.value.p;

        pstQueryCmd = malloc(sizeof(BleWifi_Wifi_FSM_CmdQ_t) + pstPeekCmd->u32Length);
        if (pstQueryCmd == NULL)
        {
            printf("pstQueryCmd allocate fail\r\n");
            goto done;
        }

        //copy pstPeekCmd to pstQueryCmd because pstPeekCmd will free soon
        _BwWifiFSMCmdCopy(pstPeekCmd , pstQueryCmd);

        if(pstQueryCmd->u32EventId == BW_WIFI_FSM_MSG_WIFI_REQ_AUTO_CONNECT_SCAN)
        {
            _BwWifiFsm_Remove_Pair_CMD(BW_WIFI_FSM_MSG_WIFI_REQ_AUTO_CONNECT_SCAN);

            BleWifi_Wifi_FSM_CmdPush(BW_WIFI_FSM_MSG_WIFI_REQ_AUTO_CONNECT, NULL , 0, NULL, BLEWIFI_QUEUE_FRONT);
            BleWifi_Wifi_FSM_MsgSend(BW_WIFI_FSM_MSG_WIFI_EXEC_CMD, NULL, 0, NULL, BLEWIFI_QUEUE_BACK);
        }

        if(pstQueryCmd->u32EventId == BW_WIFI_FSM_MSG_WIFI_REQ_ROAMING_SCAN)
        {
            _BwWifiFsm_Remove_Pair_CMD(BW_WIFI_FSM_MSG_WIFI_REQ_ROAMING_SCAN);

            for( iIdx = g_iIdxOfApInfo; iIdx < (MW_FIM_GP11_FIXED_SSID_PWD_NUM+MW_FIM_GP11_SSID_PWD_NUM); iIdx++)
            {
                if (g_apInfo[iIdx].tSsidPwd.used == 0)
                {
                    continue;
                }

                 ret = BleWifi_Wifi_Get_BSsid(&(g_apInfo[iIdx]));

                if( 0 == ret )
                {
                    printf("\nFound Roaming APInfo[%d]=%s\n\n", iIdx, g_apInfo[iIdx].tSsidPwd.ssid);

                    g_iIdxOfApInfo = iIdx;

                    memcpy(stConnConfig.bssid, g_apInfo[iIdx].bssid , WIFI_MAC_ADDRESS_LENGTH);

                    stConnConfig.connected = 0;
                    stConnConfig.password_length = strlen((char *)g_apInfo[iIdx].tSsidPwd.password);
                    if(stConnConfig.password_length != 0)
                    {
                        if(stConnConfig.password_length > WIFI_LENGTH_PASSPHRASE)
                        {
                            stConnConfig.password_length = WIFI_LENGTH_PASSPHRASE;
                        }
                        memcpy((char *)stConnConfig.password, g_apInfo[iIdx].tSsidPwd.password , stConnConfig.password_length);
                    }

                    BleWifi_Wifi_FSM_CmdPush(BW_WIFI_FSM_MSG_WIFI_REQ_ROAMING_CONNECT,(uint8_t *)&stConnConfig , sizeof(wifi_conn_config_t), NULL, BLEWIFI_QUEUE_FRONT);
                    BleWifi_Wifi_FSM_MsgSend(BW_WIFI_FSM_MSG_WIFI_EXEC_CMD, NULL, 0, NULL, BLEWIFI_QUEUE_BACK);
                }
                else
                {
                    printf("\nNot found Roaming APInfo[%d]=%s\n\n", iIdx, g_apInfo[iIdx].tSsidPwd.ssid);
                }
            }

            if (iIdx == (MW_FIM_GP11_FIXED_SSID_PWD_NUM+MW_FIM_GP11_SSID_PWD_NUM))
            {
                printf("\nNo Roaming AP in Scan list!\n\n");

                _BwWifiFsm_DoAutoConnect_Retry();
            }
        }
        else if(pstQueryCmd->u32EventId == BW_WIFI_FSM_MSG_WIFI_REQ_SCAN)
        {
            _BwWifiFsm_Remove_Pair_CMD(BW_WIFI_FSM_MSG_WIFI_REQ_SCAN);
        }
        else
        {
            tracer_drct_printf("[ERROR]Cmdq has no scan req\r\n");
            osDelay(3000);
            Hal_Sys_SwResetAll();
        }
    }
    else
    {
        tracer_drct_printf("[ERROR]Cmdq has no scan req\r\n");
        osDelay(3000);
        Hal_Sys_SwResetAll();
    }

done:
    if (pstQueryCmd != NULL)
    {
        free(pstQueryCmd);
    }

    return BW_WIFI_CMD_FINISH;
}

static int32_t BwWifiFsm_TaskEvtHandler_WifiConnectionSuccessInd(uint32_t u32EventId, uint8_t *pu8Data, uint32_t u32DataLen)
{
    osEvent rxEvent;
    BleWifi_Wifi_FSM_CmdQ_t *pstPeekCmd = NULL;
    BleWifi_Wifi_FSM_CmdQ_t *pstQueryCmd = NULL;

    uint32_t u32ConnectionOnceTime = 0;
    uint32_t u32CurrentTime = 0;

    //save ssid and password to FIM
    wifi_scan_info_t tInfo = {0};

    rxEvent = osMessagePeek(g_tBwWifiFsmCmdQueueId , 0);

    if(rxEvent.status == osEventMessage)
    {
        pstPeekCmd = (BleWifi_Wifi_FSM_CmdQ_t *)rxEvent.value.p;

        pstQueryCmd = malloc(sizeof(BleWifi_Wifi_FSM_CmdQ_t) + pstPeekCmd->u32Length);
        if (pstQueryCmd == NULL)
        {
            printf("pstQueryCmd allocate fail\r\n");
            goto done;
        }

        //copy pstPeekCmd to pstQueryCmd because pstPeekCmd will free soon
        _BwWifiFSMCmdCopy(pstPeekCmd , pstQueryCmd);

        if((pstQueryCmd->u32EventId == BW_WIFI_FSM_MSG_WIFI_REQ_CONNECT) && (g_u8WifiCmdWaitingFlag ==BLEWIFI_WIFI_EXEC_CMD_WAITING))
        {
            BleWifi_Wifi_Query_Status(BLEWIFI_WIFI_QUERY_AP_INFO , &tInfo);
            //wifi_sta_get_ap_info(&tInfo);

            //printf("Save AP info:SSID:%s, PWD:%s\n", tInfo.ssid, wifi_config_req_connect.sta_config.password);
            T_MwFim_GP11_Ssid_PWD tNewSsidPwd;

            memset((void *)&tNewSsidPwd, 0, sizeof(T_MwFim_GP11_Ssid_PWD));
            memcpy((void *)tNewSsidPwd.ssid, (void *)tInfo.ssid, strlen((char *)tInfo.ssid));
            memcpy((void *)tNewSsidPwd.password, (void *)g_wifi_config_req_connect.sta_config.password, sizeof(g_wifi_config_req_connect.sta_config.password));

            SsidPwdAdd(tNewSsidPwd);
        }
    }
//    printf("BLEWIFI: MSG BLEWIFI_CTRL_MSG_WIFI_CONNECTION_SUCCESS_IND \r\n");
    printf("[ATS]WIFI connection success\r\n");
    if(g_BwWifiReqConnRetryTimes == 0)
    {
        u32CurrentTime = osKernelSysTick();
        if(u32CurrentTime >= g_u32ConnectionStartTime)
        {
            u32ConnectionOnceTime = u32CurrentTime - g_u32ConnectionStartTime;
        }
        else // overflow
        {
            u32ConnectionOnceTime = 0xFFFFFFFF - g_u32ConnectionStartTime + u32CurrentTime + 1;
        }

        g_u32TotalRetryTime = g_u32TotalRetryTime + u32ConnectionOnceTime;
        BleWifi_COM_EventStatusSet(g_tWifiFsmEventGroup, BW_WIFI_FSM_EVENT_BIT_DHCP_TIMEOUT, false);

        osTimerStop(g_tAppCtrlDHCPTimer);
        if(g_u32TotalRetryTime >= g_u32WifiConnTimeout)
        {
            osTimerStart(g_tAppCtrlDHCPTimer, 1);
        }
        else
        {
            osTimerStart(g_tAppCtrlDHCPTimer, g_u32WifiConnTimeout - g_u32TotalRetryTime);
        }
    }

    //reset the auto connect interval
    g_u32AppCtrlAutoConnectIntervalSelect = 0;

    //stop retry connect
    g_BwWifiReqConnRetryTimes = BLEWIFI_WIFI_REQ_CONNECT_RETRY_TIMES;

    g_u32TotalRetryTime = 0;
    g_u32MaxOnceFailTime = 0;
    g_u32ConnectionStartTime = 0;

    g_u8CurrFSMState = BW_WIFI_FSM_STATE_CONNECTED;
    lwip_net_start(WIFI_MODE_STA);

    _BwWifiFsm_Remove_Pair_CMD(BW_WIFI_FSM_MSG_WIFI_REQ_CONNECT);
    _BwWifiFsm_Remove_Pair_CMD(BW_WIFI_FSM_MSG_WIFI_REQ_AUTO_CONNECT);
    _BwWifiFsm_Remove_Pair_CMD(BW_WIFI_FSM_MSG_WIFI_REQ_ROAMING_CONNECT);

done:
    if (pstQueryCmd != NULL)
    {
        free(pstQueryCmd);
    }

    return BW_WIFI_CMD_FINISH;
}

void _BwWifiFsm_WifidisconnectCheck(uint8_t u8Reason)
{
    osEvent rxEvent;
    BleWifi_Wifi_FSM_CmdQ_t *pstPeekCmd = NULL;
    BleWifi_Wifi_FSM_CmdQ_t *pstQueryCmd = NULL;
    wifi_conn_config_t stWifiConnConfig = {0};
    wifi_scan_config_t stScanConfig={0};

    uint32_t u32ConnectionOnceTime = 0;
    uint32_t u32CurrentTime = 0;

    g_u8WifiDisReaCode = u8Reason;

    // !! prevent sysyem always busy , watch dog can't exec;
    Hal_Wdt_Clear();

    rxEvent = osMessagePeek(g_tBwWifiFsmCmdQueueId , 0);

    if(g_BwWifiReqConnRetryTimes == 0)
    {
        u32CurrentTime = osKernelSysTick();
        if(u32CurrentTime >= g_u32ConnectionStartTime)
        {
            u32ConnectionOnceTime = u32CurrentTime - g_u32ConnectionStartTime;
        }
        else // overflow
        {
            u32ConnectionOnceTime = 0xFFFFFFFF - g_u32ConnectionStartTime + u32CurrentTime + 1;
        }
        g_u32ConnectionStartTime = u32CurrentTime;

        g_u32TotalRetryTime = g_u32TotalRetryTime + u32ConnectionOnceTime;
        if(u32ConnectionOnceTime > g_u32MaxOnceFailTime)
        {
            g_u32MaxOnceFailTime = u32ConnectionOnceTime;
        }
        if(g_u32TotalRetryTime + g_u32MaxOnceFailTime > g_u32WifiConnTimeout) // remaining time < once exec time
        {
            g_BwWifiReqConnRetryTimes = BLEWIFI_WIFI_REQ_CONNECT_RETRY_TIMES;
        }
    }

    if(rxEvent.status == osEventMessage)
    {
        pstPeekCmd = (BleWifi_Wifi_FSM_CmdQ_t *)rxEvent.value.p;

        pstQueryCmd = malloc(sizeof(BleWifi_Wifi_FSM_CmdQ_t) + pstPeekCmd->u32Length);
        if (pstQueryCmd == NULL)
        {
            printf("pstQueryCmd allocate fail\r\n");
            goto done;
        }

        //copy pstPeekCmd to pstQueryCmd because pstPeekCmd will free soon
        _BwWifiFSMCmdCopy(pstPeekCmd , pstQueryCmd);

        if((pstQueryCmd->u32EventId == BW_WIFI_FSM_MSG_WIFI_REQ_AUTO_CONNECT) && (g_u8WifiCmdWaitingFlag ==BLEWIFI_WIFI_EXEC_CMD_WAITING))
        {
            _BwWifiFsm_Remove_Pair_CMD(BW_WIFI_FSM_MSG_WIFI_REQ_AUTO_CONNECT);
            if (0 != g_RoamingApInfoTotalCnt)
            {
                g_iIdxOfApInfo = 0; //initialize index for roaming connection


                BLEWIFI_INFO("Do roaming scan \r\n");

                memset(&stScanConfig , 0 ,sizeof(stScanConfig));
                stScanConfig.show_hidden = 1;
                stScanConfig.scan_type = WIFI_SCAN_TYPE_MIX;

                BleWifi_Wifi_FSM_CmdPush(BW_WIFI_FSM_MSG_WIFI_REQ_ROAMING_SCAN, (uint8_t *)&stScanConfig , sizeof(wifi_scan_config_t), NULL, BLEWIFI_QUEUE_FRONT);
                BleWifi_Wifi_FSM_MsgSend(BW_WIFI_FSM_MSG_WIFI_EXEC_CMD, NULL, 0, NULL, BLEWIFI_QUEUE_BACK);
            }
            else
            {
                _BwWifiFsm_DoAutoConnect_Retry();
            }
        }
        else if((pstQueryCmd->u32EventId == BW_WIFI_FSM_MSG_WIFI_REQ_ROAMING_CONNECT) && (g_u8WifiCmdWaitingFlag ==BLEWIFI_WIFI_EXEC_CMD_WAITING))
        {
            _BwWifiFsm_Remove_Pair_CMD(BW_WIFI_FSM_MSG_WIFI_REQ_ROAMING_CONNECT);

            _BwWifiFsm_DoAutoConnect_Retry();
        }
        else if((pstQueryCmd->u32EventId == BW_WIFI_FSM_MSG_WIFI_REQ_CONNECT) && (g_u8WifiCmdWaitingFlag ==BLEWIFI_WIFI_EXEC_CMD_WAITING))
        {
            _BwWifiFsm_Remove_Pair_CMD(BW_WIFI_FSM_MSG_WIFI_REQ_CONNECT);
            if (g_BwWifiReqConnRetryTimes < BLEWIFI_WIFI_REQ_CONNECT_RETRY_TIMES)
            {
                memcpy(&stWifiConnConfig , pstQueryCmd->u8aData , sizeof(stWifiConnConfig));
//                g_BwWifiReqConnRetryTimes++;
                printf("g_BwWifiReqConnRetryTimes = %u\r\n",g_BwWifiReqConnRetryTimes);
                BleWifi_Wifi_FSM_CmdPush(BW_WIFI_FSM_MSG_WIFI_REQ_CONNECT, (uint8_t*)&stWifiConnConfig, sizeof(wifi_conn_config_t), NULL, BLEWIFI_QUEUE_FRONT);
                BleWifi_Wifi_FSM_MsgSend(BW_WIFI_FSM_MSG_WIFI_EXEC_CMD, NULL, 0, NULL, BLEWIFI_QUEUE_BACK);
            }
            else
            {
                BleWifi_Wifi_Disconnected_CB(u8Reason);
                //BleWifi_Ble_SendResponse(BLEWIFI_RSP_CONNECT, BLEWIFI_WIFI_CONNECTED_FAIL);
            }
        }
        else if(pstQueryCmd->u32EventId == BW_WIFI_FSM_MSG_WIFI_REQ_DISCONNECT)
        {
            _BwWifiFsm_Remove_Pair_CMD(BW_WIFI_FSM_MSG_WIFI_REQ_DISCONNECT);
        }
        else
        {
            BleWifi_Wifi_Disconnected_CB(u8Reason);
            BleWifi_Wifi_FSM_CmdPush(BW_WIFI_FSM_MSG_WIFI_REQ_AUTO_CONNECT, NULL, 0, NULL, BLEWIFI_QUEUE_BACK);
            BleWifi_Wifi_FSM_MsgSend(BW_WIFI_FSM_MSG_WIFI_EXEC_CMD, NULL, 0, NULL, BLEWIFI_QUEUE_BACK);
        }
    }
    else
    {
        BleWifi_Wifi_Disconnected_CB(u8Reason);
        BleWifi_Wifi_FSM_CmdPush(BW_WIFI_FSM_MSG_WIFI_REQ_AUTO_CONNECT, NULL, 0, NULL, BLEWIFI_QUEUE_BACK);
        BleWifi_Wifi_FSM_MsgSend(BW_WIFI_FSM_MSG_WIFI_EXEC_CMD, NULL, 0, NULL, BLEWIFI_QUEUE_BACK);
    }

done:
    if (pstQueryCmd != NULL)
    {
        free(pstQueryCmd);
    }
}

static int32_t BwWifiFsm_TaskEvtHandler_WifiConnectionFailInd(uint32_t u32EventId, uint8_t *pu8Data, uint32_t u32DataLen)
{
    uint8_t u8Reason = *pu8Data;

    g_u8CurrFSMState = BW_WIFI_FSM_STATE_IDLE;

//    printf("BLEWIFI: MSG BLEWIFI_CTRL_MSG_WIFI_CONNECTION_FAIL_IND \r\n");
    printf("[ATS]WIFI connection fail\r\n");

    _BwWifiFsm_WifidisconnectCheck(u8Reason);

    return BW_WIFI_CMD_FINISH;
}

static int32_t BwWifiFsm_TaskEvtHandler_WifiDisconnectionInd(uint32_t u32EventId, uint8_t *pu8Data, uint32_t u32DataLen)
{
    uint8_t u8Reason = *pu8Data;

    BleWifi_COM_EventStatusSet(g_tWifiFsmEventGroup, BW_WIFI_FSM_EVENT_BIT_DHCP_TIMEOUT, false);
//    printf("BLEWIFI: MSG BLEWIFI_CTRL_MSG_WIFI_DISCONNECTION_IND \r\n");
    printf("[ATS]WIFI disconnection\r\n");

    if(255 == u8Reason)
    {
        printf("M3 CMD timeout\r\n");
        return BW_WIFI_CMD_M3_TIMEOUT;
    }

    _BwWifiFsm_WifidisconnectCheck(u8Reason);

    g_u8CurrFSMState = BW_WIFI_FSM_STATE_IDLE;

    return BW_WIFI_CMD_FINISH;
}

static int32_t BwWifiFsm_TaskEvtHandler_WifiGotIpInd(uint32_t u32EventId, uint8_t *pu8Data, uint32_t u32DataLen)
{
    BLEWIFI_INFO("BLEWIFI: MSG BLEWIFI_CTRL_MSG_WIFI_GOT_IP_IND \r\n");
#ifdef __BLEWIFI_TRANSPARENT__
    msg_print_uart1("WIFI GOT IP\n");
#endif

    osTimerStop(g_tAppCtrlDHCPTimer);

    if(BleWifi_COM_EventStatusGet(g_tWifiFsmEventGroup, BW_WIFI_FSM_EVENT_BIT_DHCP_TIMEOUT) == true)
    {
        BleWifi_COM_EventStatusSet(g_tWifiFsmEventGroup, BW_WIFI_FSM_EVENT_BIT_DHCP_TIMEOUT, false);
        return BW_WIFI_CMD_FINISH;
    }

    printf("[ATS]WIFI GOT IP\r\n");
    BLEWIFI_INFO("BLEWIFI: MSG BLEWIFI_CTRL_MSG_WIFI_GOT_IP_IND \r\n");

    printf("The FSM state is WiFi Ready State\n");

    g_u8CurrFSMState = BW_WIFI_FSM_STATE_READY;

    BleWifi_Wifi_Connected_CB();

    return BW_WIFI_CMD_FINISH;
}

static T_Bw_Wifi_FsmEvtHandlerTbl *g_tBwWifiFSMStateTbl[] =
{
    {g_tBwWifiFsmEvtTbl_Init},
    {g_tBwWifiFsmEvtTbl_Idle},
    {g_tBwWifiFsmEvtTbl_Connected},
    {g_tBwWifiFsmEvtTbl_Ready}
};

static int32_t BwWifiFsm_TaskEvtHandler_WifiInit_Req(uint32_t u32EventId, uint8_t *pu8Data, uint32_t u32DataLen)
{
    printf("Recv BLEWIFI_REQ_Init \r\n");

    //get SSID
    SsidPwdInit();

    /* Event Loop Initialization */
    wifi_event_loop_init((wifi_event_cb_t)BleWifi_Wifi_EventHandlerCb);

    /* Initialize wifi stack and register wifi init complete event handler */
    wifi_init(NULL, NULL);

    /* Wi-Fi operation start */
    wifi_start();

    //wifi_auto_connect_set_ap_num(1);

    /* Init the beacon time (ms) */
    g_ulBleWifi_Wifi_BeaconTime = 100;

    return BW_WIFI_CMD_EXECUTING;
}

static int32_t BwWifiFsm_TaskEvtHandler_Scan_Req(uint32_t u32EventId, uint8_t *pu8Data, uint32_t u32DataLen)
{
    int sRet;
    wifi_scan_config_t *pstScanConfig = (wifi_scan_config_t*)pu8Data;
    osEvent rxEvent;
    BleWifi_Wifi_FSM_CmdQ_t *pstQueryCmd;
    wifi_scan_config_t stScanConfig={0};

    printf("APPWIFI: Recv BLEWIFI_REQ_SCAN \r\n");

    /////////// for debug    ///////////////////////////
    if(pstScanConfig->ssid  != NULL)
    {
        printf("scan_config.ssid = %s\n", pstScanConfig->ssid);
    }

    printf("scan_config.channel = %d\n", pstScanConfig->channel);
    printf("scan_config.show_hidden = %d\n", pstScanConfig->show_hidden);
    printf("scan_config.scan_type = %d\n", pstScanConfig->scan_type);
    ////////////////////////////////////////////////////

    sRet = wifi_scan_start(pstScanConfig, NULL);
    if(0 != sRet)
    {
        printf("exec scan req failed\r\n");

        rxEvent = osMessagePeek(g_tBwWifiFsmCmdQueueId , 0);
        if(rxEvent.status == osEventMessage)
        {
            pstQueryCmd = (BleWifi_Wifi_FSM_CmdQ_t *)rxEvent.value.p;
            if(pstQueryCmd->u32EventId == BW_WIFI_FSM_MSG_WIFI_REQ_AUTO_CONNECT_SCAN)
            {
                if (0 != g_RoamingApInfoTotalCnt)
                {
                    g_iIdxOfApInfo = 0; //initialize index for roaming connection

                    memset(&stScanConfig , 0 ,sizeof(stScanConfig));
                    stScanConfig.show_hidden = 1;
                    stScanConfig.scan_type = WIFI_SCAN_TYPE_MIX;

                    BleWifi_Wifi_FSM_CmdPush(BW_WIFI_FSM_MSG_WIFI_REQ_ROAMING_SCAN, (uint8_t *)&stScanConfig , sizeof(wifi_scan_config_t) , NULL, BLEWIFI_QUEUE_BACK);
                }
            }
            else if(pstQueryCmd->u32EventId == BW_WIFI_FSM_MSG_WIFI_REQ_ROAMING_SCAN)
            {
                _BwWifiFsm_DoAutoConnect_Retry();
            }
        }
        return BW_WIFI_CMD_FAILED;
    }

    return BW_WIFI_CMD_EXECUTING;
}

static int32_t BwWifiFsm_TaskEvtHandler_Connect_Req(uint32_t u32EventId, uint8_t *pu8Data, uint32_t u32DataLen)
{
    wifi_conn_config_t *pstConnConfig = (wifi_conn_config_t*)pu8Data;
    int sRet=0;
    uint8_t u8CheckBssid[WIFI_MAC_ADDRESS_LENGTH] = {0};
    osEvent rxEvent;
    BleWifi_Wifi_FSM_CmdQ_t *pstQueryCmd;
    wifi_scan_config_t stScanConfig={0};

    printf("BLEWIFI: Recv BLEWIFI_REQ_CONNECT \r\n");

    memset(&g_wifi_config_req_connect , 0 ,sizeof(wifi_config_t));

    if(memcmp(u8CheckBssid , pstConnConfig->bssid , WIFI_MAC_ADDRESS_LENGTH) != 0)
    {
        memcpy(g_wifi_config_req_connect.sta_config.bssid, pstConnConfig->bssid, WIFI_MAC_ADDRESS_LENGTH);

        printf("sta_config.password.sta_config.bssid = %s\r\n",(char *)g_wifi_config_req_connect.sta_config.bssid);
    }
    else
    {
        memcpy(g_wifi_config_req_connect.sta_config.ssid, pstConnConfig->ssid, pstConnConfig->ssid_length);
        g_wifi_config_req_connect.sta_config.ssid_length = pstConnConfig->ssid_length;

        printf("sta_config.ssid_length = %d\r\n",g_wifi_config_req_connect.sta_config.ssid_length);
        printf("sta_config.ssid = %s\r\n",(char *)g_wifi_config_req_connect.sta_config.ssid);
    }
    g_wifi_config_req_connect.sta_config.password_length = pstConnConfig->password_length;
    memcpy((char *)g_wifi_config_req_connect.sta_config.password, pstConnConfig->password, g_wifi_config_req_connect.sta_config.password_length);

    ////  for debug  ////
//    printf("sta_config.ssid(len=%d) = %s\r\n",pstConnConfig->ssid_length, g_wifi_config_req_connect.sta_config.ssid);
//    printf("sta_config.bssid = %d\r\n",g_wifi_config_req_connect.sta_config.bssid);
    printf("sta_config.password_length = %d\r\n",g_wifi_config_req_connect.sta_config.password_length);
    printf("sta_config.password = %s\r\n",(char *)g_wifi_config_req_connect.sta_config.password);
    /////////////////////

    wifi_set_config(WIFI_MODE_STA, &g_wifi_config_req_connect);
    sRet = wifi_connection_connect(&g_wifi_config_req_connect);

    if( 0 != sRet )
    {
        printf("exec conn req failed\n");
        rxEvent = osMessagePeek(g_tBwWifiFsmCmdQueueId , 0);
        if(rxEvent.status == osEventMessage)
        {
            pstQueryCmd = (BleWifi_Wifi_FSM_CmdQ_t *)rxEvent.value.p;

            if(pstQueryCmd->u32EventId == BW_WIFI_FSM_MSG_WIFI_REQ_AUTO_CONNECT)
            {
                if (0 != g_RoamingApInfoTotalCnt)
                {
                    g_iIdxOfApInfo = 0; //initialize index for roaming connection

                    memset(&stScanConfig , 0 ,sizeof(stScanConfig));
                    stScanConfig.show_hidden = 1;
                    stScanConfig.scan_type = WIFI_SCAN_TYPE_MIX;

                    BleWifi_Wifi_FSM_CmdPush(BW_WIFI_FSM_MSG_WIFI_REQ_ROAMING_SCAN, (uint8_t *)&stScanConfig , sizeof(wifi_scan_config_t) , NULL, BLEWIFI_QUEUE_BACK);
                }
            }
            else if(pstQueryCmd->u32EventId == BW_WIFI_FSM_MSG_WIFI_REQ_ROAMING_CONNECT)
            {
                _BwWifiFsm_DoAutoConnect_Retry();
            }
        }
        return BW_WIFI_CMD_FAILED;
    }

    return BW_WIFI_CMD_EXECUTING;
}

static int32_t BwWifiFsm_TaskEvtHandler_Auto_Connect_Start_Req(uint32_t u32EventId, uint8_t *pu8Data, uint32_t u32DataLen)
{
    return BW_WIFI_CMD_FINISH;
}

static int32_t BwWifiFsm_TaskEvtHandler_Auto_Connect_Scan_Req(uint32_t u32EventId, uint8_t *pu8Data, uint32_t u32DataLen)
{
    uint8_t u8AutoConnListNum = 0;
    wifi_scan_config_t stScanConfig;
    int sRet;

    memset(&stScanConfig , 0 ,sizeof(stScanConfig));
    stScanConfig.show_hidden = 1;
    stScanConfig.scan_type = WIFI_SCAN_TYPE_MIX;

    // if the count of auto-connection list is empty, don't do the auto-connect
    BleWifi_Wifi_Query_Status(BLEWFII_WIFI_GET_AUTO_CONN_LIST_NUM , (void *)&u8AutoConnListNum);

    if (0 == u8AutoConnListNum)
    {
        _BwWifiFsm_Remove_Pair_CMD(BW_WIFI_FSM_MSG_WIFI_REQ_AUTO_CONNECT_SCAN);

        // do roaming
        if (0 != g_RoamingApInfoTotalCnt)
        {
            g_iIdxOfApInfo = 0; //initialize index for roaming connection

            BLEWIFI_INFO("Do roaming scan \r\n");

            BleWifi_Wifi_FSM_CmdPush(BW_WIFI_FSM_MSG_WIFI_REQ_ROAMING_SCAN, (uint8_t *)&stScanConfig , sizeof(wifi_scan_config_t) , NULL, BLEWIFI_QUEUE_FRONT);
            BleWifi_Wifi_FSM_MsgSend(BW_WIFI_FSM_MSG_WIFI_EXEC_CMD, NULL, 0, NULL, BLEWIFI_QUEUE_BACK);
        }
        return BW_WIFI_CMD_FINISH;
    }

    sRet = wifi_scan_start(&stScanConfig, NULL);
    if(0 != sRet)
    {
        // do roaming
        if (0 != g_RoamingApInfoTotalCnt)
        {
            g_iIdxOfApInfo = 0; //initialize index for roaming connection

            BLEWIFI_INFO("Do roaming scan \r\n");

            BleWifi_Wifi_FSM_CmdPush(BW_WIFI_FSM_MSG_WIFI_REQ_ROAMING_SCAN, (uint8_t *)&stScanConfig , sizeof(wifi_scan_config_t) , NULL, BLEWIFI_QUEUE_FRONT);
            BleWifi_Wifi_FSM_MsgSend(BW_WIFI_FSM_MSG_WIFI_EXEC_CMD, NULL, 0, NULL, BLEWIFI_QUEUE_BACK);
        }

        printf("exec scan req failed\r\n");
        return BW_WIFI_CMD_FAILED;
    }
    return BW_WIFI_CMD_EXECUTING;
}

static int32_t BwWifiFsm_TaskEvtHandler_Auto_Connect_Req(uint32_t u32EventId, uint8_t *pu8Data, uint32_t u32DataLen)
{
    int sRet = 0;
    wifi_scan_config_t stScanConfig={0};

    printf("Auto_Connect_Req\r\n");

    sRet = wifi_auto_connect_start();
    if(sRet != 0)
    {
        // do roaming
        if (0 != g_RoamingApInfoTotalCnt)
        {
            g_iIdxOfApInfo = 0; //initialize index for roaming connection

            memset(&stScanConfig , 0 ,sizeof(stScanConfig));
            stScanConfig.show_hidden = 1;
            stScanConfig.scan_type = WIFI_SCAN_TYPE_MIX;

            BLEWIFI_INFO("Do roaming scan \r\n");

            BleWifi_Wifi_FSM_CmdPush(BW_WIFI_FSM_MSG_WIFI_REQ_ROAMING_SCAN, (uint8_t *)&stScanConfig , sizeof(wifi_scan_config_t) , NULL, BLEWIFI_QUEUE_BACK);
            BleWifi_Wifi_FSM_MsgSend(BW_WIFI_FSM_MSG_WIFI_EXEC_CMD, NULL, 0, NULL, BLEWIFI_QUEUE_BACK);
        }

        return BW_WIFI_CMD_FAILED;
    }
    return BW_WIFI_CMD_EXECUTING;
}

static int32_t BwWifiFsm_TaskEvtHandler_Disconnect_Req(uint32_t u32EventId, uint8_t *pu8Data, uint32_t u32DataLen)
{
    printf("BLEWIFI: Recv BLEWIFI_REQ_DISCONNECT \r\n");

    int sRet=0;

    //g_BwWifiReqConnRetryTimes = BLEWIFI_CTRL_AUTO_CONN_STATE_IDLE;

    sRet = wifi_connection_disconnect_ap();

    if( 0 != sRet )
    {
        printf("exe disconn req failed\n");
        return BW_WIFI_CMD_FAILED;
    }

    return BW_WIFI_CMD_EXECUTING;
}

static int32_t BwWifiFsm_TaskEvtHandler_Stop_Req(uint32_t u32EventId, uint8_t *pu8Data, uint32_t u32DataLen)
{
    //special case , This cmd doesn't execute actually
    g_u8WifiCmdWaitingFlag = BLEWIFI_WIFI_EXEC_CMD_WAITING;
    osTimerStop(g_tAppCtrlAutoConnectTriggerTimer);
    BleWifi_COM_EventStatusSet(g_tWifiFsmEventGroup, BW_WIFI_FSM_EVENT_BIT_EXEC_AUTO_CONN, false);

    _BwWifiFsm_Remove_Pair_CMD(BW_WIFI_FSM_MSG_WIFI_STOP);

    return BW_WIFI_CMD_FINISH;
}

#ifdef TBD
static int32_t BwWifiFsm_TaskEvtHandler_WifiStatus_Req(uint32_t u32EventId, uint8_t *pu8Data, uint32_t u32DataLen)
{
    printf("BLEWIFI: Recv BLEWIFI_REQ_WIFI_STATUS \r\n");
    return BW_WIFI_CMD_FINISH;
}
#endif

#if 0
static int32_t BwWifiFsm_TaskEvtHandler_Reset_Req(uint32_t u32EventId, uint8_t *pu8Data, uint32_t u32DataLen)
{
    printf("BLEWIFI: Recv BLEWIFI_REQ_RESET \r\n");
    BleWifi_COM_EventStatusSet(g_tWifiFsmEventGroup, BW_WIFI_FSM_EVENT_BIT_EXEC_RESET, true);

    int32_t ret = 0;

    ret = wifi_auto_connect_reset();
    BleWifi_Ble_SendResponse(BLEWIFI_RSP_RESET, ret);
//    g_BwWifiReqConnRetryTimes = BLEWIFI_CTRL_AUTO_CONN_STATE_IDLE;


    ret&=wifi_connection_disconnect_ap();


    if( 0 != ret )
    {
        printf("exe reset req failed\n");
        return BW_WIFI_CMD_FAILED;
    }

    return BW_WIFI_CMD_EXECUTING;

}
#endif

static int32_t BwWifiFsm_TaskEvtHandler_WifiCommand_Exec(uint32_t u32EventId, uint8_t *pu8Data, uint32_t u32DataLen)
{
    osEvent tRxEvent;
    BleWifi_Wifi_FSM_CmdQ_t* pstNextCmd;

    printf("BwWifiFsm_TaskEvtHandler_WifiCommand_Exec\n\n");

    if (BLEWIFI_WIFI_EXEC_CMD_WAITING == g_u8WifiCmdWaitingFlag)
    {
        printf("Waiting.......\n");
    }

    if (BLEWIFI_WIFI_EXEC_CMD_CONTINUE == g_u8WifiCmdWaitingFlag)
    {
        tRxEvent = osMessagePeek(g_tBwWifiFsmCmdQueueId, 0);
        if (tRxEvent.status == osEventMessage)
        {
            pstNextCmd = (BleWifi_Wifi_FSM_CmdQ_t *)tRxEvent.value.p;
            printf("Next cmd is 0x%02x\n", pstNextCmd->u32EventId);
            BleWifi_Wifi_FSM(pstNextCmd->u32EventId, pstNextCmd->u8aData, pstNextCmd->u32Length);
        }
    }

    return 0;
}

static int32_t BwWifiFsm_TaskEvtHandler_DHCPTimeout(uint32_t u32EventId, uint8_t *pu8Data, uint32_t u32DataLen)
{
    if(BW_WIFI_FSM_STATE_CONNECTED == g_u8CurrFSMState)
    {
        uint8_t u8Reason = BW_WIFI_STOP_CONNECT;
        BleWifi_COM_EventStatusSet(g_tWifiFsmEventGroup, BW_WIFI_FSM_EVENT_BIT_DHCP_TIMEOUT, true);

        printf("[WiFiFSM]DHCP TMO, WiFi Disconnect\n", BW_WIFI_FSM_MSG_WIFI_DISCONNECTION_IND);

        BleWifi_Wifi_FSM_CmdPush(BW_WIFI_FSM_MSG_WIFI_REQ_DISCONNECT, &u8Reason, sizeof(uint8_t), BleWifi_Wifi_Req_Disconnected_CB, BLEWIFI_QUEUE_BACK);         //DHCP timeout need notify appliction
        BleWifi_Wifi_FSM_CmdPush(BW_WIFI_FSM_MSG_WIFI_REQ_AUTO_CONNECT, NULL, 0, NULL, BLEWIFI_QUEUE_BACK);
        BleWifi_Wifi_FSM_MsgSend(BW_WIFI_FSM_MSG_WIFI_EXEC_CMD, NULL, 0, NULL, BLEWIFI_QUEUE_BACK);
    }

    return 0;
}

#if 0
static int32_t BwWifiFsm_TaskEvtHandler_WifiAutoConnectInd(uint32_t u32EventId, uint8_t *pu8Data, uint32_t u32DataLen)
{
    BLEWIFI_INFO("BLEWIFI: MSG BLEWIFI_CTRL_MSG_WIFI_AUTO_CONNECT_IND \r\n");
    int ret = wifi_auto_connect_start();
    if(0 == ret)
    {
        printf("\n[%s %d]timer expired! trigger AutoConnect, change to connecting state\n\n", __FUNCTION__, __LINE__);
        g_BwWifiReqConnRetryTimes = BLEWIFI_CTRL_AUTO_CONN_STATE_IDLE;
    }
    else
    {
        printf("\n[%s %d]wifi_auto_connect_start() failed.\n\n", __FUNCTION__, __LINE__);
    }

    return 1;
}
#endif

static int32_t BleWifi_Wifi_FSM_MsgHandler(T_Bw_Wifi_FsmEvtHandlerTbl tHanderTbl[], uint32_t u32EventId, uint8_t *pu8Data, uint32_t u32DataLen)
{
    //BleWifi_Wifi_FSM_CmdQ_t *pstQueryCmd;
    osEvent rxEvent;
    int8_t u8Status = BW_WIFI_CMD_NOT_MATCH;
    uint32_t i = 0;

    while (tHanderTbl[i].u32EventId != BW_WIFI_FSM_MSG_HANDLER_TBL_END)
    {
        // match
        if (tHanderTbl[i].u32EventId == u32EventId)
        {
            u8Status = tHanderTbl[i].fpFunc(u32EventId, pu8Data, u32DataLen);
            break;
        }
        i++;
    }

    // M3 CMD timeout or Req fail
    if ((u8Status == BW_WIFI_CMD_M3_TIMEOUT) || (u8Status == BW_WIFI_CMD_FAILED))
    {
        printf("tHanderTbl[i].u32EventId = 0x%x , BW_WIFI_CMD_FAILED\r\n" ,tHanderTbl[i].u32EventId );
        if(u8Status == BW_WIFI_CMD_M3_TIMEOUT)
        {
            printf("M3 CMD timeout\r\n");
        }
        memset(&rxEvent, 0, sizeof(osEvent));
        rxEvent = osMessageGet(g_tBwWifiFsmCmdQueueId, 0); // pop from queue
        free(rxEvent.value.p);

        g_u8WifiCmdWaitingFlag = BLEWIFI_WIFI_EXEC_CMD_CONTINUE;
        if(BleWifi_Wifi_IsQueueEmpty(g_tBwWifiFsmCmdQueueId) == false)  // cmdq is not empty
        {
            BleWifi_Wifi_FSM_MsgSend(BW_WIFI_FSM_MSG_WIFI_EXEC_CMD, NULL, 0, NULL, BLEWIFI_QUEUE_BACK);
        }
    }
    // CMD FINISH
    else if (u8Status == BW_WIFI_CMD_FINISH)
    {
#if 0
        printf("tHanderTbl[i].u32EventId = 0x%x , BW_WIFI_CMD_FINISH\r\n" ,tHanderTbl[i].u32EventId );
        if((tHanderTbl[i].u32EventId > BW_WIFI_FSM_MSG_WIFI_IND_START) && (tHanderTbl[i].u32EventId < BW_WIFI_FSM_MSG_WIFI_IND_END))
        {
            rxEvent = osMessagePeek(g_tBwWifiFsmCmdQueueId , 0);

            if(rxEvent.status == osEventMessage)
            {
                pstQueryCmd = (BleWifi_Wifi_FSM_CmdQ_t *)rxEvent.value.p;

                /*if(tHanderTbl[i].u32EventId == BW_WIFI_FSM_MSG_WIFI_CONNECTION_FAIL_IND) // special case
                {
                    tHanderTbl[i].u32EventId--;  //transform to BW_WIFI_FSM_MSG_WIFI_CONNECTION_SUCCESS_IND for cmd compare
                }*/

                if((pstQueryCmd->u32EventId == (tHanderTbl[i].u32EventId - BW_WIFI_FSM_MSG_WIFI_IND_START)) ||
                    (pstQueryCmd->u32EventId == BW_WIFI_FSM_MSG_WIFI_REQ_CONNECT && tHanderTbl[i].u32EventId == BW_WIFI_FSM_MSG_WIFI_CONNECTION_SUCCESS_IND) ||
                    (pstQueryCmd->u32EventId == BW_WIFI_FSM_MSG_WIFI_REQ_CONNECT && tHanderTbl[i].u32EventId == BW_WIFI_FSM_MSG_WIFI_CONNECTION_FAIL_IND) ||
                    (pstQueryCmd->u32EventId == BW_WIFI_FSM_MSG_WIFI_REQ_CONNECT && tHanderTbl[i].u32EventId == BW_WIFI_FSM_MSG_WIFI_DISCONNECTION_IND) ||
                    (pstQueryCmd->u32EventId == BW_WIFI_FSM_MSG_WIFI_REQ_AUTO_CONNECT_SCAN && tHanderTbl[i].u32EventId == BW_WIFI_FSM_MSG_WIFI_SCAN_DONE_IND) ||
                    (pstQueryCmd->u32EventId == BW_WIFI_FSM_MSG_WIFI_REQ_AUTO_CONNECT && tHanderTbl[i].u32EventId == BW_WIFI_FSM_MSG_WIFI_CONNECTION_SUCCESS_IND) ||
                    (pstQueryCmd->u32EventId == BW_WIFI_FSM_MSG_WIFI_REQ_AUTO_CONNECT && tHanderTbl[i].u32EventId == BW_WIFI_FSM_MSG_WIFI_CONNECTION_FAIL_IND) ||
                    (pstQueryCmd->u32EventId == BW_WIFI_FSM_MSG_WIFI_REQ_AUTO_CONNECT && tHanderTbl[i].u32EventId == BW_WIFI_FSM_MSG_WIFI_DISCONNECTION_IND))


                {
                    memset(&rxEvent , 0 , sizeof(osEvent));
                    rxEvent = osMessageGet(g_tBwWifiFsmCmdQueueId , 0); // pop from queue
                    free(rxEvent.value.p);

                    g_u8WifiCmdWaitingFlag = BLEWIFI_WIFI_EXEC_CMD_CONTINUE;
                    if(BleWifi_Wifi_IsQueueEmpty(g_tBwWifiFsmCmdQueueId) == false)  // cmdq is not empty
                    {
                        BleWifi_Wifi_FSM_MsgSend(BW_WIFI_FSM_MSG_WIFI_EXEC_CMD, NULL, 0, NULL, BLEWIFI_QUEUE_BACK);
                    }
                }
            }
        }
#endif
#if 0
        if ((tHanderTbl[i].u32EventId >= BW_WIFI_FSM_MSG_WIFI_REQ_STATUS) && (tHanderTbl[i].u32EventId <= BW_WIFI_FSM_MSG_WIFI_DISCONNECTION_IND))
        {
            rxEvent = osMessagePeek(g_tBwWifiFsmCmdQueueId, 0);
            if (rxEvent.status == osEventMessage)
            {
                pstQueryCmd = (BleWifi_Wifi_FSM_CmdQ_t *)rxEvent.value.p;

                //scan req -> rev scan done, conn req -> rev connected
//                if(pstQueryCmd->u32EventId == (tHanderTbl[i].ulEventId - 0x07))
                {
                    printf("Pre cmd is 0x%02x, Rcv Id is 0x%02x, so rem!!\n", pstQueryCmd->u32EventId, tHanderTbl[i].u32EventId);
                    memset(&rxEvent , 0 , sizeof(osEvent));
                    rxEvent = osMessageGet(g_tBwWifiFsmCmdQueueId , 0); // pop from queue
                    free(rxEvent.value.p);

                    g_u8WifiCmdWaitingFlag = BLEWIFI_WIFI_EXEC_CMD_CONTINUE;
                    BleWifi_Wifi_FSM_MsgSend(BW_WIFI_FSM_MSG_WIFI_EXEC_CMD, NULL, 0, NULL, BLEWIFI_QUEUE_BACK);
                }
            }
        }
//        else if(tHanderTbl[i].ulEventId == BW_WIFI_FSM_MSG_WIFI_DISCONNECTION_IND)
//        {
//            printf("Rcv Disconn, g_u8WifiCmdWaitingFlag=BLEWIFI_WIFI_EXEC_CMD_CONTINUE\n");
//            g_u8WifiCmdWaitingFlag = BLEWIFI_WIFI_EXEC_CMD_CONTINUE;
//        }
#endif
    }
    // CMD is executing......
    else if (u8Status == BW_WIFI_CMD_EXECUTING)
    {
        printf("tHanderTbl[i].u32EventId = 0x%x , BW_WIFI_CMD_EXECUTING\r\n" ,tHanderTbl[i].u32EventId );
        g_u8WifiCmdWaitingFlag = BLEWIFI_WIFI_EXEC_CMD_WAITING;
    }
    // CMD is not match
    else if (u8Status == BW_WIFI_CMD_NOT_MATCH)
    {
        printf("tHanderTbl[i].u32EventId = 0x%x , BW_WIFI_CMD_NOT_MATCH\r\n" ,tHanderTbl[i].u32EventId );
        if((u32EventId > BW_WIFI_FSM_MSG_WIFI_REQ_START) && (u32EventId < BW_WIFI_FSM_MSG_WIFI_REQ_END))
        {
            memset(&rxEvent , 0 , sizeof(osEvent));
            rxEvent = osMessageGet(g_tBwWifiFsmCmdQueueId , 0); // pop from queue
            free(rxEvent.value.p);

            if(BleWifi_Wifi_IsQueueEmpty(g_tBwWifiFsmCmdQueueId) == false)  // cmdq is not empty
            {
                BleWifi_Wifi_FSM_MsgSend(BW_WIFI_FSM_MSG_WIFI_EXEC_CMD, NULL, 0, NULL, BLEWIFI_QUEUE_BACK);
            }
        }
    }

//    memset(&rxEvent , 0 , sizeof(osEvent));
//    rxEvent = osMessagePeek(g_tBwWifiFsmCmdQueueId , 0);
//    if(rxEvent.status == osEventMessage)  // cmdq is not empty
//    {
//        pstQueryCmd = (BleWifi_Wifi_FSM_CmdQ_t *)rxEvent.value.p;
//        printf("Next cmd is 0x%02x, so Exe!!\n", pstQueryCmd->u32EventId);
//        BleWifi_Wifi_FSM_MsgSend(BW_WIFI_FSM_MSG_WIFI_EXEC_CMD, NULL, 0, BLEWIFI_QUEUE_BACK);
//    }

    return 0;
}

int32_t BleWifi_Wifi_FSM(uint32_t u32EventId, uint8_t *pu8Data, uint32_t u32DataLen)
{
    printf("The Wifi FSM state[%u], event id[0x%X]\n", g_u8CurrFSMState, u32EventId);
    return BleWifi_Wifi_FSM_MsgHandler(g_tBwWifiFSMStateTbl[g_u8CurrFSMState], u32EventId, pu8Data, u32DataLen);
}

static int32_t BleWifi_Wifi_FSM_TaskEvtHandler(uint32_t u32EventId, uint8_t *pu8Data, uint32_t u32DataLen)
{
    uint32_t i = 0;

    while (g_tBwWifiFsmEvtHandlerTbl[i].u32EventId != BW_WIFI_FSM_MSG_HANDLER_TBL_END)
    {
        // match
        if (g_tBwWifiFsmEvtHandlerTbl[i].u32EventId == u32EventId)
        {
            g_tBwWifiFsmEvtHandlerTbl[i].fpFunc(u32EventId, pu8Data, u32DataLen);
            break;
        }

        i++;
    }

    // not match
    if (g_tBwWifiFsmEvtHandlerTbl[i].u32EventId == BW_WIFI_FSM_MSG_HANDLER_TBL_END)
    {
    }

    return 0;
}

void BleWifi_Wifi_FSM_Task(void *args)
{
    osEvent tRxEvent;
    BleWifi_Wifi_FSM_CmdQ_t *ptMsg;

    for (;;)
    {
        /* Wait event */
        tRxEvent = osMessageGet(g_tBwWifiFsmMsgQueueId, osWaitForever);
        if (tRxEvent.status != osEventMessage)
            continue;

        ptMsg = (BleWifi_Wifi_FSM_CmdQ_t *)tRxEvent.value.p;

        osSemaphoreWait(g_tWifiInternalSemaphoreId, osWaitForever);

        if (ptMsg->u32EventId >= BW_WIFI_FSM_MSG_WIFI_EXEC_CMD)
            BleWifi_Wifi_FSM_TaskEvtHandler(ptMsg->u32EventId, ptMsg->u8aData, ptMsg->u32Length);
        else
            BleWifi_Wifi_FSM(ptMsg->u32EventId, ptMsg->u8aData, ptMsg->u32Length);

        osSemaphoreRelease(g_tWifiInternalSemaphoreId);

        /* Release buffer */
        if (ptMsg != NULL)
            free(ptMsg);
    }
}

static int32_t BleWifi_Wifi_FSM_CommonSend(osMessageQId tQueueId, uint32_t u32MsgType, uint8_t *pu8Data, uint32_t u32DataLen, T_BleWifi_Wifi_App_Ctrl_CB_Fp fpCB, uint32_t u32IsFront)
{
    BleWifi_Wifi_FSM_CmdQ_t *ptMsg;

	if (NULL == tQueueId)
	{
        BLEWIFI_ERROR("BLEWIFI: Wifi FSM is no msg queue\n");
        goto error;
	}

    /* Mem allocate */
    ptMsg = malloc(sizeof(BleWifi_Wifi_FSM_CmdQ_t) + u32DataLen);
    if (ptMsg == NULL)
	{
        BLEWIFI_ERROR("BLEWIFI: Wifi FSM task pMsg allocate fail\n");
	    goto error;
    }

    ptMsg->u32EventId = u32MsgType;
    ptMsg->u32Length = u32DataLen;
    ptMsg->fpCtrlAppCB = fpCB;
    if (u32DataLen > 0)
    {
        memcpy(ptMsg->u8aData, pu8Data, u32DataLen);
    }

    // Front
    if (u32IsFront == BLEWIFI_QUEUE_FRONT)
    {
        if (osOK != osMessagePutFront(tQueueId, (uint32_t)ptMsg, 0))
        {
            printf("BLEWIFI: Wifi FSM task message send fail. reset system\r\n");
            osDelay(3000);
            Hal_Sys_SwResetAll();
            goto error;
        }
    }
    // Back
    else
    {
        if (osOK != osMessagePut(tQueueId, (uint32_t)ptMsg, 0))
        {
            printf("BLEWIFI: Wifi FSM task message send fail. reset system \r\n");
            osDelay(3000);
            Hal_Sys_SwResetAll();
            goto error;
        }
    }

    return 0;

error:
	if (ptMsg != NULL)
	{
	    free(ptMsg);
    }

	return -1;
}

int32_t BleWifi_Wifi_FSM_MsgSend(uint32_t u32MsgType, uint8_t *pu8Data, uint32_t u32DataLen, T_BleWifi_Wifi_App_Ctrl_CB_Fp fpCB, uint32_t u32IsFront)
{
    return BleWifi_Wifi_FSM_CommonSend(g_tBwWifiFsmMsgQueueId, u32MsgType, pu8Data, u32DataLen, fpCB, u32IsFront);
}

int32_t BleWifi_Wifi_FSM_CmdPush_Without_Check(uint32_t u32EventId, uint8_t *pu8Data, uint32_t u32DataLen, T_BleWifi_Wifi_App_Ctrl_CB_Fp fpCB, uint32_t u32IsFront)
{
    return BleWifi_Wifi_FSM_CommonSend(g_tBwWifiFsmCmdQueueId, u32EventId, pu8Data, u32DataLen, fpCB, u32IsFront);
}

int32_t BleWifi_Wifi_FSM_CmdPush(uint32_t u32EventId, uint8_t *pu8Data, uint32_t u32DataLen, T_BleWifi_Wifi_App_Ctrl_CB_Fp fpCB, uint32_t u32IsFront)
{
    if(BleWifi_COM_EventStatusGet(g_tWifiFsmEventGroup, BW_WIFI_FSM_EVENT_BIT_WIFI_USED) == true)
    {
        return BleWifi_Wifi_FSM_CmdPush_Without_Check(u32EventId , pu8Data , u32DataLen , fpCB , u32IsFront);
    }
    else // if wifi stop ,commandQ don't accepted any command form internal FSM.
    {
        return 0;
    }
}

static void BleWifi_Wifi_Fsm_DHCP_TimeOutCallBack(void const *argu)
{
    BleWifi_Wifi_FSM_MsgSend(BW_WIFI_FSM_MSG_DHCP_TIMEOUT, NULL, 0, NULL, BLEWIFI_QUEUE_BACK);
}

void BleWifi_Wifi_FSM_Init(void)
{
    osThreadDef_t task_def;
    osMessageQDef_t queue_def;
    osTimerDef_t timer_dhcp_def;

    g_u8CurrFSMState = BW_WIFI_FSM_STATE_INIT;

    /* Create wifi FSM message queue*/
    queue_def.queue_sz = BLEWIFI_WIFI_FSM_MSG_QUEUE_SIZE;
    g_tBwWifiFsmMsgQueueId = osMessageCreate(&queue_def, NULL);
    if(g_tBwWifiFsmMsgQueueId == NULL)
    {
        BLEWIFI_ERROR("BLEWIFI: Wifi FSM task create msg queue fail\n");
    }

    /* Create wifi FSM Cmd queue*/
    queue_def.queue_sz = BLEWIFI_WIFI_FSM_CMD_QUEUE_SIZE;
    g_tBwWifiFsmCmdQueueId = osMessageCreate(&queue_def, NULL);
    if(g_tBwWifiFsmCmdQueueId == NULL)
    {
        BLEWIFI_ERROR("BLEWIFI: Wifi FSM task create cmd queue fail\n");
    }

    /* Create wifi FSM task */
    task_def.name = BLEWIFI_WIFI_FSM_TASK_NAME;
    task_def.stacksize = BLEWIFI_WIFI_FSM_TASK_STACK_SIZE;
    task_def.tpriority = BLEWIFI_WIFI_FSM_TASK_PRIORITY;
    task_def.pthread = BleWifi_Wifi_FSM_Task;
    g_tBwWifiFsmTaskId = osThreadCreate(&task_def, (void*)NULL);
    if(g_tBwWifiFsmTaskId == NULL)
    {
        BLEWIFI_ERROR("BLEWIFI: Wifi FSM task create fail\n");
    }
    else
    {
        BLEWIFI_INFO("BLEWIFI: Wifi FSM task create successful\n");
    }

    /* create dhcp timeout timer */
    timer_dhcp_def.ptimer = BleWifi_Wifi_Fsm_DHCP_TimeOutCallBack;
    g_tAppCtrlDHCPTimer = osTimerCreate(&timer_dhcp_def, osTimerOnce, NULL);
    if (g_tAppCtrlDHCPTimer == NULL)
    {
        BLEWIFI_ERROR("BLEWIFI: create DHCP timer fail \r\n");
    }


}



