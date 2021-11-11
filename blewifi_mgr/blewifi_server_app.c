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

#include "ble_hci_if.h"
#include "ble_cm_if.h"
#include "ble_smp_if.h"
#include "ble_gap_if.h"
#include "ble_gatt_if.h"

#include "blewifi_common.h"
#include "blewifi_configuration.h"
#include "app_configuration.h"
#include "blewifi_server_app.h"
#include "blewifi_server_app_gatt.h"

#include "mw_fim_default_group11_project.h"
#include "hal_os_patch.h"
#include "blewifi_ble_FSM.h"
#include "blewifi_ble_api.h"

extern LE_ERR_STATE LeGapGetBdAddr(BD_ADDR addr);
extern uint8_t g_u8BleCMDWaitingFlag;

extern T_BleWifi_Ble_MsgHandlerTbl gBleGattMsgHandlerTbl[];

BLE_APP_DATA_T gTheBle = {0};
extern osMessageQId g_tBleCMDQ_Id;

static int32_t BleWifi_Ble_SmMsgHandler_PairingActionInd(TASK task, MESSAGEID id, MESSAGE message);
static int32_t BleWifi_Ble_SmMsgHandler_EncryptionChangeInd(TASK task, MESSAGEID id, MESSAGE message);
static int32_t BleWifi_Ble_SmMsgHandler_EncryptionRefreshInd(TASK task, MESSAGEID id, MESSAGE message);
static int32_t BleWifi_Ble_SmMsgHandler_PairingCompleteInd(TASK task, MESSAGEID id, MESSAGE message);
T_BleWifi_Ble_MsgHandlerTbl gBleSmMsgHandlerTbl[] =
{
    {LE_SMP_MSG_PAIRING_ACTION_IND,     BleWifi_Ble_SmMsgHandler_PairingActionInd},
    {LE_SMP_MSG_ENCRYPTION_CHANGE_IND,  BleWifi_Ble_SmMsgHandler_EncryptionChangeInd},
    {LE_SMP_MSG_ENCRYPTION_REFRESH_IND, BleWifi_Ble_SmMsgHandler_EncryptionRefreshInd},
    {LE_SMP_MSG_PAIRING_COMPLETE_IND,   BleWifi_Ble_SmMsgHandler_PairingCompleteInd},

    {0xFFFFFFFF,                        NULL}
};

