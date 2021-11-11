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
#ifndef _BLEWIFI_WIFI_FSM
#define _BLEWIFI_WIFI_FSM

#include <stdlib.h>
#include <string.h>
#include "cmsis_os.h"
#include "event_groups.h"
#include "sys_os_config.h"
#include "sys_os_config_patch.h"
#include "at_cmd_common.h"
#include "wifi_types.h"
#include "blewifi_common.h"
#include "blewifi_configuration.h"
#include "app_configuration.h"

typedef enum {
    BW_WIFI_FSM_STATE_INIT             = 0x0,
    BW_WIFI_FSM_STATE_IDLE             = 0x1,
    BW_WIFI_FSM_STATE_CONNECTED        = 0x2,
    BW_WIFI_FSM_STATE_READY            = 0x3,
}APP_WIFI_STATE_MACHINE;


typedef enum bw_wifi_fsm_msg_type
{
    BW_WIFI_FSM_MSG_WIFI_REQ_START                  = 0x0,      //Req event start
    BW_WIFI_FSM_MSG_WIFI_REQ_INIT                   = 0x1,      //Wi-Fi init
    BW_WIFI_FSM_MSG_WIFI_REQ_SCAN                   = 0x2,      //Wi-Fi scan
    BW_WIFI_FSM_MSG_WIFI_REQ_CONNECT                = 0x3,      //Wi-Fi connect
    //don't used                                    = 0x4,
    BW_WIFI_FSM_MSG_WIFI_REQ_DISCONNECT             = 0x5,      //Wi-Fi disconnnect
    BW_WIFI_FSM_MSG_WIFI_STOP                       = 0x6,      //Wi-Fi stop

    BW_WIFI_FSM_MSG_WIFI_REQ_AUTO_CONNECT_START     = 0x15,     //Wi-Fi Auto connect scan
    BW_WIFI_FSM_MSG_WIFI_REQ_AUTO_CONNECT_SCAN      = 0x16,     //Wi-Fi Auto connect scan
    BW_WIFI_FSM_MSG_WIFI_REQ_AUTO_CONNECT           = 0x17,     //Wi-Fi Auto connect

    BW_WIFI_FSM_MSG_WIFI_REQ_ROAMING_SCAN           = 0x20,     //Wi-Fi Roaming scan
    BW_WIFI_FSM_MSG_WIFI_REQ_ROAMING_CONNECT        = 0x21,     //Wi-Fi Roaming connect

    BW_WIFI_FSM_MSG_WIFI_REQ_END                    = 0x22,      //Req event end

    /* Wi-Fi Trigger */
    BW_WIFI_FSM_MSG_WIFI_IND_START                  = 0x40,     //Ind start
    BW_WIFI_FSM_MSG_WIFI_INIT_COMPLETE              = 0x41,     //Wi-Fi report status
    BW_WIFI_FSM_MSG_WIFI_SCAN_DONE_IND              = 0x42,     //Wi-Fi report status
    BW_WIFI_FSM_MSG_WIFI_CONNECTION_SUCCESS_IND     = 0x43,     //Wi-Fi report status
    BW_WIFI_FSM_MSG_WIFI_CONNECTION_FAIL_IND        = 0x44,     //Wi-Fi report status
    BW_WIFI_FSM_MSG_WIFI_DISCONNECTION_IND          = 0x45,     //Wi-Fi report status
    //don't used
    BW_WIFI_FSM_MSG_WIFI_IND_END                    = 0x47,     //Ind end

    BW_WIFI_FSM_MSG_WIFI_GOT_IP_IND                 = 0x60,     //Wi-Fi report status

    BW_WIFI_FSM_MSG_WIFI_EXEC_CMD                   = 0x100,    //execute the WIFI command to FSM
    BW_WIFI_FSM_MSG_DHCP_TIMEOUT                    = 0x101,    //DHCP Timeout

    BW_WIFI_FSM_MSG_WIFI_NUM,

    BW_WIFI_FSM_MSG_HANDLER_TBL_END                 = 0xFFFF,

    /* Others Event */

    BW_WIFI_FSM_MSG_OTHER_NUM
} bw_wifi_fsm_msg_type_e;


