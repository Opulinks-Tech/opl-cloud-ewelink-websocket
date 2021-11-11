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

#include "ble_hci_if.h"
#include "ble_cm_if.h"
#include "ble_smp_if.h"
#include "ble_gap_if.h"
#include "ble_gatt_if.h"
#include "blewifi_server_app_gatt.h"

#include "blewifi_ble_api.h"
#include "blewifi_ble_FSM.h"
#include "blewifi_server_app.h"
#include "blewifi_common.h"
#include "app_ctrl.h"
#include "blewifi_wifi_api.h"
#include "mw_ota_def.h"
#include "mw_ota.h"
#include "hal_os_patch.h"
#include "blewifi_data.h"
#include "blewifi_handle.h"
#include "hal_system.h"

extern BLE_APP_DATA_T gTheBle;
static BLE_ADV_TIME_T gTheBleAdvTime;
static uint8_t g_u8BleFsmState = BLEWIFI_BLE_STATE_INIT;
static uint8_t g_u8IsPushADVExitCmd = 0;  //push exit ADV cmd from application
//static BLE_ADV_TIME_T g_stADVTime = {0};
extern osMessageQId g_tBleCMDQ_Id;
uint8_t g_u8BleCMDWaitingFlag = BLEWIFI_BLE_EXEC_CMD_CONTINUE; // 0 : no waiting , 1 : waiting

static int32_t BleWifi_Ble_FSM_MsgHandler_Initializing(uint32_t u32EvtType, void *data, int len);
static int32_t BleWifi_Ble_FSM_MsgHandler_EnterAdvertising(uint32_t u32EvtType, void *data, int len);
static int32_t BleWifi_Ble_FSM_MsgHandler_ExitAdvertising(uint32_t u32EvtType, void *data, int len);
static int32_t BleWifi_Ble_FSM_MsgHandler_ChangeAdvertisingTime(uint32_t u32EvtType, void *data, int len);
static int32_t BleWifi_Ble_FSM_MsgHandler_Disconnection(uint32_t u32EvtType, void *data, int len);
static int32_t BleWifi_Ble_FSM_MsgHandler_ChangeADVData(uint32_t u32EvtType, void *data, int len);
static int32_t BleWifi_Ble_FSM_MsgHandler_ChangeScanData(uint32_t u32EvtType, void *data, int len);
static int32_t BleWifi_Ble_FSM_MsgHandler_BleStop(uint32_t u32EvtType, void *data, int len);

static int32_t BleWifi_Ble_FSM_MsgHandler_InitComplete_Cfm(uint32_t u32EvtType, void *data, int len);
static int32_t BleWifi_Ble_FSM_MsgHandler_EnterAdvertising_Cfm(uint32_t u32EvtType, void *data, int len);
static int32_t BleWifi_Ble_FSM_MsgHandler_ExitAdvertising_Cfm(uint32_t u32EvtType, void *data, int len);
static int32_t BleWifi_Ble_FSM_MsgHandler_ChangeAdvertisingTime_Cfm(uint32_t u32EvtType, void *data, int len);
static int32_t BleWifi_Ble_FSM_MsgHandler_Disconnect_Cfm(uint32_t u32EvtType, void *data, int len);
static int32_t BleWifi_Ble_FSM_MsgHandler_Connection_Success(uint32_t u32EvtType, void *data, int len);
static int32_t BleWifi_Ble_FSM_MsgHandler_Connection_Fail(uint32_t u32EvtType, void *data, int len);

static int32_t BleWifi_Ble_FSM_MsgHandler_SEND_DATA(uint32_t u32EvtType, void *data, int len);
static int32_t BleWifi_Ble_FSM_MsgHandler_SEND_TO_PEER(uint32_t u32EvtType, void *data, int len);
static int32_t BleWifi_Ble_FSM_MsgHandler_SEND_TO_PEER_CFM(uint32_t u32EvtType, void *data, int len);

static int32_t BleWifi_Ble_FSM_MsgHandler_SetADVData_CFM(uint32_t u32EvtType, void *data, int len);
static int32_t BleWifi_Ble_FSM_MsgHandler_SetScanData_CFM(uint32_t u32EvtType, void *data, int len);
static int32_t BleWifi_Ble_FSM_MsgHandler_SignalUpdate_REQ(uint32_t u32EvtType, void *data, int len);
static int32_t BleWifi_Ble_FSM_MsgHandler_ConnPara_REQ(uint32_t u32EvtType, void *data, int len);
static int32_t BleWifi_Ble_FSM_MsgHandler_ConnUpdateComplete_CFM(uint32_t u32EvtType, void *data, int len);
static int32_t BleWifi_Ble_FSM_MsgHandler_SetDisconnection_CFM(uint32_t u32EvtType, void *data, int len);

static int32_t BleWifi_Ble_FSM_MsgHandler(T_BleWifi_Ble_FSM_MsgHandlerTbl tHanderTbl[], uint32_t u32EvtType , void *data , int len);

T_BleWifi_Ble_FSM_MsgHandlerTbl gstInitEventTbl[] =
{
    {BW_BLE_FSM_INITIALIZING,                  BleWifi_Ble_FSM_MsgHandler_Initializing},
    {BW_BLE_FSM_ADVERTISING_TIME_CHANGE,       BleWifi_Ble_FSM_MsgHandler_ChangeAdvertisingTime},
    {BW_BLE_FSM_CHANGE_ADVERTISING_DATA,       BleWifi_Ble_FSM_MsgHandler_ChangeADVData},
    {BW_BLE_FSM_CHANGE_SCAN_DATA,              BleWifi_Ble_FSM_MsgHandler_ChangeScanData},
    {BW_BLE_FSM_BLE_STOP,                      BleWifi_Ble_FSM_MsgHandler_BleStop},

    {BW_BLE_FSM_ADVERTISING_TIME_CHANGE_CFM,   BleWifi_Ble_FSM_MsgHandler_ChangeAdvertisingTime_Cfm},
    {BW_BLE_FSM_INIT_COMPLETE,                 BleWifi_Ble_FSM_MsgHandler_InitComplete_Cfm},
    {BW_BLE_FSM_SET_ADV_DATA_CFM,              BleWifi_Ble_FSM_MsgHandler_SetADVData_CFM},
    {BW_BLE_FSM_SET_SCAN_DATA_CFM,             BleWifi_Ble_FSM_MsgHandler_SetScanData_CFM},

    {BW_BLE_FSM_END,                           NULL}
};