static int32_t BleWifi_Ble_CmMsgHandler_InitCompleteCfm(TASK task, MESSAGEID id, MESSAGE message);
static int32_t BleWifi_Ble_CmMsgHandler_SetAdvertisingDataCfm(TASK task, MESSAGEID id, MESSAGE message);
static int32_t BleWifi_Ble_CmMsgHandler_SetScanRspDataCfm(TASK task, MESSAGEID id, MESSAGE message);
static int32_t BleWifi_Ble_CmMsgHandler_SetAdvertisingParamsCfm(TASK task, MESSAGEID id, MESSAGE message);
static int32_t BleWifi_Ble_CmMsgHandler_EnterAdvertisingCfm(TASK task, MESSAGEID id, MESSAGE message);
static int32_t BleWifi_Ble_CmMsgHandler_ExitAdvertisingCfm(TASK task, MESSAGEID id, MESSAGE message);
static int32_t BleWifi_Ble_CmMsgHandler_ConnectionCompleteInd(TASK task, MESSAGEID id, MESSAGE message);
static int32_t BleWifi_Ble_CmMsgHandler_SignalUpdateReq(TASK task, MESSAGEID id, MESSAGE message);
static int32_t BleWifi_Ble_CmMsgHandler_ConnParaReq(TASK task, MESSAGEID id, MESSAGE message);
static int32_t BleWifi_Ble_CmMsgHandler_ConnUpdateCompleteInd(TASK task, MESSAGEID id, MESSAGE message);
static int32_t BleWifi_Ble_CmMsgHandler_SetDisconnectCfm(TASK task, MESSAGEID id, MESSAGE message);
static int32_t BleWifi_Ble_CmMsgHandler_DisconnectCompleteInd(TASK task, MESSAGEID id, MESSAGE message);
T_BleWifi_Ble_MsgHandlerTbl gBleCmMsgHandlerTbl[] =
{
    {LE_CM_MSG_INIT_COMPLETE_CFM,           BleWifi_Ble_CmMsgHandler_InitCompleteCfm},
    {LE_CM_MSG_SET_ADVERTISING_DATA_CFM,    BleWifi_Ble_CmMsgHandler_SetAdvertisingDataCfm},
    {LE_CM_MSG_SET_SCAN_RSP_DATA_CFM,       BleWifi_Ble_CmMsgHandler_SetScanRspDataCfm},
    {LE_CM_MSG_SET_ADVERTISING_PARAMS_CFM,  BleWifi_Ble_CmMsgHandler_SetAdvertisingParamsCfm},
    {LE_CM_MSG_ENTER_ADVERTISING_CFM,       BleWifi_Ble_CmMsgHandler_EnterAdvertisingCfm},
    {LE_CM_MSG_EXIT_ADVERTISING_CFM,        BleWifi_Ble_CmMsgHandler_ExitAdvertisingCfm},
    {LE_CM_CONNECTION_COMPLETE_IND,         BleWifi_Ble_CmMsgHandler_ConnectionCompleteInd},
    {LE_CM_MSG_SIGNAL_UPDATE_REQ,           BleWifi_Ble_CmMsgHandler_SignalUpdateReq},
    {LE_CM_MSG_CONN_PARA_REQ,               BleWifi_Ble_CmMsgHandler_ConnParaReq},
    {LE_CM_MSG_CONN_UPDATE_COMPLETE_IND,    BleWifi_Ble_CmMsgHandler_ConnUpdateCompleteInd},
    {LE_CM_MSG_SET_DISCONNECT_CFM,          BleWifi_Ble_CmMsgHandler_SetDisconnectCfm},
    {LE_CM_MSG_DISCONNECT_COMPLETE_IND,     BleWifi_Ble_CmMsgHandler_DisconnectCompleteInd},

    {0xFFFFFFFF,                            NULL}
};

static int32_t BleWifi_Ble_AppMsgHandler_SendData(TASK task, MESSAGEID id, MESSAGE message);
static int32_t BleWifi_Ble_AppMsgHandler_SendToPeer(TASK task, MESSAGEID id, MESSAGE message);
static int32_t BleWifi_Ble_AppMsgHandler_SendToPeerCfm(TASK task, MESSAGEID id, MESSAGE message);
static int32_t BleWifi_Ble_AppMsgHandler_BleCommand_Exec(TASK task, MESSAGEID id, MESSAGE message);
T_BleWifi_Ble_MsgHandlerTbl gBleAppMsgHandlerTbl[] =
{
    {BW_APP_MSG_SEND_DATA,                 BleWifi_Ble_AppMsgHandler_SendData},
    {BW_APP_MSG_SEND_TO_PEER,              BleWifi_Ble_AppMsgHandler_SendToPeer},
    {BW_APP_MSG_SEND_TO_PEER_CFM,          BleWifi_Ble_AppMsgHandler_SendToPeerCfm},
    {BW_APP_MSG_BLE_EXEC_CMD,              BleWifi_Ble_AppMsgHandler_BleCommand_Exec},

    {0xFFFFFFFF,                                NULL}
};

static int32_t BleWifi_Ble_MsgHandler(T_BleWifi_Ble_MsgHandlerTbl tHanderTbl[], TASK task, MESSAGEID id, MESSAGE message)
{
    uint32_t i = 0;

    while (tHanderTbl[i].ulEventId != 0xFFFFFFFF)
    {
        // match
        if (tHanderTbl[i].ulEventId == id)
        {
            tHanderTbl[i].fpFunc(task, id, message);
            break;
        }

        i++;
    }

    // not match
    if (tHanderTbl[i].ulEventId == 0xFFFFFFFF)
    {
    }
    return BLEWIFI_BLE_RET_SUCCESS;
}

