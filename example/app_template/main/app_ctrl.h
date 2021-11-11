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

/**
 * @file app_ctrl.h
 * @author Terence Yang
 * @date 06 Oct 2020
 * @brief File includes the function declaration of blewifi ctrl task.
 *
 */

#ifndef __APP_CTRL_H__
#define __APP_CTRL_H__

#include <stdint.h>
#include <stdbool.h>
#include "blewifi_configuration.h"

#define APP_CTRL_TASK_PRIORITY      (osPriorityNormal)
#define APP_CTRL_QUEUE_SIZE         (20)
#define APP_CTRL_TASK_STACK_SIZE    (768)

typedef enum blewifi_app_ctrl_msg_type
{
    /* BLE Trigger */
    APP_CTRL_MSG_BLE_INIT_COMPLETE = 0,         //BLE report status
    APP_CTRL_MSG_BLE_START_COMPLETE,            //BLE report status
    APP_CTRL_MSG_BLE_STOP_COMPLETE,             //BLE report status
    APP_CTRL_MSG_BLE_CONNECTION,                //BLE report status
    APP_CTRL_MSG_BLE_DISCONNECTION,             //BLE report status
    APP_CTRL_MSG_BLE_DATA_IND,                  //BLE receive the data from peer to device

    APP_CTRL_MSG_BLE_NUM,

    /* Wi-Fi Trigger */
    APP_CTRL_MSG_WIFI_INIT_COMPLETE = 0x80,     //Wi-Fi report status
    APP_CTRL_MSG_WIFI_SCAN_DONE,                //Wi-Fi report status
    APP_CTRL_MSG_WIFI_CONNECTION,               //Wi-Fi report status
    APP_CTRL_MSG_WIFI_DISCONNECTION,            //Wi-Fi report status

    APP_CTRL_MSG_WIFI_NUM,

    /* Others Event */
    APP_CTRL_MSG_OTHER_OTA_ON = 0x100,          //OTA
    APP_CTRL_MSG_OTHER_OTA_OFF,                 //OTA success
    APP_CTRL_MSG_OTHER_OTA_OFF_FAIL,            //OTA fail

    APP_CTRL_MSG_OTHER_SYS_TIMER,               //SYS timer

    APP_CTRL_MSG_NETWORKING_START,              //Networking Start
    APP_CTRL_MSG_NETWORKING_STOP,               //Networking Stop

#if (APP_CTRL_BUTTON_SENSOR_EN == 1)
    APP_CTRL_MSG_BUTTON_STATE_CHANGE,           //Button Stage Change
    APP_CTRL_MSG_BUTTON_DEBOUNCE_TIMEOUT,       //Button Debounce Time Out
    APP_CTRL_MSG_BUTTON_RELEASE_TIMEOUT,        //Button Release Time Out
#endif

#if (APP_CTRL_WAKEUP_IO_EN == 1)
    APP_CTRL_MSG_PS_SMART_STATE_CHANGE,         //PS SMART Stage Change
    APP_CTRL_MSG_PS_SMART_DEBOUNCE_TIMEOUT,     //PS SMART Debounce Time Out
#endif

    APP_CTRL_MSG_MAX_NUM
} app_ctrl_msg_type_e;

typedef struct
{
    uint32_t u32Event;
    uint32_t u32Length;
    uint8_t u8aMessage[];
} xAppCtrlMessage_t;

typedef enum app_ctrl_sys_state
{
    APP_CTRL_SYS_INIT = 0x00,       // PS(0), Wifi(1), Ble(1)
    APP_CTRL_SYS_NORMAL,            // PS(1), Wifi(1), Ble(1)
    APP_CTRL_SYS_BLE_OFF,           // PS(1), Wifi(1), Ble(0)

    APP_CTRL_SYS_NUM
} app_ctrl_sys_state_e;

typedef enum {
    APP_CTRL_FSM_INIT                    = 0x00,
    APP_CTRL_FSM_IDLE                    = 0x01,
    APP_CTRL_FSM_WIFI_NETWORK_READY      = 0x02,
    APP_CTRL_FSM_CLOUD_CONNECTING        = 0x03,
    APP_CTRL_FSM_CLOUD_CONNECTED         = 0x04,

    APP_CTRL_FSM_NUM
} app_ctrl_fsm_state_e;

// event group bit (0 ~ 23 bits)
#define APP_CTRL_EVENT_BIT_BLE_INIT_DONE         0x00000001U
#define APP_CTRL_EVENT_BIT_BLE_START             0x00000002U
#define APP_CTRL_EVENT_BIT_BLE_CONNECTED         0x00000004U
#define APP_CTRL_EVENT_BIT_WIFI_INIT_DONE        0x00000008U
#define APP_CTRL_EVENT_BIT_WIFI_SCANNING         0x00000010U
#define APP_CTRL_EVENT_BIT_WIFI_CONNECTING       0x00000020U
#define APP_CTRL_EVENT_BIT_WIFI_CONNECTED        0x00000040U
#define APP_CTRL_EVENT_BIT_WIFI_GOT_IP           0x00000080U
#define APP_CTRL_EVENT_BIT_NETWORK               0x00000100U
#define APP_CTRL_EVENT_BIT_OTA                   0x00000200U
#define APP_CTRL_EVENT_BIT_IOT_INIT              0x00000400U

typedef void (*App_Ctrl_EvtHandler_Fp_t)(uint32_t u32EvtType, void *pData, uint32_t u32Len);
typedef struct
{
    uint32_t u32EventId;
    App_Ctrl_EvtHandler_Fp_t fpFunc;
} App_Ctrl_EvtHandlerTbl_t;


void App_Ctrl_SysModeSet(uint8_t mode);
uint8_t App_Ctrl_SysModeGet(void);

void App_Ctrl_NetworkingStart(uint32_t u32ExpireTime);
void App_Ctrl_NetworkingStop(void);
#if (APP_CTRL_BUTTON_SENSOR_EN == 1)
void App_Ctrl_ButtonReleaseHandle(uint8_t u8ReleaseCount);
#endif

#ifdef __BLEWIFI_TRANSPARENT__
int App_Ctrl_BleCastWithExpire(uint8_t u8BleCastEnable, uint32_t u32ExpireTime);
int App_Ctrl_BleCastParamSet(uint32_t u32CastInteval);
#endif

int App_Ctrl_MsgSend(uint32_t u32MsgType, uint8_t *pu8Data, uint32_t u32DataLen);
void App_Ctrl_Init(void);

#endif /* __APP_CTRL_H__ */