T_BleWifi_Ble_FSM_MsgHandlerTbl gstIdleEventTbl[] =
{
    {BW_BLE_FSM_ADVERTISING_START,             BleWifi_Ble_FSM_MsgHandler_EnterAdvertising},
    {BW_BLE_FSM_ADVERTISING_TIME_CHANGE,       BleWifi_Ble_FSM_MsgHandler_ChangeAdvertisingTime},
    {BW_BLE_FSM_CHANGE_ADVERTISING_DATA,       BleWifi_Ble_FSM_MsgHandler_ChangeADVData},
    {BW_BLE_FSM_CHANGE_SCAN_DATA,              BleWifi_Ble_FSM_MsgHandler_ChangeScanData},
    {BW_BLE_FSM_BLE_STOP,                      BleWifi_Ble_FSM_MsgHandler_BleStop},

    {BW_BLE_FSM_ADVERTISING_START_CFM,         BleWifi_Ble_FSM_MsgHandler_EnterAdvertising_Cfm},
    {BW_BLE_FSM_ADVERTISING_TIME_CHANGE_CFM,   BleWifi_Ble_FSM_MsgHandler_ChangeAdvertisingTime_Cfm},
    {BW_BLE_FSM_SET_ADV_DATA_CFM,              BleWifi_Ble_FSM_MsgHandler_SetADVData_CFM},
    {BW_BLE_FSM_SET_SCAN_DATA_CFM,             BleWifi_Ble_FSM_MsgHandler_SetScanData_CFM},

    {BW_BLE_FSM_END,            NULL}
};


T_BleWifi_Ble_FSM_MsgHandlerTbl gstAdvertisingEventTbl[] =
{
    {BW_BLE_FSM_ADVERTISING_EXIT,              BleWifi_Ble_FSM_MsgHandler_ExitAdvertising},
    {BW_BLE_FSM_ADVERTISING_TIME_CHANGE,       BleWifi_Ble_FSM_MsgHandler_ChangeAdvertisingTime},
    {BW_BLE_FSM_CHANGE_ADVERTISING_DATA,       BleWifi_Ble_FSM_MsgHandler_ChangeADVData},
    {BW_BLE_FSM_CHANGE_SCAN_DATA,              BleWifi_Ble_FSM_MsgHandler_ChangeScanData},
    {BW_BLE_FSM_BLE_STOP,                      BleWifi_Ble_FSM_MsgHandler_BleStop},

    {BW_BLE_FSM_ADVERTISING_EXIT_CFM,          BleWifi_Ble_FSM_MsgHandler_ExitAdvertising_Cfm},
    {BW_BLE_FSM_CONNECTION_SUCCESS,            BleWifi_Ble_FSM_MsgHandler_Connection_Success},
    {BW_BLE_FSM_CONNECTION_FAIL,               BleWifi_Ble_FSM_MsgHandler_Connection_Fail},
    {BW_BLE_FSM_SET_ADV_DATA_CFM,              BleWifi_Ble_FSM_MsgHandler_SetADVData_CFM},
    {BW_BLE_FSM_SET_SCAN_DATA_CFM,             BleWifi_Ble_FSM_MsgHandler_SetScanData_CFM},

    {BW_BLE_FSM_END,            NULL}
};

T_BleWifi_Ble_FSM_MsgHandlerTbl gstConnectedEventTbl[] =
{
    {BW_BLE_FSM_DISCONNECT,                    BleWifi_Ble_FSM_MsgHandler_Disconnection},
    {BW_BLE_FSM_ADVERTISING_TIME_CHANGE,       BleWifi_Ble_FSM_MsgHandler_ChangeAdvertisingTime},
    {BW_BLE_FSM_CHANGE_ADVERTISING_DATA,       BleWifi_Ble_FSM_MsgHandler_ChangeADVData},
    {BW_BLE_FSM_CHANGE_SCAN_DATA,              BleWifi_Ble_FSM_MsgHandler_ChangeScanData},

    {BW_BLE_FSM_ADVERTISING_TIME_CHANGE_CFM,   BleWifi_Ble_FSM_MsgHandler_ChangeAdvertisingTime_Cfm},
    {BW_BLE_FSM_DISCONNECT_CFM,                BleWifi_Ble_FSM_MsgHandler_Disconnect_Cfm},

    {BW_BLE_FSM_SEND_DATA,                     BleWifi_Ble_FSM_MsgHandler_SEND_DATA},
    {BW_BLE_FSM_SEND_TO_PEER,                  BleWifi_Ble_FSM_MsgHandler_SEND_TO_PEER},
    {BW_BLE_FSM_SEND_TO_PEER_CFM,              BleWifi_Ble_FSM_MsgHandler_SEND_TO_PEER_CFM},

    {BW_BLE_FSM_SET_ADV_DATA_CFM,              BleWifi_Ble_FSM_MsgHandler_SetADVData_CFM},
    {BW_BLE_FSM_SET_SCAN_DATA_CFM,             BleWifi_Ble_FSM_MsgHandler_SetScanData_CFM},
    {BW_BLE_FSM_SIGNAL_UPDATE_REQ,             BleWifi_Ble_FSM_MsgHandler_SignalUpdate_REQ},
    {BW_BLE_FSM_CONN_PARA_REQ,                 BleWifi_Ble_FSM_MsgHandler_ConnPara_REQ},
    {BW_BLE_FSM_CONN_UPDATE_COMPLETE_CFM,      BleWifi_Ble_FSM_MsgHandler_ConnUpdateComplete_CFM},
    {BW_BLE_FSM_SET_DISCONNECT_CFM,            BleWifi_Ble_FSM_MsgHandler_SetDisconnection_CFM},

    {BW_BLE_FSM_END,            NULL}
};

static T_BleWifi_Ble_FSM_MsgHandlerTbl *g_pstBleFSMStateTbl[] =
{
    {gstInitEventTbl},
    {gstIdleEventTbl},
    {gstAdvertisingEventTbl},
    {gstConnectedEventTbl}
};

int32_t _BwBleClearQueue(osMessageQId tBwBleCmdQueueId)
{
    osEvent rxEvent;

    while(1)
    {
        rxEvent = osMessageGet(tBwBleCmdQueueId , 0); // pop from queue
        if (rxEvent.status == osEventMessage)
        {
            /*
            // for debug
            BleWifi_BLE_FSM_CmdQ_t *pstTmpCmd = NULL;
            pstTmpCmd = (BleWifi_BLE_FSM_CmdQ_t *)rxEvent.value.p;
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

int32_t BwBleFlushCMD(osMessageQId tBwBleCmdQueueId)
{
    osEvent rxEvent;
    BleWifi_BLE_FSM_CmdQ_t *pstPeekCmd = NULL;
    BleWifi_BLE_FSM_CmdQ_t *pstTmpCmd = NULL;

    if(g_u8BleCMDWaitingFlag == BLEWIFI_BLE_EXEC_CMD_WAITING)    //keep executing cmd.
    {
        rxEvent = osMessagePeek(tBwBleCmdQueueId , 0);

        if(rxEvent.status == osEventMessage) //Queue has least one CMD
        {
            pstPeekCmd = (BleWifi_BLE_FSM_CmdQ_t *)rxEvent.value.p;
            if(pstPeekCmd == NULL)
            {
                printf("Error BwBleFlushCMD cmd is null!\r\n");
                goto done;
            }
            else
            {
                /* Mem allocate */
                pstTmpCmd = malloc(sizeof(BleWifi_BLE_FSM_CmdQ_t) + pstPeekCmd->u32Length);
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
                _BwBleClearQueue(tBwBleCmdQueueId);

                //restore the waiting cmd into queue
                BleWifi_Ble_PushToFSMCmdQ(pstTmpCmd->u32EventId , pstTmpCmd->u8aData , pstTmpCmd->u32Length , pstTmpCmd->fpCtrlAppCB);

                //free the temp cmd
                free(pstTmpCmd);
            }
        }
        else // error case , if g_u8BleCMDWaitingFlag is waiting
        {
            printf("Error BwBleFlushCMD no cmd!\r\n");
            goto done;
        }
    }
    else
    {
        _BwBleClearQueue(tBwBleCmdQueueId);
    }