void BleWifi_Ble_TaskHandler(TASK task, MESSAGEID id, MESSAGE message)
{
    osSemaphoreWait(g_tBleInternalSemaphoreId, osWaitForever);

	if ((id >= LE_GATT_MSG_BASE) && (id < LE_GATT_MSG_TOP))
	{
		BleWifi_Ble_MsgHandler(gBleGattMsgHandlerTbl, task, id, message);
    }
    else if ((id >= LE_SMP_MSG_BASE) && (id < LE_SMP_MSG_TOP))
	{
        BleWifi_Ble_MsgHandler(gBleSmMsgHandlerTbl, task, id, message);
    }
	else if ((id >= LE_CM_MSG_BASE) && (id < LE_CM_MSG_TOP))
	{
        BleWifi_Ble_MsgHandler(gBleCmMsgHandlerTbl, task, id, message);
    }
    else if ((id >= BW_APP_MSG_BASE) && (id < BW_APP_MSG_TOP))
    {
        BleWifi_Ble_MsgHandler(gBleAppMsgHandlerTbl, task, id, message);
    }

    osSemaphoreRelease(g_tBleInternalSemaphoreId);
}

static int32_t BleWifi_Ble_SmMsgHandler_PairingActionInd(TASK task, MESSAGEID id, MESSAGE message)
{
    LE_SMP_MSG_PAIRING_ACTION_IND_T *ind = (LE_SMP_MSG_PAIRING_ACTION_IND_T *)message;
    BLEWIFI_INFO("APP-LE_SMP_MSG_PAIRING_ACTION_IND hdl = %x sc = %d action = %d\r\n", ind->conn_hdl, ind->sc, ind->action);

    LeSmpSecurityRsp(ind->conn_hdl, TRUE);

    return 0;
}

static int32_t BleWifi_Ble_SmMsgHandler_EncryptionChangeInd(TASK task, MESSAGEID id, MESSAGE message)
{
    LE_SMP_MSG_ENCRYPTION_CHANGE_IND_T *ind = (LE_SMP_MSG_ENCRYPTION_CHANGE_IND_T *)message;

    BLEWIFI_INFO("APP-LE_SMP_MSG_ENCRYPTION_CHANGE_IND enable = %d\r\n", ind->enable);
    gTheBle.encrypted = ind->enable;

    return 0;
}

static int32_t BleWifi_Ble_SmMsgHandler_EncryptionRefreshInd(TASK task, MESSAGEID id, MESSAGE message)
{
#ifdef BLEWIFI_SHOW_INFO
    LE_SMP_MSG_ENCRYPTION_REFRESH_IND_T *ind = (LE_SMP_MSG_ENCRYPTION_REFRESH_IND_T *)message;
    BLEWIFI_INFO("APP-LE_SMP_MSG_ENCRYPTION_REFRESH_IND status = %x\r\n", ind->status);
#endif

    return 0;
}

static int32_t BleWifi_Ble_SmMsgHandler_PairingCompleteInd(TASK task, MESSAGEID id, MESSAGE message)
{
    LE_SMP_MSG_PAIRING_COMPLETE_IND_T *ind = (LE_SMP_MSG_PAIRING_COMPLETE_IND_T *)message;

    BLEWIFI_INFO("APP-LE_SMP_MSG_PAIRING_COMPLETE_IND status = %x\r\n", ind->status);

    if (ind->status == SYS_ERR_SUCCESS)
    {
        gTheBle.paired = TRUE;
    }

    return 0;
}

