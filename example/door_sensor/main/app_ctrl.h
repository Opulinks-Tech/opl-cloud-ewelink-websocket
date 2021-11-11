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
#include "app_configuration.h"
#include <time.h> // This is for time_t
#include "mw_fim_default_group17_project.h"
#include "cmsis_os.h"

#define APP_CTRL_TASK_PRIORITY      (osPriorityNormal)
#define APP_CTRL_QUEUE_SIZE         (20)
#define APP_CTRL_TASK_STACK_SIZE    (768)

#define CONVERT_MINUTE_TO_MILLISECOND   (60000)

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
    APP_CTRL_MSG_WIFI_STOP_COMPLETE,            //Wi-Fi report status

    APP_CTRL_MSG_WIFI_NUM,

    /* Others Event */
    APP_CTRL_MSG_OTHER_OTA_ON = 0x100,          //OTA
    APP_CTRL_MSG_OTHER_OTA_OFF,                 //OTA success
    APP_CTRL_MSG_OTHER_OTA_OFF_FAIL,            //OTA fail

    APP_CTRL_MSG_OTHER_LED_TIMER,
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

    APP_CTRL_MSG_HTTP_POST_DATA_IND,
#ifdef __SONOFF__
    APP_CTRL_MSG_IS_TEST_MODE_AT_FACTORY,
    APP_CTRL_MSG_IS_TEST_MODE_TIMEOUT,
#endif

    APP_CTRL_MSG_DOOR_STATECHANGE,
    APP_CTRL_MSG_DOOR_DEBOUNCETIMEOUT,
    APP_CTRL_MSG_DOOR_STATUSWARNING_TIMEOUT,
    APP_CTRL_MSG_DOOR_DAMAGE_CHANGE,
    APP_CTRL_MSG_DOOR_DAMAGE_TIMEOUT,

    APP_CTRL_MSG_DOOR_DAILY_BATTERY_POST_TIMEOUT,

    APP_CTRL_MSG_DOOR_SET_HOLD_REMIDER,

    APP_CTRL_MSG_NETWORK_STOP_DELAY_TIMEOUT,
    APP_CTRL_MSG_AUTO_CONNECT_TIMEOUT,
    APP_CTRL_MSG_DATA_POST_FAIL,

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
#define APP_CTRL_EVENT_BIT_BUTTON_PRESS          0x00000800U

#define APP_CTRL_EVENT_BIT_NOT_CNT_SRV           0x00001000U
#define APP_CTRL_EVENT_BIT_SHORT_PRESS           0x00002000U
#define APP_CTRL_EVENT_BIT_CHANGE_HTTPURL        0x00004000U
#ifdef __SONOFF__
#define APP_CTRL_EVENT_BIT_TEST_MODE             0x00008000U  // Test Mode
#define APP_CTRL_EVENT_BIT_AT_WIFI_MODE                0x00010000U  // AT+WIFI Mode
#endif
#define APP_CTRL_EVENT_BIT_WIFI_USER_CONNECTING_EXEC   0x00020000U  // when connect , need to wait wifi stop complete
#define APP_CTRL_EVENT_BIT_WAIT_UPDATE_HOST            0x00040000U  // need to wait APP update host info when pairing.
#define APP_CTRL_EVENT_BIT_DOOR                        0x00100000U  // Door (Key) Status
#define APP_CTRL_EVENT_BIT_MANUALLY_CONNECT_SCAN       0x00200000U  // manually connect flag
#define APP_CTRL_EVENT_BIT_WIFI_AUTOCONN_LED           0x00400000U  //init & disconnect used control LED flash
#define APP_CTRL_EVENT_BIT_OFFLINE                     0x00800000U

#define POST_DATA_TIME              (3600000)  // 1 hour - smart sleep for one hour then post data

#define SHORT_TRIG                      1
#define DOOR_CLOSE                      2
#define DOOR_OPEN                       3
#define TIMER_POST                      4
#define DOUBLE_SHORT_TRIG               5
#define DOOR_SENSOR_DAMAGE              6
#define DOOR_HOLD_REMINDER_WARING       7

#define DOOR_WARING_FLAG_OPEN           1
#define DOOR_WARING_FLAG_CLOSE          0

#define DOOR_DEFENSE_OFF                0
#define DOOR_DEFENSE_ON                 1