done:

    return 0;
}

int32_t BleWifi_Ble_Queue_Push(osMessageQId tBwBleCmdQueueId , uint32_t u32EventId , uint8_t *pu8Data, uint32_t u32Length , T_BleWifi_Ble_App_Ctrl_CB_Fp fpCB , uint32_t u32IsFront)
{
    BleWifi_BLE_FSM_CmdQ_t *pCmd = NULL;

	if (NULL == tBwBleCmdQueueId)
	{
        BLEWIFI_ERROR("BLEWIFI: BLE FSM cmd queue is null\n");
        goto error;
	}

    /* Mem allocate */
    pCmd = malloc(sizeof(BleWifi_BLE_FSM_CmdQ_t) + u32Length);
    if (pCmd == NULL)
	{
        BLEWIFI_ERROR("BLEWIFI: BLE FSM task pCmd allocate fail\n");
	    goto error;
    }

    pCmd->u32EventId = u32EventId;
    pCmd->u32Length = u32Length;
    pCmd->fpCtrlAppCB = fpCB;
    if (u32Length > 0)
    {
        memcpy(pCmd->u8aData, pu8Data, u32Length);
    }

    // Front
    if (u32IsFront == BLEWIFI_QUEUE_FRONT)
    {
        if (osOK != osMessagePutFront(tBwBleCmdQueueId, (uint32_t)pCmd, 0))
        {
            printf("BLEWIFI: Ble FSM task cmd send fail. reset system\r\n");
            osDelay(3000);
            Hal_Sys_SwResetAll();
            goto error;
        }
    }
    // Back
    else
    {
        if (osOK != osMessagePut(tBwBleCmdQueueId, (uint32_t)pCmd, 0))
        {
            printf("BLEWIFI: Ble FSM task cmd send fail. reset system\r\n");
            osDelay(3000);
            Hal_Sys_SwResetAll();
            goto error;
        }
    }

    return 0;

error:
	if (pCmd != NULL)
	{
	    free(pCmd);
    }

	return -1;
}

static int32_t BleWifi_Ble_IsQueueEmpty(osMessageQId tBwBleCmdQueueId)
{
    osEvent rxEvent;

    // check queue is empty or not
    rxEvent = osMessagePeek(g_tBleCMDQ_Id , 0);

    if(rxEvent.status == osEventMessage) // queue is not empty
    {
        return false;
    }
    else // queue is empty
    {
        return true;
    }
}

static void _BwBleFsm_Remove_Pair_CMD(uint16_t u16CheckReq)
{
    osEvent rxEvent;
    BleWifi_BLE_FSM_CmdQ_t *pstQueryCmd;

    rxEvent = osMessagePeek(g_tBleCMDQ_Id , 0);

    if(rxEvent.status == osEventMessage)
    {
        pstQueryCmd = (BleWifi_BLE_FSM_CmdQ_t *)rxEvent.value.p;
        if((pstQueryCmd->u32EventId == u16CheckReq) && (g_u8BleCMDWaitingFlag ==BLEWIFI_BLE_EXEC_CMD_WAITING))
        {
            //execution CB
            if(pstQueryCmd->fpCtrlAppCB != NULL)
            {
                pstQueryCmd->fpCtrlAppCB();
            }

            memset(&rxEvent , 0 , sizeof(osEvent));
            rxEvent = osMessageGet(g_tBleCMDQ_Id , 0); // pop from queue
            free(rxEvent.value.p);

            g_u8BleCMDWaitingFlag = BLEWIFI_BLE_EXEC_CMD_CONTINUE;

            if(BleWifi_Ble_IsQueueEmpty(g_tBleCMDQ_Id) == false)  // cmdq is not empty
            {
                BleWifi_Ble_SendAppMsgToBle(BW_APP_MSG_BLE_EXEC_CMD, 0, NULL);
            }
        }
    }
}

static int32_t BleWifi_Ble_SetAdvtisingPara(UINT8 type, UINT8 own_addr_type, LE_BT_ADDR_T *peer_addr, UINT8 filter, UINT16 interval_min, UINT16 interval_max)
{
    LE_ERR_STATE rc;
    //uint8_t u8Ret = true;
	LE_GAP_ADVERTISING_PARAM_T para;

	para.interval_min = interval_min;
	para.interval_max = interval_max;
	para.type = type;
	para.own_addr_type = own_addr_type;

	if (peer_addr)
    {
	    para.peer_addr_type = peer_addr->type;
        MemCopy(para.peer_addr, peer_addr->addr, 6);
    }
    else
    {
	    para.peer_addr_type = LE_HCI_ADV_PEER_ADDR_PUBLIC;
		MemSet(para.peer_addr, 0, 6);
    }

	para.channel_map = 0x7;
    para.filter_policy = filter;

	rc = LeGapSetAdvParameter(&para);
    if (rc == SYS_ERR_SUCCESS)
    {
        return BLEWIFI_BLE_RET_SUCCESS;
    }
    else
    {
        //u8Ret = false;

        if (g_u8BleFsmState == BLEWIFI_BLE_STATE_INIT)
        {
            BLEWIFI_ERROR("[ERROR]BLEWIFI_CTRL_MSG_BLE_INIT fail\r\n");
#ifdef ERR_HANDLE
            BleWifi_Ctrl_MsgSend(BLEWIFI_CTRL_MSG_BLE_INIT_COMPLETE, (void *)&u8Ret, sizeof(uint8_t));
#endif
        }
        else
        {
            BLEWIFI_ERROR("[ERROR]SetAdvtisingPara fail rc = %x\r\n", rc);
#ifdef ERR_HANDLE
            BleWifi_Ctrl_MsgSend(BLEWIFI_CTRL_MSG_BLE_ADVERTISING_TIME_CHANGE_CFM, (void *)&u8Ret, sizeof(uint8_t));
#endif
        }
        return BW_BLE_CMD_FAILED;
    }
}