static int32_t BleWifi_Ble_CmMsgHandler_InitCompleteCfm(TASK task, MESSAGEID id, MESSAGE message)
{
    BleWifi_Ble_FSM(BW_BLE_FSM_INIT_COMPLETE , NULL , 0);

    return 0;
}

static int32_t BleWifi_Ble_CmMsgHandler_SetAdvertisingDataCfm(TASK task, MESSAGEID id, MESSAGE message)
{
    BLEWIFI_INFO("APP-LE_CM_MSG_SET_ADVERTISING_DATA_CFM - Status = %x\r\n", ((LE_CM_MSG_SET_ADVERTISING_DATA_CFM_T *)message)->status);

    BleWifi_Ble_FSM(BW_BLE_FSM_SET_ADV_DATA_CFM, (void *)message , 0);

    return 0;
}

static int32_t BleWifi_Ble_CmMsgHandler_SetScanRspDataCfm(TASK task, MESSAGEID id, MESSAGE message)
{
    BLEWIFI_INFO("APP-LE_CM_MSG_SET_SCAN_RSP_DATA_CFM - Status = %x\r\n", ((LE_CM_MSG_SET_SCAN_RSP_DATA_CFM_T *)message)->status);

    BleWifi_Ble_FSM(BW_BLE_FSM_SET_SCAN_DATA_CFM, (void *)message , 0);

    return 0;
}

static int32_t BleWifi_Ble_CmMsgHandler_SetAdvertisingParamsCfm(TASK task, MESSAGEID id, MESSAGE message)
{
    uint8_t u8Ret = true;
    LE_CM_MSG_SET_ADVERTISING_PARAMS_CFM_T *cfm = (LE_CM_MSG_SET_ADVERTISING_PARAMS_CFM_T *)message;
    BLEWIFI_INFO("APP-LE_CM_MSG_SET_ADVERTISING_PARAMS_CFM - Status = %x\r\n", ((LE_CM_MSG_SET_ADVERTISING_PARAMS_CFM_T *)message)->status);

    if(cfm->status != SYS_ERR_SUCCESS)
    {
        u8Ret = false;
    }
    else
    {
        u8Ret = true;
    }
    BleWifi_Ble_FSM(BW_BLE_FSM_ADVERTISING_TIME_CHANGE_CFM , (void *)&u8Ret , sizeof(uint8_t));

    return 0;
}

static int32_t BleWifi_Ble_CmMsgHandler_EnterAdvertisingCfm(TASK task, MESSAGEID id, MESSAGE message)
{
    uint8_t u8Ret = true;
    LE_CM_MSG_ENTER_ADVERTISING_CFM_T *cfm = (LE_CM_MSG_ENTER_ADVERTISING_CFM_T *)message;
    BLEWIFI_INFO("APP-LE_CM_MSG_ENTER_ADVERTISING_CFM Status = %x\r\n", cfm->status);

    if (cfm->status == SYS_ERR_SUCCESS)
    {
        u8Ret = true;
    }
    else
    {
        u8Ret = false;
    }

    BleWifi_Ble_FSM(BW_BLE_FSM_ADVERTISING_START_CFM , (void *)&u8Ret , sizeof(uint8_t));

    return 0;
}

static int32_t BleWifi_Ble_CmMsgHandler_ExitAdvertisingCfm(TASK task, MESSAGEID id, MESSAGE message)
{
    uint8_t u8Ret = true;
    LE_CM_MSG_EXIT_ADVERTISING_CFM_T *cfm = (LE_CM_MSG_EXIT_ADVERTISING_CFM_T *)message;
    BLEWIFI_INFO("APP-LE_CM_MSG_EXIT_ADVERTISING_CFM Status = %x\r\n", cfm->status);

    if (cfm->status == SYS_ERR_SUCCESS)
    {
        u8Ret = true;
    }
    else
    {
        u8Ret = false;
    }
    BleWifi_Ble_FSM(BW_BLE_FSM_ADVERTISING_EXIT_CFM , (void *)&u8Ret , sizeof(uint8_t));

    return 0;
}

