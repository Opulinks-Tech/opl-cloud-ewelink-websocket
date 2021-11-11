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
#include "hal_os_patch.h"


#ifndef _BLEWIFI_BLE_FSM_
#define _BLEWIFI_BLE_FSM_

typedef int32_t (*T_BleWifi_Ble_FSM_Handler_Fp)(uint32_t u32EvtType, void *data, int len);
typedef void (*T_BleWifi_Ble_App_Ctrl_CB_Fp)(void);

typedef struct
{
    uint32_t u32EventId;
    T_BleWifi_Ble_FSM_Handler_Fp fpFunc;
} T_BleWifi_Ble_FSM_MsgHandlerTbl;

typedef enum blewifi_ble_fsm_type
{
    BW_BLE_FSM_ACTIVE_START                = 0x0,  //active event start
    BW_BLE_FSM_INITIALIZING                = 0x1,
    BW_BLE_FSM_ADVERTISING_START           = 0x2,
    BW_BLE_FSM_ADVERTISING_EXIT            = 0x3,
    BW_BLE_FSM_ADVERTISING_TIME_CHANGE     = 0x4,
    BW_BLE_FSM_DISCONNECT                  = 0x5,
    BW_BLE_FSM_CHANGE_ADVERTISING_DATA     = 0x6,
    BW_BLE_FSM_CHANGE_SCAN_DATA            = 0x7,
    BW_BLE_FSM_BLE_STOP                    = 0x8,
    BW_BLE_FSM_ACTIVE_END                  = 0x9,  //end event start

    BW_BLE_FSM_CFM_START                   = 0x40, //cfm start
    BW_BLE_FSM_INIT_COMPLETE               = 0x41,
    BW_BLE_FSM_ADVERTISING_START_CFM       = 0x42,
    BW_BLE_FSM_ADVERTISING_EXIT_CFM        = 0x43,
    BW_BLE_FSM_ADVERTISING_TIME_CHANGE_CFM = 0x44,
    BW_BLE_FSM_DISCONNECT_CFM              = 0x45,
    //reserve                              = 0x46,
    //reserve                              = 0x47,
    //reserve                              = 0x48,
    BW_BLE_FSM_CFM_END                     = 0x49, //cfm end

    BW_BLE_FSM_CONNECTION_SUCCESS          = 0x80,
    BW_BLE_FSM_CONNECTION_FAIL             = 0x81,

    BW_BLE_FSM_SEND_DATA                   = 0x100,
    BW_BLE_FSM_SEND_TO_PEER                = 0x101,
    BW_BLE_FSM_SEND_TO_PEER_CFM            = 0x102,

    BW_BLE_FSM_SET_ADV_DATA_CFM            = 0x201,
    BW_BLE_FSM_SET_SCAN_DATA_CFM           = 0x202,
    BW_BLE_FSM_SIGNAL_UPDATE_REQ           = 0x203,
    BW_BLE_FSM_CONN_PARA_REQ               = 0x204,
    BW_BLE_FSM_CONN_UPDATE_COMPLETE_CFM    = 0x205,
    BW_BLE_FSM_SET_DISCONNECT_CFM          = 0x206,

    BW_BLE_FSM_END                         = 0xFFFF
} blewifi_ble_fsm_type_e;

typedef enum blewifi_ble_ret_event
{
    BLEWIFI_RET_NONE                            = 0x0,
    BLEWIFI_RET_INIT                            = 0x1,
    BLEWIFI_RET_START                           = 0x2,
    BLEWIFI_RET_STOP                            = 0x3,

    BLEWIFI_RET_END                         = 0xFFFF
} blewifi_ble_ret_event_e;

enum
{
	BLEWIFI_BLE_STATE_INIT = 0,
	BLEWIFI_BLE_STATE_IDLE,
	BLEWIFI_BLE_STATE_ADVERTISING,
	BLEWIFI_BLE_STATE_CONNECTED,

    BLEWIFI_BLE_STATE_TOP
};

typedef struct
{
    uint32_t u32EventId;
    uint32_t u32Length;
    T_BleWifi_Ble_App_Ctrl_CB_Fp fpCtrlAppCB;
    uint8_t u8aData[];
} BleWifi_BLE_FSM_CmdQ_t;

typedef enum bw_ble_fsm_cmd_state
{
    BW_BLE_CMD_M3_TIMEOUT = -2,
    BW_BLE_CMD_FAILED     = -1,
    BW_BLE_CMD_FINISH     =  0,
    BW_BLE_CMD_EXECUTING  =  1,
    BW_BLE_CMD_NOT_MATCH  =  2,
} bw_ble_fsm_cmd_state_type_e;

#define BLEWIFI_BLE_CMDQ_SIZE (16)

#define BLEWIFI_BLE_RET_SUCCESS         (0)
#define BLEWIFI_BLE_RET_FAIL            (-1)

#define BLEWIFI_BLE_EXEC_CMD_CONTINUE   (0)
#define BLEWIFI_BLE_EXEC_CMD_WAITING    (1)

int32_t BleWifi_Ble_FSM_Init(void);
//handle the event either active or passive
int32_t BleWifi_Ble_FSM(uint32_t u32EvtType, void *data, int len);
//push the Ble command to cmdq and decide whether execute next cmd
int32_t BleWifi_Ble_PushToFSMCmdQ(uint32_t u32EventId , uint8_t *pu8Data, uint32_t u32Length , T_BleWifi_Ble_App_Ctrl_CB_Fp fpCB);
int32_t BleWifi_Ble_Queue_Push(osMessageQId tBwBleCmdQueueId , uint32_t u32EventId , uint8_t *pu8Data, uint32_t u32Length , T_BleWifi_Ble_App_Ctrl_CB_Fp fpCB , uint32_t u32IsFront);
int32_t BwBleFlushCMD(osMessageQId tBwBleCmdQueueId);

#endif /* _BLEWIFI_BLE_FSM_ */