static int32_t BleWifi_Ble_CopyToBuf(UINT16 len, UINT8 *data)
{
    BLEWIFI_DATA_OUT_STORE_T *pstBleDataOut = &gTheBle.store;
    uint16_t u16CopyPos = 0;
    uint16_t u16CopySize = 0;
    uint16_t u16RemainCopyLen = len;
    uint8_t u8DivideCurrentCnt = 1; // start 1
    uint8_t u8DivideTotalCnt = 1;   // start 1

#if 0
    uint8_t u8aPrintBuf[256] = {0};
    uint8_t u8PrintPos = 0;

    u8PrintPos = 0;
    u8PrintPos = sprintf((char *)&u8aPrintBuf[u8PrintPos] , "Copy buffer : ");
    for (int j=0 ; j<len ; j++)
    {
        u8PrintPos += sprintf((char *)&u8aPrintBuf[u8PrintPos] , "%02X ", data[j]);
    }
    printf("%s\r\n" , u8aPrintBuf);
#endif

	if (g_u8BleFsmState != BLEWIFI_BLE_STATE_CONNECTED) return BLEWIFI_BLE_RET_FAIL;

    //printf("[ASH]len = %u\r\n" , len);

    if(len == 0 || data == NULL)
    {
        return BLEWIFI_BLE_RET_SUCCESS;
    }
    else
    {
        if((len % LE_GATT_DATA_OUT_BUF_BLOCK_SIZE) == 0)
        {
            u8DivideTotalCnt = (len / LE_GATT_DATA_OUT_BUF_BLOCK_SIZE);
        }
        else
        {
            u8DivideTotalCnt = (len / LE_GATT_DATA_OUT_BUF_BLOCK_SIZE) + 1;
        }
        //printf("[ASH]u8DivideTotalIdx = %u\r\n" , u8DivideTotalIdx);
        if(u8DivideTotalCnt >= LE_GATT_DATA_OUT_BUF_CNT)
        {
            printf("BLE buffer isn't enough\r\n");
            return BLEWIFI_BLE_RET_FAIL;
        }

        if(pstBleDataOut->u8ReadIdx > pstBleDataOut->u8WriteIdx)
        {
            if((pstBleDataOut->u8WriteIdx + u8DivideTotalCnt) >= pstBleDataOut->u8ReadIdx)
            {
                printf("BLE buffer isn't enough\r\n");
                return BLEWIFI_BLE_RET_FAIL;
            }
        }
        else if(pstBleDataOut->u8ReadIdx < pstBleDataOut->u8WriteIdx)
        {
            if(pstBleDataOut->u8WriteIdx + u8DivideTotalCnt >= LE_GATT_DATA_OUT_BUF_CNT &&
               ((pstBleDataOut->u8WriteIdx + u8DivideTotalCnt) % LE_GATT_DATA_OUT_BUF_CNT) >= pstBleDataOut->u8ReadIdx)
            {
                printf("BLE buffer isn't enough\r\n");
                return BLEWIFI_BLE_RET_FAIL;
            }
        }

        while(u16RemainCopyLen != 0)
        {
            pstBleDataOut->u8WriteIdx = (pstBleDataOut->u8WriteIdx+1) % LE_GATT_DATA_OUT_BUF_CNT;
            //printf("[ASH]pstBleDataOut->u8WriteIdx = %u\r\n" , pstBleDataOut->u8WriteIdx);
            if(u16RemainCopyLen <= LE_GATT_DATA_OUT_BUF_BLOCK_SIZE)
            {
                u16CopySize = u16RemainCopyLen;
            }
            else
            {
                u16CopySize = LE_GATT_DATA_OUT_BUF_BLOCK_SIZE;
            }
            //printf("[ASH]u16CopySize = %u\r\n" , u16CopySize);
            //printf("[ASH]u16CopyPos = %u\r\n" , u16CopyPos);
            //printf("[ASH]pstBleDataOut->u8WriteIdx * LE_GATT_DATA_OUT_BUF_BLOCK_SIZE = %u\r\n" , pstBleDataOut->u8WriteIdx * LE_GATT_DATA_OUT_BUF_BLOCK_SIZE);

            memcpy(&pstBleDataOut->buf[(pstBleDataOut->u8WriteIdx * LE_GATT_DATA_OUT_BUF_BLOCK_SIZE)] , &data[u16CopyPos] , u16CopySize);

            pstBleDataOut->u8CurrentPackageCnt[pstBleDataOut->u8WriteIdx] = u8DivideCurrentCnt;
            //printf("[ASH]pstBleDataOut->u8CurrentPackageCnt[pstBleDataOut->u8WriteIdx] = %u\r\n" , pstBleDataOut->u8CurrentPackageCnt[pstBleDataOut->u8WriteIdx]);
            pstBleDataOut->u8TotalPackageCnt[pstBleDataOut->u8WriteIdx] = u8DivideTotalCnt;
            //printf("[ASH]pstBleDataOut->u8TotalPackageCnt[pstBleDataOut->u8WriteIdx] = %u\r\n" , pstBleDataOut->u8TotalPackageCnt[pstBleDataOut->u8WriteIdx]);
            pstBleDataOut->u8BufDataLen[pstBleDataOut->u8WriteIdx] = u16CopySize;
            //printf("[ASH]pstBleDataOut->u8BufDataLen[pstBleDataOut->u8WriteIdx] = %u\r\n" , pstBleDataOut->u8BufDataLen[pstBleDataOut->u8WriteIdx]);
            u16CopyPos += u16CopySize;
            //printf("[ASH]u16CopyPos = %u\r\n" , u16CopyPos);
            u16RemainCopyLen = u16RemainCopyLen - u16CopySize;
            //printf("[ASH]u16RemainCopyLen = %u\r\n" , u16RemainCopyLen);
            u8DivideCurrentCnt ++;
            //printf("[ASH]u8DivideCurrentCnt = %u\r\n" , u8DivideCurrentCnt);
        }

        BleWifi_Ble_SendAppMsgToBle(BW_APP_MSG_SEND_TO_PEER, 0, NULL);
        return BLEWIFI_BLE_RET_SUCCESS;
    }
}