typedef enum app_ctrl_led_state
{
    APP_CTRL_LED_BLE_ON_1 = 0x00,
    APP_CTRL_LED_BLE_OFF_1,

    APP_CTRL_LED_AUTOCONN_ON_1,
    APP_CTRL_LED_AUTOCONN_OFF_1,

    APP_CTRL_LED_OTA_ON,
    APP_CTRL_LED_OTA_OFF,

    APP_CTRL_LED_ALWAYS_OFF,

    APP_CTRL_LED_TEST_MODE_ON_1,
    APP_CTRL_LED_TEST_MODE_OFF_1,
    APP_CTRL_LED_TEST_MODE_ON_2,
    APP_CTRL_LED_TEST_MODE_OFF_2,
    APP_CTRL_LED_TEST_MODE_ON_3,
    APP_CTRL_LED_TEST_MODE_OFF_3,
    APP_CTRL_LED_TEST_MODE_ON_4,
    APP_CTRL_LED_TEST_MODE_OFF_4,

    APP_CTRL_LED_NOT_CNT_SRV_ON_1,
    APP_CTRL_LED_NOT_CNT_SRV_OFF_1,
    APP_CTRL_LED_NOT_CNT_SRV_ON_2,
    APP_CTRL_LED_NOT_CNT_SRV_OFF_2,

    APP_CTRL_LED_OFFLINE_ON_1,
    APP_CTRL_LED_OFFLINE_OFF_1,
#if 1   // 20200528, Terence change offline led as same as NOT_CNT_SRV
    APP_CTRL_LED_OFFLINE_ON_2,
    APP_CTRL_LED_OFFLINE_OFF_2,
#endif

    APP_CTRL_LED_SHORT_PRESS_ON,

    APP_CTRL_LED_BOOT_ON_1,//Goter
    APP_CTRL_LED_BOOT_OFF_1,//Goter
    APP_CTRL_LED_BOOT_ON_2,//Goter

    APP_CTRL_LED_MP_NO_ROUTER_ON_1,
    APP_CTRL_LED_MP_NO_ROUTER_OFF_1,
    APP_CTRL_LED_MP_NO_SERVER_ON_1,
    APP_CTRL_LED_MP_NO_SERVER_OFF_1,
    APP_CTRL_LED_MP_NO_SERVER_ON_2,
    APP_CTRL_LED_MP_NO_SERVER_OFF_2,

    APP_CTRL_LED_DOOR_SWITCH,
    APP_CTRL_LED_DAMAGE,

    APP_CTRL_LED_ALWAYS_ON,

    APP_CTRL_LED_NUM
} app_ctrl_led_state_e;

#define SENSOR_COUNT   (32)

typedef struct
{
    uint64_t u64TimeStamp;     /* Time */
    float fVBatPercentage;     /* Battery*/
    uint8_t ubaDoorStatus;     /* Door Status */
    uint8_t ubaType;           /* Type Status */
    int8_t rssi;               /* rssi*/
} Sensor_Data_t;



typedef void (*App_Ctrl_EvtHandler_Fp_t)(uint32_t u32EvtType, void *pData, uint32_t u32Len);
typedef struct
{
    uint32_t u32EventId;
    App_Ctrl_EvtHandler_Fp_t fpFunc;
} App_Ctrl_EvtHandlerTbl_t;

extern osTimerId    g_tAppCtrl_network_stop_delay_timeout_timer;
extern osTimerId    g_tAppPostFailLedTimeOutTimerId;

void App_Ctrl_SysModeSet(uint8_t mode);
uint8_t App_Ctrl_SysModeGet(void);

void App_Ctrl_NetworkingStart(uint32_t u32ExpireTime);
void App_Ctrl_NetworkingStop(void);
void App_Ctrl_LedStatusChange(void);

#if (APP_CTRL_BUTTON_SENSOR_EN == 1)
void App_Ctrl_ButtonReleaseHandle(uint8_t u8ReleaseCount);
#endif

#ifdef __BLEWIFI_TRANSPARENT__
int App_Ctrl_BleCastWithExpire(uint8_t u8BleCastEnable, uint32_t u32ExpireTime);
int App_Ctrl_BleCastParamSet(uint32_t u32CastInteval);
#endif

int App_Ctrl_MsgSend(uint32_t u32MsgType, uint8_t *pu8Data, uint32_t u32DataLen);
void App_Ctrl_Init(void);

void App_Door_Sensor_Set_Defense(uint8_t u8En);

#endif /* __APP_CTRL_H__ */