typedef enum bw_wifi_fsm_cmd_state
{
    BW_WIFI_CMD_M3_TIMEOUT = -2,
    BW_WIFI_CMD_FAILED     = -1,
    BW_WIFI_CMD_FINISH     =  0,
    BW_WIFI_CMD_EXECUTING  =  1,
    BW_WIFI_CMD_NOT_MATCH  =  2,
} bw_wifi_fsm_cmd_state_type_e;

typedef enum bw_wifi_fsm_disconnect_reason
{
    BW_WIFI_STOP_CONNECT = 0,
    BW_WIFI_RESET = 1,
} bw_wifi_fsm_disconnect_reason_e;

#define BLEWIFI_WIFI_EXEC_CMD_CONTINUE   (0)
#define BLEWIFI_WIFI_EXEC_CMD_WAITING    (1)


#define BW_WIFI_FSM_EVENT_BIT_EXEC_AUTO_CONN                     0x00000001U
#define BW_WIFI_FSM_EVENT_BIT_EXEC_RESET                         0x00000002U
#define BW_WIFI_FSM_EVENT_BIT_DHCP_TIMEOUT                       0x00000004U
#define BW_WIFI_FSM_EVENT_BIT_WIFI_USED                          0x00000008U

extern uint32_t g_u32AppCtrlAutoConnectIntervalSelect;

typedef int32_t (*T_Bw_Wifi_FsmEvtHandler_Fp)(uint32_t u32EventId, uint8_t *pu8Data, uint32_t u32DataLen);
typedef void (*T_BleWifi_Wifi_App_Ctrl_CB_Fp)(void);
typedef struct
{
    uint32_t u32EventId;
    T_Bw_Wifi_FsmEvtHandler_Fp fpFunc;
} T_Bw_Wifi_FsmEvtHandlerTbl;

typedef struct
{
    uint32_t u32EventId;
    uint32_t u32Length;
    T_BleWifi_Wifi_App_Ctrl_CB_Fp fpCtrlAppCB;
    uint8_t u8aData[];
} BleWifi_Wifi_FSM_CmdQ_t;

typedef struct {
    uint8_t	ssid[WIFI_MAX_LENGTH_OF_SSID];	    //The SSID of the target AP.
    uint8_t	ssid_length;	                    //The length of the SSID.
    uint8_t	bssid[WIFI_MAC_ADDRESS_LENGTH];	    //The MAC address of the target AP.
    uint8_t	password[WIFI_LENGTH_PASSPHRASE];	//The password of the target AP.
    uint8_t	password_length;	                //The length of the password. If the length is 64, the password is regarded as PMK.

} wifi_status_t;

int32_t BwWifiFlushCMD(osMessageQId tBwWifiCmdQueueId);
int32_t BleWifi_Wifi_FSM(uint32_t u32EventId, uint8_t *pu8Data, uint32_t u32DataLen);
int32_t BleWifi_Wifi_FSM_MsgSend(uint32_t u32MsgType, uint8_t *pu8Data, uint32_t u32DataLen, T_BleWifi_Wifi_App_Ctrl_CB_Fp fpCB, uint32_t u32IsFront);
int32_t BleWifi_Wifi_FSM_CmdPush_Without_Check(uint32_t u32EventId, uint8_t *pu8Data, uint32_t u32DataLen, T_BleWifi_Wifi_App_Ctrl_CB_Fp fpCB, uint32_t u32IsFront);
int32_t BleWifi_Wifi_FSM_CmdPush(uint32_t u32EventId, uint8_t *pu8Data, uint32_t u32DataLen, T_BleWifi_Wifi_App_Ctrl_CB_Fp fpCB, uint32_t u32IsFront);
void BleWifi_Wifi_FSM_Init(void);

void BwWifiFsm_AutoConnectTrigger(void const *argu);

#endif /* _BLEWIFI_BLE_FSM */