static int32_t BleWifi_Ble_SendToPeer(void)
{
    uint8_t u8SendingBuf[LE_GATT_DATA_SENDING_BUF_MAX_SIZE] = {0};
    BLEWIFI_DATA_OUT_STORE_T *pstBleDataOut = &gTheBle.store;
    uint16_t u16CopyPos = 0;
    uint16_t u16CopySize = 0;
    uint16_t u16RemainCopySize = 0;
    uint16_t u16SendingDataSize = 0;
    LE_ERR_STATE status;
    uint8_t u8LastDataCheckIdx = 0;

    u16RemainCopySize = gTheBle.curr_mtu - 3; // max mtu size

    do
    {
        if(pstBleDataOut->u8ReadIdx == pstBleDataOut->u8WriteIdx) // queue is empty
        {
            return BLEWIFI_BLE_RET_SUCCESS;
        }
        pstBleDataOut->u8ReadIdx = (pstBleDataOut->u8ReadIdx+1) % LE_GATT_DATA_OUT_BUF_CNT;
        //printf("[ASH]pstBleDataOut->u8ReadIdx = %u\r\n" , pstBleDataOut->u8ReadIdx);
        u16CopySize = pstBleDataOut->u8BufDataLen[pstBleDataOut->u8ReadIdx];
        //printf("[ASH]u16CopySize = %u\r\n" , u16CopySize);
        memcpy(&u8SendingBuf[u16CopyPos] , &pstBleDataOut->buf[(pstBleDataOut->u8ReadIdx * LE_GATT_DATA_OUT_BUF_BLOCK_SIZE)] , u16CopySize);
        u16CopyPos += u16CopySize;
        //printf("[ASH]u16CopyPos = %u\r\n" , u16CopyPos);
        u16SendingDataSize += u16CopySize;
        //printf("[ASH]u16SendingDataSize = %u\r\n" , u16SendingDataSize);
        u16RemainCopySize -= u16CopySize;
        //printf("[ASH]u16RemainCopySize = %u\r\n" , u16RemainCopySize);
    }while((pstBleDataOut->u8TotalPackageCnt[pstBleDataOut->u8ReadIdx] != pstBleDataOut->u8CurrentPackageCnt[pstBleDataOut->u8ReadIdx]) && (u16RemainCopySize >= LE_GATT_DATA_OUT_BUF_BLOCK_SIZE));

    if(pstBleDataOut->u8ReadIdx != pstBleDataOut->u8WriteIdx)
    {
        u8LastDataCheckIdx = (pstBleDataOut->u8ReadIdx+1) % LE_GATT_DATA_OUT_BUF_CNT;

        if((pstBleDataOut->u8TotalPackageCnt[u8LastDataCheckIdx] == (pstBleDataOut->u8CurrentPackageCnt[u8LastDataCheckIdx]))
        && (u16RemainCopySize >= pstBleDataOut->u8BufDataLen[u8LastDataCheckIdx])) // last package
        {
            pstBleDataOut->u8ReadIdx = (pstBleDataOut->u8ReadIdx+1) % LE_GATT_DATA_OUT_BUF_CNT;

            u16CopySize = pstBleDataOut->u8BufDataLen[pstBleDataOut->u8ReadIdx];
            memcpy(&u8SendingBuf[u16CopyPos] , &pstBleDataOut->buf[(pstBleDataOut->u8ReadIdx * LE_GATT_DATA_OUT_BUF_BLOCK_SIZE)] , u16CopySize);
            u16CopyPos += u16CopySize;
            u16SendingDataSize += u16CopySize;
            u16RemainCopySize -= u16CopySize;
        }
    }

#if 0
    uint8_t u8PrintPos = 0;
    uint8_t u8aPrintBuf[256] = {0};

    u8PrintPos = 0;
    u8PrintPos = sprintf((char *)&u8aPrintBuf[u8PrintPos] , "u8aSendBuf : ");
    for (int j=0 ; j<u16SendingDataSize ; j++)
    {
        u8PrintPos += sprintf((char *)&u8aPrintBuf[u8PrintPos] , "%02X ", u8SendingBuf[j]);
    }
    printf("%s\r\n" , u8aPrintBuf);
#endif

    status = LeGattCharValNotify(gTheBle.conn_hdl, pstBleDataOut->send_hdl, u16SendingDataSize, &u8SendingBuf[0]);

    if (status != SYS_ERR_SUCCESS)
    {
        printf("BLE sending data fail\r\n");
        return BLEWIFI_BLE_RET_FAIL;
    }

    return BLEWIFI_BLE_RET_SUCCESS;
}

int32_t BleWifi_Ble_PushToFSMCmdQ(uint32_t u32EventId , uint8_t *pu8Data, uint32_t u32Length , T_BleWifi_Ble_App_Ctrl_CB_Fp fpCB)
{
    if (BleWifi_Ble_Queue_Push(g_tBleCMDQ_Id , u32EventId , pu8Data , u32Length , fpCB , BLEWIFI_QUEUE_BACK) == -1)
    {
        printf("BleWifi_Ble_PushToFSMCmdQ BleWifi_Ble_Queue_Push fail \r\n");
        return -1;
    }

    BleWifi_Ble_SendAppMsgToBle(BW_APP_MSG_BLE_EXEC_CMD, 0, NULL);


    if(u32EventId == BW_BLE_FSM_ADVERTISING_START)
    {
        g_u8IsPushADVExitCmd = false;
    }
    else if(u32EventId == BW_BLE_FSM_ADVERTISING_EXIT)
    {
        g_u8IsPushADVExitCmd = true;
    }
    return BLEWIFI_BLE_RET_SUCCESS;
}

static int32_t BleWifi_Ble_FSM_MsgHandler_Initializing(uint32_t u32EvtType, void *data, int len)
{
    /* BLE Init Step 1: Do BLE initialization first */
    LeCmInit(&gTheBle.task);
    return BW_BLE_CMD_EXECUTING;
}
static int32_t BleWifi_Ble_FSM_MsgHandler_EnterAdvertising(uint32_t u32EvtType, void *data, int len)
{
    //uint8_t u8Ret = true;
    LE_ERR_STATE rc = LeGapAdvertisingEnable(TRUE);

    if (rc == SYS_ERR_SUCCESS)
    {
        return BW_BLE_CMD_EXECUTING;
    }
    else
    {
        BLEWIFI_INFO("APP-BLEWIFI_APP_MSG_ENTER_ADVERTISING fail rc = %x\r\n", rc);
#ifdef ERR_HANDLE
        u8Ret = false;
        BleWifi_Ctrl_MsgSend(BLEWIFI_CTRL_MSG_BLE_ADVERTISING_CFM, (void *)&u8Ret, sizeof(u8Ret));
#endif

        return BW_BLE_CMD_FAILED;
    }
}
static int32_t BleWifi_Ble_FSM_MsgHandler_ExitAdvertising(uint32_t u32EvtType, void *data, int len)
{
    //uint8_t u8Ret = true;
    LE_ERR_STATE rc = LeGapAdvertisingEnable(FALSE);

    if (rc == SYS_ERR_SUCCESS)
    {
        return BW_BLE_CMD_EXECUTING;
    }
    else
    {
        BLEWIFI_INFO("APP-BLEWIFI_APP_MSG_EXIT_ADVERTISING fail rc = %x\r\n", rc);
#ifdef ERR_HANDLE
        u8Ret = false;
        BleWifi_Ctrl_MsgSend(BLEWIFI_CTRL_MSG_BLE_ADVERTISING_EXIT_CFM, (void *)&u8Ret, sizeof(u8Ret));
#endif

        return BW_BLE_CMD_FAILED;
    }
}