static int32_t BleWifi_Ble_CmMsgHandler_ConnectionCompleteInd(TASK task, MESSAGEID id, MESSAGE message)
{
    LE_CM_CONNECTION_COMPLETE_IND_T *ind = (LE_CM_CONNECTION_COMPLETE_IND_T *)message;
    BLEWIFI_INFO("APP-LE_CM_CONNECTION_COMPLETE_IND status = %x\r\n", ind->status);

    if (ind->status == SYS_ERR_SUCCESS)
    {
        //BleWifi_Ctrl_MsgSend(BLEWIFI_CTRL_MSG_BLE_CONNECTION_COMPLETE, NULL, 0);
        BleWifi_Ble_FSM(BW_BLE_FSM_CONNECTION_SUCCESS , (void *)message , 0);
    }
    else
    {
        //BleWifi_Ctrl_MsgSend(BLEWIFI_CTRL_MSG_BLE_CONNECTION_FAIL, NULL, 0);
        BleWifi_Ble_FSM(BW_BLE_FSM_CONNECTION_FAIL , (void *)message , 0);
    }

    return 0;
}

static int32_t BleWifi_Ble_CmMsgHandler_SignalUpdateReq(TASK task, MESSAGEID id, MESSAGE message)
{
    LE_CM_MSG_SIGNAL_UPDATE_REQ_T *req = (LE_CM_MSG_SIGNAL_UPDATE_REQ_T *)message;
    printf("APP-LE_CM_MSG_SIGNAL_UPDATE_REQ identifier = %d\r\n", req->identifier);
    printf("    min = %x max = %x latency = %x timeout = %x\r\n", req->interval_min, req->interval_max, req->slave_latency, req->timeout_multiplier);

    BleWifi_Ble_FSM(BW_BLE_FSM_SIGNAL_UPDATE_REQ, (void *)message , 0);

    return 0;
}

static int32_t BleWifi_Ble_CmMsgHandler_ConnParaReq(TASK task, MESSAGEID id, MESSAGE message)
{
    LE_CM_MSG_CONN_PARA_REQ_T *req = (LE_CM_MSG_CONN_PARA_REQ_T *)message;
    printf("APP-LE_CM_MSG_CONN_PARA_REQ min = %x max = %x latency = %x timeout = %x\r\n", req->itv_min, req->itv_max, req->latency, req->sv_tmo);

    BleWifi_Ble_FSM(BW_BLE_FSM_CONN_PARA_REQ, (void *)message , 0);

    return 0;
}

static int32_t BleWifi_Ble_CmMsgHandler_ConnUpdateCompleteInd(TASK task, MESSAGEID id, MESSAGE message)
{
    LE_CM_MSG_CONN_UPDATE_COMPLETE_IND_T *ind = (LE_CM_MSG_CONN_UPDATE_COMPLETE_IND_T *)message;
    printf("APP-LE_CM_MSG_CONN_UPDATE_COMPLETE_IND status = %x, itv = %x, latency = %x svt = %x\r\n", ind->status, ind->interval, ind->latency, ind->supervision_timeout);

    BleWifi_Ble_FSM(BW_BLE_FSM_CONN_UPDATE_COMPLETE_CFM, (void *)message , 0);

    return 0;
}

static int32_t BleWifi_Ble_CmMsgHandler_SetDisconnectCfm(TASK task, MESSAGEID id, MESSAGE message)
{
    // !!! after LeGapDisconnectReq
    // if the disconnection is finished, "LE_CM_MSG_DISCONNECT_COMPLETE_IND" will be executed
#ifdef BLEWIFI_SHOW_INFO
    LE_CM_MSG_SET_DISCONNECT_CFM_T *cfm = (LE_CM_MSG_SET_DISCONNECT_CFM_T *)message;
    BLEWIFI_INFO("APP-LE_CM_MSG_SET_DISCONNECT_CFM conn_hdl = %x status = %x\r\n", cfm->handle, cfm->status);
#endif

    BleWifi_Ble_FSM(BW_BLE_FSM_SET_DISCONNECT_CFM, (void *)message , 0);

    return 0;
}