static int32_t BleWifi_Ble_FSM_MsgHandler_ChangeAdvertisingTime(uint32_t u32EvtType, void *data, int len)
{
    uint16_t u16AdvInterval = 0;
    osEvent rxEvent;
    BleWifi_BLE_FSM_CmdQ_t *pstCmd = NULL;
    pstCmd = (BleWifi_BLE_FSM_CmdQ_t *)rxEvent.value.p;

    if(data == NULL)
    {
        printf("Adv data is NULL\r\n");

        return BW_BLE_CMD_FAILED;
    }
    else
    {
        u16AdvInterval = *(uint16_t *)data;
    }
    rxEvent = osMessagePeek(g_tBleCMDQ_Id , 0);

    pstCmd = (BleWifi_BLE_FSM_CmdQ_t *)rxEvent.value.p;

    if((g_u8BleFsmState == BLEWIFI_BLE_STATE_INIT) || (g_u8BleFsmState == BLEWIFI_BLE_STATE_IDLE) || (g_u8BleFsmState == BLEWIFI_BLE_STATE_CONNECTED))
    {
        /* Call BLE Stack API to change advertising time */
        gTheBleAdvTime.interval_min = u16AdvInterval;
        gTheBleAdvTime.interval_max = u16AdvInterval;

        if(BLEWIFI_BLE_RET_SUCCESS == BleWifi_Ble_SetAdvtisingPara(LE_HCI_ADV_TYPE_ADV_IND,
                                                                LE_HCI_OWN_ADDR_PUBLIC,
                                                                0,
                                                                LE_HCI_ADV_FILT_NONE,
                                                                gTheBleAdvTime.interval_min,
                                                                gTheBleAdvTime.interval_max))
        {
            return BW_BLE_CMD_EXECUTING;
        }
        else
        {
            return BW_BLE_CMD_FAILED;
        }
    }
    else if(g_u8BleFsmState == BLEWIFI_BLE_STATE_ADVERTISING)
    {
        // In BLEWIFI_BLE_STATE_ADVERTISING state BLEWIFI_BLE_FSM_ADVERTISING_TIME_CHANGE will split 3 commands
        // Exec sequence : ExitADV -> ChangeADVTime -> EnterADV , if we want to inset to the cmdQ , need to inverse.

        memset(&rxEvent , 0 , sizeof(osEvent));
        rxEvent = osMessageGet(g_tBleCMDQ_Id , 0);

        if (BleWifi_Ble_Queue_Push(g_tBleCMDQ_Id , BW_BLE_FSM_ADVERTISING_START , NULL , 0 , NULL , BLEWIFI_QUEUE_FRONT) == -1)
        {
            printf("ChangeAdvertisingTime push fail \r\n");
        }

        if (BleWifi_Ble_Queue_Push(g_tBleCMDQ_Id , BW_BLE_FSM_ADVERTISING_TIME_CHANGE , (uint8_t *)&u16AdvInterval , sizeof(u16AdvInterval) , pstCmd->fpCtrlAppCB , BLEWIFI_QUEUE_FRONT) == -1)
        {
            printf("ChangeAdvertisingTime push fail \r\n");
        }

        if (BleWifi_Ble_Queue_Push(g_tBleCMDQ_Id , BW_BLE_FSM_ADVERTISING_EXIT , NULL , 0 , NULL , BLEWIFI_QUEUE_FRONT) == -1)
        {
            printf("ChangeAdvertisingTime push fail \r\n");
        }

        memset(&rxEvent , 0 , sizeof(osEvent));
        rxEvent = osMessagePeek(g_tBleCMDQ_Id , 0);

        if(rxEvent.status == osEventMessage) // queue is not empty
        {
            BleWifi_Ble_SendAppMsgToBle(BW_APP_MSG_BLE_EXEC_CMD, 0, NULL);
        }

    }
    return BLEWIFI_BLE_RET_SUCCESS;
}

static int32_t BleWifi_Ble_FSM_MsgHandler_Disconnection(uint32_t u32EvtType, void *data, int len)
{
    LE_ERR_STATE rc;
    //uint8_t u8Ret = true;

    /* Disconnect BLE connection */
    rc = LeGapDisconnectReq(gTheBle.conn_hdl);
    if (rc == SYS_ERR_SUCCESS)
    {
        return BW_BLE_CMD_EXECUTING;
    }
    else
    {
        BLEWIFI_WARN("[%s][%d]BleWifi_Ble_AppMsgHandler_Disconnection fail rc = %x\r\n", __func__ , __LINE__ , rc);
#ifdef ERR_HANDLE
        u8Ret = false;
        BleWifi_Ctrl_MsgSend(BLEWIFI_CTRL_MSG_BLE_DISCONNECT, (void *)&u8Ret , 0);
#endif

        return BW_BLE_CMD_FAILED;
    }
}

static int32_t BleWifi_Ble_FSM_MsgHandler_ChangeADVData(uint32_t u32EvtType, void *data, int len)
{
    LE_ERR_STATE rc;

    gTheBle.adv_data.len = len;

    memset(gTheBle.adv_data.buf, 0, sizeof(gTheBle.adv_data.buf));
    MemCopy(gTheBle.adv_data.buf, data, len);

    rc = LeGapSetAdvData(gTheBle.adv_data.len, gTheBle.adv_data.buf);
    if (rc == SYS_ERR_SUCCESS)
    {
        return BW_BLE_CMD_EXECUTING;
    }
    else
    {
        return BW_BLE_CMD_FAILED;
    }
}

static int32_t BleWifi_Ble_FSM_MsgHandler_ChangeScanData(uint32_t u32EvtType, void *data, int len)
{
    LE_ERR_STATE rc;

    gTheBle.scn_data.len = len;

    memset(gTheBle.scn_data.buf, 0, sizeof(gTheBle.scn_data.buf));
    MemCopy(gTheBle.scn_data.buf, data, len);

    BleWifi_Ble_AppUpdateDevName(gTheBle.scn_data.buf + 2, gTheBle.scn_data.len - 2);
    rc = LeSetScanRspData(gTheBle.scn_data.len, gTheBle.scn_data.buf);
    if (rc == SYS_ERR_SUCCESS)
    {
        return BW_BLE_CMD_EXECUTING;
    }
    else
    {
        return BW_BLE_CMD_FAILED;
    }
}

static int32_t BleWifi_Ble_FSM_MsgHandler_BleStop(uint32_t u32EvtType, void *data, int len)
{
    //special case , This cmd doesn't execute actually
    g_u8BleCMDWaitingFlag = BLEWIFI_BLE_EXEC_CMD_WAITING;

    _BwBleFsm_Remove_Pair_CMD(BW_BLE_FSM_BLE_STOP);

    return BW_BLE_CMD_FINISH;
}

static int32_t BleWifi_Ble_FSM_MsgHandler_InitComplete_Cfm(uint32_t u32EvtType, void *data, int len)
{
	BLE_ADV_SCAN_T	stAdvData = {0};
    BLE_ADV_SCAN_T	stScanData = {0};
    //BLE_CMD_T *pstInsertCmd;
    //osEvent rxEvent;

    // !!! after LeCmInit
    BLEWIFI_INFO("APP-LE_CM_MSG_INIT_COMPLETE_CFM\r\n");

    LeGattInit(&gTheBle.task);
    LeSmpInit(&gTheBle.task);
    LeSmpSetDefaultConfig(LE_SM_IO_CAP_NO_IO, FALSE, FALSE, TRUE);

    //BleWifi_Ble_SetAdvData();
    //BleWifi_Ble_SetScanData();
    /*BleWifi_Ble_SetAdvtisingPara(LE_HCI_ADV_TYPE_ADV_IND,
                                 LE_HCI_OWN_ADDR_PUBLIC,
                                 0,
                                 LE_HCI_ADV_FILT_NONE,
                                 BLEWIFI_BLE_ADVERTISEMENT_INTERVAL_INIT_MIN,
                                 BLEWIFI_BLE_ADVERTISEMENT_INTERVAL_INIT_MAX);*/

    _BwBleFsm_Remove_Pair_CMD(BW_BLE_FSM_INITIALIZING);

    BleWifi_Ble_InitAdvData((uint8_t *)stAdvData.buf , (uint8_t *)&stAdvData.len);
    if (BleWifi_Ble_Queue_Push(g_tBleCMDQ_Id , BW_BLE_FSM_CHANGE_ADVERTISING_DATA , (uint8_t *)&stAdvData.buf , (uint32_t)stAdvData.len , NULL , BLEWIFI_QUEUE_FRONT) == -1)
    {
        printf("BW_BLE_FSM_CHANGE_ADVERTISING_DATA push fail \r\n");
    }

    BleWifi_Ble_InitScanData((uint8_t *)stScanData.buf , (uint8_t *)&stScanData.len);
    if (BleWifi_Ble_Queue_Push(g_tBleCMDQ_Id , BW_BLE_FSM_CHANGE_SCAN_DATA , (uint8_t *)&stScanData.buf , (uint32_t)stScanData.len , NULL , BLEWIFI_QUEUE_FRONT) == -1)
    {
        printf("BW_BLE_FSM_CHANGE_SCAN_DATA push fail \r\n");
    }

    return BW_BLE_CMD_FINISH;
}

static int32_t BleWifi_Ble_FSM_MsgHandler_EnterAdvertising_Cfm(uint32_t u32EvtType, void *data, int len)
{
    uint8_t u8Ret = *((uint8_t *)data);

    if(u8Ret == true)
    {
        g_u8BleFsmState = BLEWIFI_BLE_STATE_ADVERTISING;
    }
    else
    {
        BLEWIFI_ERROR("ADV cfm Return fail.\r\n");
    }

    _BwBleFsm_Remove_Pair_CMD(BW_BLE_FSM_ADVERTISING_START);

    return BW_BLE_CMD_FINISH;
}

static int32_t BleWifi_Ble_FSM_MsgHandler_ExitAdvertising_Cfm(uint32_t u32EvtType, void *data, int len)
{
    uint8_t u8Ret = *((uint8_t *)data);

    if(u8Ret == true)
    {
        g_u8BleFsmState = BLEWIFI_BLE_STATE_IDLE;
    }
    else
    {
        if(g_u8BleFsmState == BLEWIFI_BLE_STATE_CONNECTED)
        {
            BLEWIFI_INFO("The Connect event had executed before exitADV CFM \r\n");
        }
        else
        {
            BLEWIFI_ERROR("exitADV CFM fail\r\n");
        }
    }
    _BwBleFsm_Remove_Pair_CMD(BW_BLE_FSM_ADVERTISING_EXIT);

    return BW_BLE_CMD_FINISH;
}

static int32_t BleWifi_Ble_FSM_MsgHandler_ChangeAdvertisingTime_Cfm(uint32_t u32EvtType, void *data, int len)
{
    uint8_t u8Ret = *((uint8_t *)data);
    //BLE_CMD_T *pstQueryCmd = {0};
    //osEvent rxEvent;

    // !!! Init complete
    if (g_u8BleFsmState == BLEWIFI_BLE_STATE_INIT)
    {
    	if(u8Ret == false)
    	{
#ifdef ERR_HANDLE
#endif
    	}
        else
        {
            g_u8BleFsmState = BLEWIFI_BLE_STATE_IDLE;
        }
    }
    else
    {
    	if(u8Ret == false)
    	{
			BLEWIFI_ERROR("[ERROR]ChangeAdvertisingTime_Cfm fail\r\n");
#ifdef ERR_HANDLE
#endif
    	}
        else
        {
#ifdef TBD_CBK
            //BleWifi_Ctrl_MsgSend(BLEWIFI_CTRL_MSG_BLE_ADVERTISING_TIME_CHANGE_CFM, (void *)&u8Ret , sizeof(uint8_t));
#endif
        }
    }

    _BwBleFsm_Remove_Pair_CMD(BW_BLE_FSM_ADVERTISING_TIME_CHANGE);

    return BW_BLE_CMD_FINISH;
}

static int32_t BleWifi_Ble_FSM_MsgHandler_Disconnect_Cfm(uint32_t u32EvtType, void *data, int len)
{
    // !!! [Device] after LeGapDisconnectReq
    // !!! [Peer] request the disconnection
    BLEWIFI_DATA_OUT_STORE_T *s = &gTheBle.store;
    uint32_t *pu32Reason = (uint32_t *)data;

    s->u8WriteIdx = 0;
    s->u8ReadIdx = 0;
    memset(s->u8CurrentPackageCnt , 0 ,sizeof(s->u8CurrentPackageCnt));
    memset(s->u8TotalPackageCnt , 0 ,sizeof(s->u8TotalPackageCnt));
    memset(s->u8BufDataLen , 0 ,sizeof(s->u8BufDataLen));
    memset(s->buf , 0 ,sizeof(s->buf));

    g_u8BleFsmState = BLEWIFI_BLE_STATE_IDLE;

    printf("Disconnect reason = %x \r\n", *pu32Reason);

    BleWifi_Ble_Disconnected_CB(*pu32Reason);

    if(g_u8IsPushADVExitCmd == false) // start adv
    {
        if (BleWifi_Ble_Queue_Push(g_tBleCMDQ_Id , BW_BLE_FSM_ADVERTISING_START , NULL , 0 , NULL , BLEWIFI_QUEUE_BACK) == -1)
        {
            printf("BLEWIFI : Connection_Fail BleWifi_Ble_Queue_Push fail \r\n");
        }
        BleWifi_Ble_SendAppMsgToBle(BW_APP_MSG_BLE_EXEC_CMD, 0, NULL);
    }



///   if(cmdq has disconnection cmd)
//        return BLEWIFI_BLE_RET_SUCCESS;
//    else
//        return BLEWIFI_BLE_RET_SUCCESS;

    _BwBleFsm_Remove_Pair_CMD(BW_BLE_FSM_DISCONNECT);

    return BW_BLE_CMD_FINISH;
}


static int32_t BleWifi_Ble_FSM_MsgHandler_Connection_Success(uint32_t u32EvtType, void *data, int len)
{
    LE_CM_CONNECTION_COMPLETE_IND_T *ind = (LE_CM_CONNECTION_COMPLETE_IND_T *)data;

    gTheBle.conn_hdl = ind->conn_hdl;
    gTheBle.bt_addr.type = ind->peer_addr_type;
    MemCopy(gTheBle.bt_addr.addr, ind->peer_addr, 6);

    gTheBle.max_itvl = ind->conn_interval;
    gTheBle.latency = ind->conn_latency;
    gTheBle.sv_tmo = ind->supervison_timeout;

    BleWifi_Ble_GattIndicateServiceChange(ind->conn_hdl);

    g_u8BleFsmState = BLEWIFI_BLE_STATE_CONNECTED;

    BleWifi_Ble_Connected_CB();

    return BW_BLE_CMD_FINISH;
}
static int32_t BleWifi_Ble_FSM_MsgHandler_Connection_Fail(uint32_t u32EvtType, void *data, int len)
{
    g_u8BleFsmState = BLEWIFI_BLE_STATE_IDLE;

    if(g_u8IsPushADVExitCmd == false)
    {
        if (BleWifi_Ble_Queue_Push(g_tBleCMDQ_Id , BW_BLE_FSM_ADVERTISING_START, NULL , 0 , NULL , BLEWIFI_QUEUE_BACK) == -1)
        {
            printf("BLEWIFI : Connection_Fail BleWifi_Ble_Queue_Push fail \r\n");
        }
    }
#ifdef TBD_CBK
    //BleWifi_Ctrl_EventStatusSet(BLEWIFI_CTRL_EVENT_BIT_BLE_ADVTISING, false);
#endif
    return BW_BLE_CMD_FINISH;
}