static int32_t BleWifi_Ble_CmMsgHandler_DisconnectCompleteInd(TASK task, MESSAGEID id, MESSAGE message)
{
    uint32_t u32RetReason = 0;
    LE_CM_MSG_DISCONNECT_COMPLETE_IND_T *ind = (LE_CM_MSG_DISCONNECT_COMPLETE_IND_T *)message;

#ifdef BLEWIFI_SHOW_INFO
    BLEWIFI_INFO("APP-LE_CM_MSG_DISCONNECT_COMPLETE_IND conn_hdl = %x status = %x reason = %x \r\n", ind->conn_hdl, ind->status, ind->reason);
#endif

    u32RetReason = (uint32_t)ind->reason;

    BleWifi_Ble_FSM(BW_BLE_FSM_DISCONNECT_CFM, (void *)&u32RetReason , sizeof(uint32_t));

    return 0;
}

static int32_t BleWifi_Ble_AppMsgHandler_SendData(TASK task, MESSAGEID id, MESSAGE message)
{
    BleWifi_Ble_FSM(BW_BLE_FSM_SEND_DATA , (void *)message , 0);

    return 0;
}

static int32_t BleWifi_Ble_AppMsgHandler_SendToPeer(TASK task, MESSAGEID id, MESSAGE message)
{
    BleWifi_Ble_FSM(BW_BLE_FSM_SEND_TO_PEER , (void *)message , 0);

    return 0;
}

static int32_t BleWifi_Ble_AppMsgHandler_SendToPeerCfm(TASK task, MESSAGEID id, MESSAGE message)
{
    BleWifi_Ble_FSM(BW_BLE_FSM_SEND_TO_PEER_CFM , (void *)message , 0);

    return 0;
}

BLE_APP_DATA_T* BleWifi_Ble_GetEntity(void)
{
	return &gTheBle;
}

void BleWifi_Ble_SendAppMsgToBle(UINT32 id, UINT16 len, void *data)
{
	if ((id >= BW_APP_MSG_BASE) && (id < BW_APP_MSG_TOP))
	{
		void *p = 0;

		if (len)
        {
			MESSAGE_DATA_BULID(BLEWIFI_MESSAGE, len);

			msg->len = len;
			msg->data = MESSAGE_OFFSET(BLEWIFI_MESSAGE);
			MemCopy(msg->data, data, len);
			p = msg;
        }

	    LeSendMessage(&gTheBle.task, id, p);
    }
}

/*void BleWifi_Ble_ServerAppInit(void)
{

}*/

int32_t BleWifi_Ble_AppMsgHandler_BleCommand_Exec(TASK task, MESSAGEID id, MESSAGE message)
{
    BleWifi_BLE_FSM_CmdQ_t *pstNextCmd = NULL;
    osEvent rxEvent;

    if(g_u8BleCMDWaitingFlag == BLEWIFI_BLE_EXEC_CMD_WAITING) // don't exec cmd
    {
        return BLEWIFI_BLE_RET_SUCCESS;
    }

    rxEvent = osMessagePeek(g_tBleCMDQ_Id , 0);

    if(rxEvent.status == osEventMessage)
    {
        pstNextCmd = (BleWifi_BLE_FSM_CmdQ_t *)rxEvent.value.p;
        if(pstNextCmd != NULL)
        {
            BleWifi_Ble_FSM(pstNextCmd->u32EventId , pstNextCmd->u8aData , pstNextCmd->u32Length);
        }
    }

    return BLEWIFI_BLE_RET_SUCCESS;
}