static int32_t BleWifi_Ble_FSM_MsgHandler_SEND_DATA(uint32_t u32EvtType, void *data, int len)
{
    BLEWIFI_MESSAGE_T *wifi_data = (BLEWIFI_MESSAGE_T *)data;

    // copy data to buffer
    BleWifi_Ble_CopyToBuf(wifi_data->len, wifi_data->data);

    return BW_BLE_CMD_FINISH;
}
static int32_t BleWifi_Ble_FSM_MsgHandler_SEND_TO_PEER(uint32_t u32EvtType, void *data, int len)
{
    // send data from buffer to peer
    BleWifi_Ble_SendToPeer();

    return BW_BLE_CMD_FINISH;
}
static int32_t BleWifi_Ble_FSM_MsgHandler_SEND_TO_PEER_CFM(uint32_t u32EvtType, void *data, int len)
{
    // trigger the next "send data from buffer to peer"
    BleWifi_Ble_SendAppMsgToBle(BW_APP_MSG_SEND_TO_PEER, 0, NULL);

    return BW_BLE_CMD_FINISH;
}

static int32_t BleWifi_Ble_FSM_MsgHandler_SetADVData_CFM(uint32_t u32EvtType, void *data, int len)
{
    _BwBleFsm_Remove_Pair_CMD(BW_BLE_FSM_CHANGE_ADVERTISING_DATA);

    return BW_BLE_CMD_FINISH;
}
static int32_t BleWifi_Ble_FSM_MsgHandler_SetScanData_CFM(uint32_t u32EvtType, void *data, int len)
{
    _BwBleFsm_Remove_Pair_CMD(BW_BLE_FSM_CHANGE_SCAN_DATA);

    return BW_BLE_CMD_FINISH;
}
static int32_t BleWifi_Ble_FSM_MsgHandler_SignalUpdate_REQ(uint32_t u32EvtType, void *data, int len)
{
    LE_CM_MSG_SIGNAL_UPDATE_REQ_T *req = (LE_CM_MSG_SIGNAL_UPDATE_REQ_T *)data;

    LeGapConnUpdateResponse(req->conn_hdl, req->identifier, TRUE);

    return BW_BLE_CMD_FINISH;
}
static int32_t BleWifi_Ble_FSM_MsgHandler_ConnPara_REQ(uint32_t u32EvtType, void *data, int len)
{
    LE_CM_MSG_CONN_PARA_REQ_T *req = (LE_CM_MSG_CONN_PARA_REQ_T *)data;

    LeGapConnParaRequestRsp(req->conn_hdl, TRUE);

    return BW_BLE_CMD_FINISH;
}
static int32_t BleWifi_Ble_FSM_MsgHandler_ConnUpdateComplete_CFM(uint32_t u32EvtType, void *data, int len)
{
    LE_CM_MSG_CONN_UPDATE_COMPLETE_IND_T *ind = (LE_CM_MSG_CONN_UPDATE_COMPLETE_IND_T *)data;

    if (ind->status == SYS_ERR_SUCCESS)
    {
        gTheBle.max_itvl = ind->interval;
        gTheBle.latency = ind->latency;
        gTheBle.sv_tmo = ind->supervision_timeout;
    }
    else
    {
        LeGapDisconnectReq(ind->conn_hdl);
    }

    return BW_BLE_CMD_FINISH;
}
static int32_t BleWifi_Ble_FSM_MsgHandler_SetDisconnection_CFM(uint32_t u32EvtType, void *data, int len)
{
    return BW_BLE_CMD_FINISH;
}

static int32_t BleWifi_Ble_FSM_MsgHandler(T_BleWifi_Ble_FSM_MsgHandlerTbl tHanderTbl[], uint32_t u32EvtType , void *data , int len)
{
    uint32_t i = 0;
    //BleWifi_BLE_FSM_CmdQ_t *pstQueryCmd = {0};
    //uint8_t u8IsExecNextCmd = 0;
    osEvent rxEvent;
    int8_t u8Status = BW_BLE_CMD_NOT_MATCH;

    while (tHanderTbl[i].u32EventId != BW_BLE_FSM_END)
    {
        // match
        if (tHanderTbl[i].u32EventId == u32EvtType)
        {
            u8Status = tHanderTbl[i].fpFunc(u32EvtType , data , len);
            break;
        }
        i++;
    }

    //printf("tHanderTbl[i].u32EventId = 0x%x , u8Status = 0x%x , g_u8BleFsmState = %u , g_u8BleCMDWaitingFlag = %u\r\n" , tHanderTbl[i].u32EventId , u8Status , g_u8BleFsmState , g_u8BleCMDWaitingFlag);

    if(u8Status == BW_BLE_CMD_FINISH)
    {

    }
    else if(u8Status == BW_BLE_CMD_EXECUTING)
    {
        g_u8BleCMDWaitingFlag = BLEWIFI_BLE_EXEC_CMD_WAITING;
    }
    else if(u8Status == BW_BLE_CMD_FAILED || u8Status == BW_BLE_CMD_NOT_MATCH)
    {
        if(u32EvtType > BW_BLE_FSM_ACTIVE_START && u32EvtType < BW_BLE_FSM_ACTIVE_END)
        {
            //remove cmd
            memset(&rxEvent , 0 , sizeof(osEvent));
            rxEvent = osMessageGet(g_tBleCMDQ_Id , 0); // pop from queue
            free(rxEvent.value.p);

            g_u8BleCMDWaitingFlag = BLEWIFI_BLE_EXEC_CMD_CONTINUE;
            if(BleWifi_Ble_IsQueueEmpty(g_tBleCMDQ_Id) == false)  // cmdq is not empty
            {
                BleWifi_Ble_SendAppMsgToBle(BW_APP_MSG_BLE_EXEC_CMD, 0, NULL);
            }

        }
    }
    return BLEWIFI_BLE_RET_SUCCESS;
}


int32_t BleWifi_Ble_FSM(uint32_t u32EvtType , void *data , int len)
{
    //printf("The BLE FSM state is %u  id = 0x%x\n",g_u8BleFsmState,u32EvtType);
    return BleWifi_Ble_FSM_MsgHandler(g_pstBleFSMStateTbl[g_u8BleFsmState] , u32EvtType , data , len);
}

int32_t BleWifi_Ble_FSM_Init(void)
{
    BLEWIFI_INFO("APP-BleWifi_Ble_ServerAppInit\r\n");

	MemSet(&gTheBle, 0, sizeof(gTheBle));
	MemSet(&gTheBleAdvTime, 0, sizeof(gTheBleAdvTime));

	gTheBle.curr_mtu = 23;

	gTheBle.min_itvl = DEFAULT_DESIRED_MIN_CONN_INTERVAL;
	gTheBle.max_itvl = DEFAULT_DESIRED_MAX_CONN_INTERVAL;
	gTheBle.latency  = DEFAULT_DESIRED_SLAVE_LATENCY;
	gTheBle.sv_tmo   = DEFAULT_DESIRED_SUPERVERSION_TIMEOUT;

    LeHostCreateTask(&gTheBle.task, BleWifi_Ble_TaskHandler);

    g_u8BleFsmState = BLEWIFI_BLE_STATE_INIT;
    //g_stADVTime.interval_min = BLEWIFI_BLE_ADVERTISEMENT_INTERVAL_INIT_MIN;
    //g_stADVTime.interval_min = BLEWIFI_BLE_ADVERTISEMENT_INTERVAL_INIT_MAX;

    return BLEWIFI_BLE_RET_SUCCESS;
}

