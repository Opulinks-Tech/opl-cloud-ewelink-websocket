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
 * @file app_ctrl.c
 * @author Terence Yang
 * @date 06 Oct 2020
 * @brief File creates the blewifi ctrl task architecture.
 *
 */

#include <stdlib.h>
#include <string.h>
#include "cmsis_os.h"
#include "event_groups.h"
#include "sys_os_config.h"
#include "sys_os_config_patch.h"
#include "at_cmd_common.h"

#include "blewifi_common.h"
#include "blewifi_configuration.h"
#include "app_configuration.h"
#include "app_main.h"
#include "app_ctrl.h"
#include "blewifi_wifi_api.h"
#include "blewifi_ble_api.h"
#include "blewifi_data.h"
#include "mw_ota_def.h"
#include "mw_ota.h"
#include "hal_system.h"
#include "mw_fim_default_group03.h"
#include "mw_fim_default_group03_patch.h"
#include "mw_fim_default_group11_project.h"
#include "mw_fim_default_group12_project.h"
#include "mw_fim_default_group17_project.h"
#include "ps_public.h"
#if (APP_CTRL_BUTTON_SENSOR_EN == 1)
#include "btn_press_ctrl.h"
#endif
#if (APP_CTRL_WAKEUP_IO_EN == 1)
#include "smart_sleep.h"
#endif
#include "blewifi_wifi_FSM.h"

//#include "sensor_button.h"
#include "sensor_door.h"
//#include "sensor_https.h"
//#include "sensor_data.h"
#include "sensor_common.h"
#include "sensor_battery.h"
#include "iot_data.h"
#include "iot_rb_data.h"
#include "wifi_api.h"
#include "iot_data.h"
#include "iot_handle.h"
#include "coolkit_websocket.h"

#define APP_CTRL_RESET_DELAY    (3000)  // ms

osThreadId   g_tAppCtrlTaskId;
osMessageQId g_tAppCtrlQueueId;

osTimerId    g_tAppCtrlSysTimer;
#ifdef __SONOFF__
osTimerId    g_tAppCtrlTestModeId;
#endif
osTimerId    g_tAppCtrlNetworkTimerId;
osTimerId    g_tAppCtrlLedTimer;
osTimerId    g_tAppPostFailLedTimeOutTimerId;
osTimerId    g_ota_timeout_timer = NULL;
osTimerId    g_ble_disconnect_delay_timeout_timer = NULL;
osTimerId    g_cloud_DoorStatusWarning_timeout_timer = NULL;
osTimerId    g_cloud_DailyBatteryPost_timeout_timer = NULL;
osTimerId    g_tAppCtrl_network_stop_delay_timeout_timer = NULL;

EventGroupHandle_t g_tAppCtrlEventGroup;

uint8_t g_u8AppCtrlSysMode;
uint8_t g_u8AppCtrlSysStatus;
uint8_t g_ubAppCtrlLedStatus;
uint8_t g_u8ButtonProcessed;
uint8_t g_u8ShortPressButtonProcessed; //It means the time of button pressed is less than 5s 20191018EL
uint8_t g_nLastPostDatatType = TIMER_POST;
#ifdef __SONOFF__
uint8_t g_u8IsSonoffFactoryDone = false;  //false : App_Ctrl_TestMode_Timeout not execute, true : App_Ctrl_TestMode_Timeout executed
#endif

extern uint8_t g_WifiSSID[WIFI_MAX_LENGTH_OF_SSID];
extern uint8_t g_WifiPassword[64];
extern uint8_t g_WifiSSIDLen;
extern uint8_t g_WifiPasswordLen;

int8_t g_WifiRSSI;

T_MwFim_GP12_HttpPostContent g_tHttpPostContent;
T_MwFim_GP12_HttpHostInfo g_tHostInfo;
T_MwFim_GP17_Hold_Reminder g_tHoldReminder;
T_MwFim_GP17_System_Defense_En g_tSystemDefenseEn;

extern blewifi_ota_t *gTheOta;
extern IoT_Ring_buffer_t g_stIotRbData;
extern uint32_t g_RoamingApInfoTotalCnt;

uint32_t g_u32TickStart = 0, g_u32TickEnd = 0;

uint8_t g_u8MpScanProcessFinished = false; //true : don't need to execute mp scan test , false : need to execute mp scan test
uint8_t g_u8MpScanTestFlag = false; //true : do mp scan  , false : don't do mp scan
uint8_t g_u8MpScanCnt = 0;
uint8_t g_u8MpIsFindTargetAP = false;
uint8_t g_u8MpBlinkingMode = MP_BLINKING_NONE;

E_GpioIdx_t g_eLedIOPort = LED_IO_PORT_WIFI_DISCONNECTED;

uint8_t g_u8IsManuallyConnectScanRetry = false;

#ifdef __SONOFF__
// Check test mode
#define BLEWIFI_CTRL_IS_TEST_MODE_UNCHECK   (-1)    // Init, it can change to normal mode or test mode
#define BLEWIFI_CTRL_IS_TEST_MODE_NO        (0)     // Result is Normal mode, it can not changed mode in this mode
#define BLEWIFI_CTRL_IS_TEST_MODE_YES       (1)     // Result is Test mode, it can not changed mode in this mode
int8_t      g_s8IsTestMode = BLEWIFI_CTRL_IS_TEST_MODE_UNCHECK;    // 0 : No, 1 : Yes
#endif

#ifdef CUS_AUTO_CONNECT_TABLE
uint32_t g_u32AutoConnIntervalTable[AUTO_CONNECT_INTERVAL_TABLE_SIZE] =
{
    30000,
    30000,
    60000,
    60000,
    900000,
};
#endif

uint32_t g_ulaAppCtrlLedInterval[APP_CTRL_LED_NUM] =
{
    LED_TIME_BLE_ON_1,
    LED_TIME_BLE_OFF_1,

    LED_TIME_AUTOCONN_ON_1,
    LED_TIME_AUTOCONN_OFF_1,

    LED_TIME_OTA_ON,
    LED_TIME_OTA_OFF,

    LED_TIME_ALWAYS_OFF,

    LED_TIME_TEST_MODE_ON_1,
    LED_TIME_TEST_MODE_OFF_1,
    LED_TIME_TEST_MODE_ON_2,
    LED_TIME_TEST_MODE_OFF_2,
    LED_TIME_TEST_MODE_ON_3,
    LED_TIME_TEST_MODE_OFF_3,
    LED_TIME_TEST_MODE_ON_4,
    LED_TIME_TEST_MODE_OFF_4,

    LED_TIME_NOT_CNT_SRV_ON_1,
    LED_TIME_NOT_CNT_SRV_OFF_1,
    LED_TIME_NOT_CNT_SRV_ON_2,
    LED_TIME_NOT_CNT_SRV_OFF_2,

#if 1   // 20200528, Terence change offline led as same as NOT_CNT_SRV
    LED_TIME_OFFLINE_ON_1,
    LED_TIME_OFFLINE_OFF_1,
    LED_TIME_OFFLINE_ON_2,
    LED_TIME_OFFLINE_OFF_2,
#else
    LED_TIME_OFFLINE_ON,
    LED_TIME_OFFLINE_OFF,
#endif

    LED_TIME_SHORT_PRESS_ON,

    LED_TIME_BOOT_ON_1,
    LED_TIME_BOOT_OFF_1,
    LED_TIME_BOOT_ON_1,

    LED_MP_NO_ROUTER_ON_1,
    LED_MP_NO_ROUTER_OFF_1,
    LED_MP_NO_SERVER_ON_1,
    LED_MP_NO_SERVER_OFF_1,
    LED_MP_NO_SERVER_ON_2,
    LED_MP_NO_SERVER_OFF_2,

    LED_TIME_DOOR_SWITCH,
    LED_TIME_DAMAGE,

    LED_ALWAYS_ON
};


#if(STRESS_TEST_AUTO_CONNECT == 1)
uint32_t g_u32StressTestCount = 0;
#endif

void App_Sensor_Post_To_Cloud(uint8_t ubType);

static void App_Ctrl_TaskEvtHandler_BleInitComplete(uint32_t u32EvtType, void *pData, uint32_t u32Len);
static void App_Ctrl_TaskEvtHandler_BleStartComplete(uint32_t u32EvtType, void *pData, uint32_t u32Len);
static void App_Ctrl_TaskEvtHandler_BleStopComplete(uint32_t u32EvtType, void *pData, uint32_t u32Len);
static void App_Ctrl_TaskEvtHandler_BleConnection(uint32_t u32EvtType, void *pData, uint32_t u32Len);
static void App_Ctrl_TaskEvtHandler_BleDisconnection(uint32_t u32EvtType, void *pData, uint32_t u32Len);
static void App_Ctrl_TaskEvtHandler_BleDataInd(uint32_t u32EvtType, void *pData, uint32_t u32Len);

static void App_Ctrl_TaskEvtHandler_WifiInitComplete(uint32_t u32EvtType, void *pData, uint32_t u32Len);
static void App_Ctrl_TaskEvtHandler_WifiScanDone(uint32_t u32EvtType, void *pData, uint32_t u32Len);
static void App_Ctrl_TaskEvtHandler_WifiConnection(uint32_t u32EvtType, void *pData, uint32_t u32Len);
static void App_Ctrl_TaskEvtHandler_WifiDisconnection(uint32_t u32EvtType, void *pData, uint32_t u32Len);
static void App_Ctrl_TaskEvtHandler_WifiStopComplete(uint32_t u32EvtType, void *pData, uint32_t u32Len);

static void App_Ctrl_TaskEvtHandler_OtherOtaOn(uint32_t u32EvtType, void *pData, uint32_t u32Len);
static void App_Ctrl_TaskEvtHandler_OtherOtaOff(uint32_t u32EvtType, void *pData, uint32_t u32Len);
static void App_Ctrl_TaskEvtHandler_OtherOtaOffFail(uint32_t u32EvtType, void *pData, uint32_t u32Len);

static void App_Ctrl_TaskEvtHandler_OtherLedTimer(uint32_t u32EvtType, void *pData, uint32_t u32Len);
static void App_Ctrl_TaskEvtHandler_OtherSysTimer(uint32_t u32EvtType, void *pData, uint32_t u32Len);


static void App_Ctrl_TaskEvtHandler_NetworkingStart(uint32_t u32EvtType, void *pData, uint32_t u32Len);
static void App_Ctrl_TaskEvtHandler_NetworkingStop(uint32_t u32EvtType, void *pData, uint32_t u32Len);

#if (APP_CTRL_BUTTON_SENSOR_EN == 1)
static void App_Ctrl_TaskEvtHandler_ButtonStateChange(uint32_t u32EvtType, void *pData, uint32_t u32Len);
static void App_Ctrl_TaskEvtHandler_ButtonDebounceTimeOut(uint32_t u32EvtType, void *pData, uint32_t u32Len);
static void App_Ctrl_TaskEvtHandler_ButtonReleaseTimeOut(uint32_t u32EvtType, void *pData, uint32_t u32Len);
#endif
#if (APP_CTRL_WAKEUP_IO_EN == 1)
static void App_Ctrl_TaskEvtHandler_PsSmartStateChange(uint32_t u32EvtType, void *pData, uint32_t u32Len);
static void App_Ctrl_TaskEvtHandler_PsSmartDebounceTimeOut(uint32_t u32EvtType, void *pData, uint32_t u32Len);
#endif

static void App_Ctrl_TaskEvtHandler_DoorStateChange(uint32_t u32EvtType, void *pData, uint32_t u32Len);
static void App_Ctrl_TaskEvtHandler_DoorDebounceTimeOut(uint32_t u32EvtType, void *pData, uint32_t u32Len);
static void App_Ctrl_TaskEvtHandler_DoorStatusWarningTimeOut(uint32_t u32EvtType, void *pData, uint32_t u32Len);
static void App_Ctrl_TaskEvtHandler_DoorDamageChange(uint32_t u32EvtType, void *pData, uint32_t u32Len);
static void App_Ctrl_TaskEvtHandler_DoorDamageTimeOut(uint32_t u32EvtType, void *pData, uint32_t u32Len);

static void App_Ctrl_TaskEvtHandler_HttpPostDataInd(uint32_t u32EvtType, void *pData, uint32_t u32Len);
#ifdef __SONOFF__
static void APP_Ctrl_TaskEvtHandler_IsTestModeAtFactory(uint32_t u32EvtType, void *pData, uint32_t u32Len);
static void APP_Ctrl_TaskEvtHandler_IsTestModeTimeout(uint32_t u32EvtType, void *pData, uint32_t u32Len);
#endif

static void App_Ctrl_TaskEvtHandler_DailyBatteryPost(uint32_t u32EvtType, void *pData, uint32_t u32Len);

static void App_Ctrl_TaskEvtHandler_SetHoldRemider(uint32_t u32EvtType, void *pData, uint32_t u32Len);
static void App_Ctrl_TaskEvtHandler_NetworkStop_Delay(uint32_t u32EvtType, void *pData, uint32_t u32Len);
static void App_Ctrl_TaskEvtHandler_AutoConnectTimeOut(uint32_t u32EvtType, void *pData, uint32_t u32Len);
static void APP_Ctrl_TaskEvtHandler_PostFail(uint32_t u32EvtType, void *pData, uint32_t u32Len);

static App_Ctrl_EvtHandlerTbl_t g_tAppCtrlEvtHandlerTbl[] =
{
    {APP_CTRL_MSG_BLE_INIT_COMPLETE,                App_Ctrl_TaskEvtHandler_BleInitComplete},
    {APP_CTRL_MSG_BLE_START_COMPLETE,               App_Ctrl_TaskEvtHandler_BleStartComplete},
    {APP_CTRL_MSG_BLE_STOP_COMPLETE,                App_Ctrl_TaskEvtHandler_BleStopComplete},
    {APP_CTRL_MSG_BLE_CONNECTION,                   App_Ctrl_TaskEvtHandler_BleConnection},
    {APP_CTRL_MSG_BLE_DISCONNECTION,                App_Ctrl_TaskEvtHandler_BleDisconnection},
    {APP_CTRL_MSG_BLE_DATA_IND,                     App_Ctrl_TaskEvtHandler_BleDataInd},

    {APP_CTRL_MSG_WIFI_INIT_COMPLETE,               App_Ctrl_TaskEvtHandler_WifiInitComplete},
    {APP_CTRL_MSG_WIFI_SCAN_DONE,                   App_Ctrl_TaskEvtHandler_WifiScanDone},
    {APP_CTRL_MSG_WIFI_CONNECTION,                  App_Ctrl_TaskEvtHandler_WifiConnection},
    {APP_CTRL_MSG_WIFI_DISCONNECTION,               App_Ctrl_TaskEvtHandler_WifiDisconnection},
    {APP_CTRL_MSG_WIFI_STOP_COMPLETE,               App_Ctrl_TaskEvtHandler_WifiStopComplete},

    {APP_CTRL_MSG_OTHER_OTA_ON,                     App_Ctrl_TaskEvtHandler_OtherOtaOn},
    {APP_CTRL_MSG_OTHER_OTA_OFF,                    App_Ctrl_TaskEvtHandler_OtherOtaOff},
    {APP_CTRL_MSG_OTHER_OTA_OFF_FAIL,               App_Ctrl_TaskEvtHandler_OtherOtaOffFail},

    {APP_CTRL_MSG_OTHER_LED_TIMER,                  App_Ctrl_TaskEvtHandler_OtherLedTimer},
    {APP_CTRL_MSG_OTHER_SYS_TIMER,                  App_Ctrl_TaskEvtHandler_OtherSysTimer},

    {APP_CTRL_MSG_NETWORKING_START,                 App_Ctrl_TaskEvtHandler_NetworkingStart},
    {APP_CTRL_MSG_NETWORKING_STOP,                  App_Ctrl_TaskEvtHandler_NetworkingStop},

#if (APP_CTRL_BUTTON_SENSOR_EN==1)
    {APP_CTRL_MSG_BUTTON_STATE_CHANGE,              App_Ctrl_TaskEvtHandler_ButtonStateChange},
    {APP_CTRL_MSG_BUTTON_DEBOUNCE_TIMEOUT,          App_Ctrl_TaskEvtHandler_ButtonDebounceTimeOut},
    {APP_CTRL_MSG_BUTTON_RELEASE_TIMEOUT,           App_Ctrl_TaskEvtHandler_ButtonReleaseTimeOut},
#endif
#if (APP_CTRL_WAKEUP_IO_EN == 1)
    {APP_CTRL_MSG_PS_SMART_STATE_CHANGE,            App_Ctrl_TaskEvtHandler_PsSmartStateChange},
    {APP_CTRL_MSG_PS_SMART_DEBOUNCE_TIMEOUT,        App_Ctrl_TaskEvtHandler_PsSmartDebounceTimeOut},
#endif

    {APP_CTRL_MSG_DOOR_STATECHANGE,                 App_Ctrl_TaskEvtHandler_DoorStateChange},
    {APP_CTRL_MSG_DOOR_DEBOUNCETIMEOUT,             App_Ctrl_TaskEvtHandler_DoorDebounceTimeOut},
    {APP_CTRL_MSG_DOOR_STATUSWARNING_TIMEOUT,       App_Ctrl_TaskEvtHandler_DoorStatusWarningTimeOut},
    {APP_CTRL_MSG_DOOR_DAMAGE_CHANGE,               App_Ctrl_TaskEvtHandler_DoorDamageChange},
    {APP_CTRL_MSG_DOOR_DAMAGE_TIMEOUT,              App_Ctrl_TaskEvtHandler_DoorDamageTimeOut},

    {APP_CTRL_MSG_HTTP_POST_DATA_IND,               App_Ctrl_TaskEvtHandler_HttpPostDataInd},

#ifdef __SONOFF__
    {APP_CTRL_MSG_IS_TEST_MODE_AT_FACTORY,          APP_Ctrl_TaskEvtHandler_IsTestModeAtFactory},
    {APP_CTRL_MSG_IS_TEST_MODE_TIMEOUT,             APP_Ctrl_TaskEvtHandler_IsTestModeTimeout},
#endif

    {APP_CTRL_MSG_DOOR_DAILY_BATTERY_POST_TIMEOUT,  App_Ctrl_TaskEvtHandler_DailyBatteryPost},

    {APP_CTRL_MSG_DOOR_SET_HOLD_REMIDER,            App_Ctrl_TaskEvtHandler_SetHoldRemider},

    {APP_CTRL_MSG_NETWORK_STOP_DELAY_TIMEOUT,       App_Ctrl_TaskEvtHandler_NetworkStop_Delay},
    {APP_CTRL_MSG_AUTO_CONNECT_TIMEOUT,             App_Ctrl_TaskEvtHandler_AutoConnectTimeOut},
    {APP_CTRL_MSG_DATA_POST_FAIL,                   APP_Ctrl_TaskEvtHandler_PostFail},

    {0xFFFFFFFF,                                    NULL}
};

#ifdef __SONOFF__
void FinalizedATWIFIcmd()
{
    if (true == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_TEST_MODE))
    {
         printf("\033[1;32;40m");
         printf("[%s %d] do wifi_auto_connect_reset!!!");
         printf("\033[0m");
         printf("\n");
         BleWifi_Wifi_Reset_Req();

         #ifdef TEST_MODE_DEBUG_ENABLE
         msg_print_uart1("cleanAutoConnectInfo\r\n");
         #endif
     }
}
#endif

void App_Door_Sensor_Set_Defense(uint8_t u8En)
{
    if (g_tSystemDefenseEn.u8En == DOOR_DEFENSE_OFF)
    {
        g_tSystemDefenseEn.u8En = u8En;

        if (u8En == DOOR_DEFENSE_ON)
        {
#ifdef __SONOFF__
            // SONOFF door sensor doesn't have damage function
#else
            // check damage
            if (GPIO_LEVEL_HIGH == Hal_Vic_GpioInput(DOOR_SENSOR_DAMAGE_IO_PORT))
            {
                // Trigger to send http data
                App_Sensor_Post_To_Cloud(DOOR_SENSOR_DAMAGE);
                if((true == g_u8MpScanProcessFinished)
                    && (false == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_WIFI_GOT_IP)
                    && (false == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_NETWORK))))
                {
                    printf("---->AutoConn...DoorDamage\n");
                    // the idle of the connection retry
                    BleWifi_Wifi_Start_Conn(NULL);
                }
            }
#endif

            // check reminder
            if (g_tHoldReminder.u8Enable == true)
            {
                App_Ctrl_MsgSend(APP_CTRL_MSG_DOOR_SET_HOLD_REMIDER, (uint8_t *)&g_tHoldReminder, sizeof(T_MwFim_GP17_Hold_Reminder));
            }
        }
    }

    g_tSystemDefenseEn.u8En = u8En;

    if(MW_FIM_OK != MwFim_FileWrite(MW_FIM_IDX_GP17_PROJECT_SYSTEM_DEFENSE_EN, 0, MW_FIM_GP17_SYSTEM_DEFENSE_EN_SIZE, (uint8_t*)&g_tSystemDefenseEn))
    {
        printf("Write fim System_Defense_En fail\r\n");
    }
}

void App_Sensor_Post_To_Cloud(uint8_t ubType)
{
    Sensor_Data_t tSensorData = {0};
    IoT_Properity_t tProperity;
    uint8_t u8NeedColudConnectionCheck = 0;

    // check the CurrentDefensee
    if(g_tSystemDefenseEn.u8En == DOOR_DEFENSE_OFF)
    {
        if((ubType == DOOR_SENSOR_DAMAGE) || (ubType == DOOR_HOLD_REMINDER_WARING))
        {
            printf("g_tSystemDefenseEn.u8En = DOOR_DEFENSE_OFF , ignore post type %u\r\n",ubType);
            return ;
        }
    }

    tSensorData.ubaDoorStatus = BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_DOOR);
    tSensorData.ubaType = ubType;
    tSensorData.u64TimeStamp = Coolkit_Cloud_GetNewSeqID();

    tProperity.ubData = malloc(sizeof(Sensor_Data_t));

    if(tProperity.ubData == NULL)
    {
        printf("malloc failed\n");
        return;
    }
    memset(tProperity.ubData, 0, sizeof(Sensor_Data_t));
    memcpy(tProperity.ubData, (char*)&tSensorData, sizeof(Sensor_Data_t));
    tProperity.ulLen = sizeof(Sensor_Data_t);

    //Debug
    printf("debug -- ubType %u Post Door status: %s(ts=%lu)\n",ubType , tSensorData.ubaDoorStatus?"Close":"Open", BleWifi_COM_SntpTimestampGet());

    IoT_Ring_Buffer_Push(&g_stIotRbData, &tProperity);

    //not ready, disable it fist
    u8NeedColudConnectionCheck = IOT_DATA_POST_CHECK_CLOUD_CONNECTION;
    Iot_Data_TxTask_MsgSend(IOT_DATA_TX_MSG_CLOUD_POST, &u8NeedColudConnectionCheck, sizeof(u8NeedColudConnectionCheck));
}

void App_Ctrl_DoLedDisplay(void)
{
    if(!g_u8ShortPressButtonProcessed) //if short press button is processed, showing this light status is the 1st priority 20191018EL
    {
        switch (g_ubAppCtrlLedStatus)
        {
            case APP_CTRL_LED_BLE_ON_1:          // pair #1
#ifdef __SONOFF__
                Hal_Vic_GpioOutput(LED_IO_PORT_WIFI_CONNECTED, GPIO_LEVEL_HIGH);
#else
                Hal_Vic_GpioOutput(LED_IO_PORT_WIFI_DISCONNECTED, GPIO_LEVEL_HIGH);
                Hal_Vic_GpioOutput(LED_IO_PORT_WIFI_CONNECTED, GPIO_LEVEL_LOW);
#endif
                break;
            case APP_CTRL_LED_AUTOCONN_ON_1:     // pair #2
            case APP_CTRL_LED_OTA_ON:            // pair #3
            case APP_CTRL_LED_TEST_MODE_ON_1:      // pair #4
            case APP_CTRL_LED_TEST_MODE_ON_2:      // pair #4
            case APP_CTRL_LED_TEST_MODE_ON_3:      // pair #4
            case APP_CTRL_LED_TEST_MODE_ON_4:      // pair #4
            case APP_CTRL_LED_NOT_CNT_SRV_ON_1:  // pair #5
            case APP_CTRL_LED_NOT_CNT_SRV_ON_2:  // pair #5
            case APP_CTRL_LED_OFFLINE_ON_1:      // pair #6
            case APP_CTRL_LED_OFFLINE_ON_2:      // pair #6
            case APP_CTRL_LED_BOOT_ON_1:         // pair #7//Goter
            case APP_CTRL_LED_BOOT_ON_2:         // pair #7//Goter
            case APP_CTRL_LED_SHORT_PRESS_ON:    // LEO one blink for short press//Goter
            case APP_CTRL_LED_MP_NO_ROUTER_ON_1:
            case APP_CTRL_LED_MP_NO_SERVER_ON_1:
            case APP_CTRL_LED_MP_NO_SERVER_ON_2:
            case APP_CTRL_LED_DOOR_SWITCH:
            case APP_CTRL_LED_DAMAGE:
            case APP_CTRL_LED_ALWAYS_ON:
#ifdef __SONOFF__
                Hal_Vic_GpioOutput(LED_IO_PORT_WIFI_CONNECTED, GPIO_LEVEL_HIGH);
#else
                if(g_eLedIOPort == LED_IO_PORT_WIFI_DISCONNECTED)
                {
                    Hal_Vic_GpioOutput(LED_IO_PORT_WIFI_DISCONNECTED, GPIO_LEVEL_HIGH);
                    Hal_Vic_GpioOutput(LED_IO_PORT_WIFI_CONNECTED, GPIO_LEVEL_LOW);
                }
                else
                {
                    Hal_Vic_GpioOutput(LED_IO_PORT_WIFI_DISCONNECTED, GPIO_LEVEL_LOW);
                    Hal_Vic_GpioOutput(LED_IO_PORT_WIFI_CONNECTED, GPIO_LEVEL_HIGH);
                }
#endif
                break;

            case APP_CTRL_LED_BLE_OFF_1:          // pair #1
#ifdef __SONOFF__
                Hal_Vic_GpioOutput(LED_IO_PORT_WIFI_CONNECTED, GPIO_LEVEL_LOW);
#else
                Hal_Vic_GpioOutput(LED_IO_PORT_WIFI_DISCONNECTED, GPIO_LEVEL_LOW);
                Hal_Vic_GpioOutput(LED_IO_PORT_WIFI_CONNECTED, GPIO_LEVEL_LOW);
#endif
                break;
            case APP_CTRL_LED_AUTOCONN_OFF_1:     // pair #2
            case APP_CTRL_LED_OTA_OFF:            // pair #3
            case APP_CTRL_LED_TEST_MODE_OFF_1:      // pair #4
            case APP_CTRL_LED_TEST_MODE_OFF_2:      // pair #4
            case APP_CTRL_LED_TEST_MODE_OFF_3:      // pair #4
            case APP_CTRL_LED_TEST_MODE_OFF_4:      // pair #4
            case APP_CTRL_LED_NOT_CNT_SRV_OFF_1:  // pair #5
            case APP_CTRL_LED_NOT_CNT_SRV_OFF_2:  // pair #5
            case APP_CTRL_LED_OFFLINE_OFF_1:      // pair #6
            case APP_CTRL_LED_OFFLINE_OFF_2:      // pair #6
            case APP_CTRL_LED_BOOT_OFF_1:         // pair #7//Goter
            case APP_CTRL_LED_MP_NO_ROUTER_OFF_1:
            case APP_CTRL_LED_MP_NO_SERVER_OFF_1:
            case APP_CTRL_LED_MP_NO_SERVER_OFF_2:
            case APP_CTRL_LED_ALWAYS_OFF:         // LED always off
#ifdef __SONOFF__
                Hal_Vic_GpioOutput(LED_IO_PORT_WIFI_CONNECTED, GPIO_LEVEL_LOW);
#else
                Hal_Vic_GpioOutput(LED_IO_PORT_WIFI_DISCONNECTED, GPIO_LEVEL_LOW);
                Hal_Vic_GpioOutput(LED_IO_PORT_WIFI_CONNECTED, GPIO_LEVEL_LOW);
#endif
                break;
            // error handle
            default:
#ifdef __SONOFF__
                Hal_Vic_GpioOutput(LED_IO_PORT_WIFI_CONNECTED, GPIO_LEVEL_LOW);
#else
                Hal_Vic_GpioOutput(LED_IO_PORT_WIFI_DISCONNECTED, GPIO_LEVEL_LOW);
                Hal_Vic_GpioOutput(LED_IO_PORT_WIFI_CONNECTED, GPIO_LEVEL_LOW);
#endif
                return;
        }
    }

    // start the led timer
    osTimerStop(g_tAppCtrlLedTimer);
    osTimerStart(g_tAppCtrlLedTimer, g_ulaAppCtrlLedInterval[g_ubAppCtrlLedStatus]);
}

void App_Ctrl_LedStatusChange(void)
{
    // LED status: MP Test > Button press >  OTA > BLE || BLE Adv > Wifi > WiFi Autoconnect > None

#ifdef __SONOFF__
    // Test Mode
    if (true == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_TEST_MODE))
    {
        // status change
        g_ubAppCtrlLedStatus = APP_CTRL_LED_TEST_MODE_OFF_4;
        App_Ctrl_DoLedDisplay();
    }
    else if(true == g_u8MpIsFindTargetAP)
#else
    if(true == g_u8MpIsFindTargetAP)
#endif
    {
        // Button press
        if (true == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_BUTTON_PRESS))
        {
            // status change
            g_eLedIOPort = LED_IO_PORT_WIFI_CONNECTED;
            g_ubAppCtrlLedStatus = APP_CTRL_LED_ALWAYS_ON;
        }
#ifdef __SONOFF__
        // door sensor
        else if (true == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_DOOR))
        {
            // status change
            g_eLedIOPort = LED_IO_PORT_WIFI_CONNECTED;
            g_ubAppCtrlLedStatus = APP_CTRL_LED_ALWAYS_ON;
        }
#endif
        else if(g_u8MpBlinkingMode == MP_BLINKING_NO_SERVER)
        {
            g_eLedIOPort = LED_IO_PORT_WIFI_DISCONNECTED;
            g_ubAppCtrlLedStatus = APP_CTRL_LED_MP_NO_SERVER_ON_1;
        }
        else if(g_u8MpBlinkingMode == MP_BLINKING_NO_ROUTER)
        {
            g_eLedIOPort = LED_IO_PORT_WIFI_DISCONNECTED;
            g_ubAppCtrlLedStatus = APP_CTRL_LED_MP_NO_ROUTER_ON_1;
        }
        App_Ctrl_DoLedDisplay();
    }

    // Button press
    else if (true == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_BUTTON_PRESS))
    {
        // status change
        g_ubAppCtrlLedStatus = APP_CTRL_LED_ALWAYS_ON;
        App_Ctrl_DoLedDisplay();
    }
#ifdef __SONOFF__
    // OTA
    else if (true == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_OTA))
    {
        // status change
        g_ubAppCtrlLedStatus = APP_CTRL_LED_OTA_OFF;
        App_Ctrl_DoLedDisplay();
    }
#endif
    // Network now
    else if (true == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_NETWORK))
    {
        // status change
        g_ubAppCtrlLedStatus = APP_CTRL_LED_BLE_OFF_1;
        App_Ctrl_DoLedDisplay();
    }
#ifdef __SONOFF__
    // Wifi Auto
    /*else if (true == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_WIFI_AUTOCONN_LED))
    {
        // status change
        g_ubAppCtrlLedStatus = APP_CTRL_LED_AUTOCONN_OFF_1;
        App_Ctrl_DoLedDisplay();
    }*/
    // Device offline
    else if (true == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_OFFLINE))
    {
        g_ubAppCtrlLedStatus = APP_CTRL_LED_OFFLINE_OFF_2;
        App_Ctrl_DoLedDisplay();
    }
    // Got IP but cannot connect to server
    else if (true == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_NOT_CNT_SRV))
    {
        // status change
        g_ubAppCtrlLedStatus = APP_CTRL_LED_NOT_CNT_SRV_OFF_2;
        App_Ctrl_DoLedDisplay();
    }
#endif
    // Wi-Fi is connected
    else if (true == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_WIFI_GOT_IP))
    {
        // status change
        g_ubAppCtrlLedStatus = APP_CTRL_LED_ALWAYS_OFF;
        App_Ctrl_DoLedDisplay();
    }

    // None
    else
    {
        // status change
        g_ubAppCtrlLedStatus = APP_CTRL_LED_ALWAYS_OFF;
        App_Ctrl_DoLedDisplay();
    }
}

void App_Ctrl_LedStatusAutoUpdate(void)
{
    switch (g_ubAppCtrlLedStatus)
    {
        case APP_CTRL_LED_BLE_ON_1:
            g_ubAppCtrlLedStatus = APP_CTRL_LED_BLE_OFF_1;
            break;

        case APP_CTRL_LED_BLE_OFF_1:
            g_ubAppCtrlLedStatus = APP_CTRL_LED_BLE_ON_1;
            break;

        case APP_CTRL_LED_AUTOCONN_ON_1:
            g_ubAppCtrlLedStatus = APP_CTRL_LED_AUTOCONN_OFF_1;
            break;

        case APP_CTRL_LED_AUTOCONN_OFF_1:
            g_ubAppCtrlLedStatus = APP_CTRL_LED_AUTOCONN_ON_1;
            break;

        case APP_CTRL_LED_OTA_ON:
            g_ubAppCtrlLedStatus = APP_CTRL_LED_OTA_OFF;
            break;

        case APP_CTRL_LED_OTA_OFF:
            g_ubAppCtrlLedStatus = APP_CTRL_LED_OTA_ON;
            break;

        case APP_CTRL_LED_ALWAYS_OFF:
            break;

        case APP_CTRL_LED_TEST_MODE_ON_1:
            g_ubAppCtrlLedStatus = APP_CTRL_LED_TEST_MODE_OFF_1;
            break;

        case APP_CTRL_LED_TEST_MODE_OFF_1:
            g_ubAppCtrlLedStatus = APP_CTRL_LED_TEST_MODE_ON_2;
            break;

        case APP_CTRL_LED_TEST_MODE_ON_2:
            g_ubAppCtrlLedStatus = APP_CTRL_LED_TEST_MODE_OFF_2;
            break;

        case APP_CTRL_LED_TEST_MODE_OFF_2:
            g_ubAppCtrlLedStatus = APP_CTRL_LED_TEST_MODE_ON_3;
            break;

        case APP_CTRL_LED_TEST_MODE_ON_3:
            g_ubAppCtrlLedStatus = APP_CTRL_LED_TEST_MODE_OFF_3;
            break;

        case APP_CTRL_LED_TEST_MODE_OFF_3:
            g_ubAppCtrlLedStatus = APP_CTRL_LED_TEST_MODE_ON_4;
            break;

        case APP_CTRL_LED_TEST_MODE_ON_4:
            g_ubAppCtrlLedStatus = APP_CTRL_LED_TEST_MODE_OFF_4;
            break;

        case APP_CTRL_LED_TEST_MODE_OFF_4:
            g_ubAppCtrlLedStatus = APP_CTRL_LED_TEST_MODE_ON_1;
            break;

        case APP_CTRL_LED_NOT_CNT_SRV_ON_1:
            g_ubAppCtrlLedStatus = APP_CTRL_LED_NOT_CNT_SRV_OFF_1;
            break;

        case APP_CTRL_LED_NOT_CNT_SRV_OFF_1:
            g_ubAppCtrlLedStatus = APP_CTRL_LED_NOT_CNT_SRV_ON_2;
            break;

        case APP_CTRL_LED_NOT_CNT_SRV_ON_2:
            g_ubAppCtrlLedStatus = APP_CTRL_LED_NOT_CNT_SRV_OFF_2;
            break;

        case APP_CTRL_LED_NOT_CNT_SRV_OFF_2:
            g_ubAppCtrlLedStatus = APP_CTRL_LED_NOT_CNT_SRV_ON_1;
            break;

        case APP_CTRL_LED_OFFLINE_ON_1:
            g_ubAppCtrlLedStatus = APP_CTRL_LED_OFFLINE_OFF_1;
            break;

        case APP_CTRL_LED_MP_NO_ROUTER_ON_1:
            g_ubAppCtrlLedStatus = APP_CTRL_LED_MP_NO_ROUTER_OFF_1;
            break;

        case APP_CTRL_LED_MP_NO_ROUTER_OFF_1:
            g_ubAppCtrlLedStatus = APP_CTRL_LED_MP_NO_ROUTER_ON_1;
            break;

        case APP_CTRL_LED_MP_NO_SERVER_ON_1:
            g_ubAppCtrlLedStatus = APP_CTRL_LED_MP_NO_SERVER_OFF_1;
            break;

        case APP_CTRL_LED_MP_NO_SERVER_OFF_1:
            g_ubAppCtrlLedStatus = APP_CTRL_LED_MP_NO_SERVER_ON_2;
            break;

        case APP_CTRL_LED_MP_NO_SERVER_ON_2:
            g_ubAppCtrlLedStatus = APP_CTRL_LED_MP_NO_SERVER_OFF_2;
            break;

        case APP_CTRL_LED_MP_NO_SERVER_OFF_2:
            g_ubAppCtrlLedStatus = APP_CTRL_LED_MP_NO_SERVER_ON_1;
            break;

        case APP_CTRL_LED_DOOR_SWITCH:
            App_Ctrl_LedStatusChange(); //restore led status
            break;

        case APP_CTRL_LED_DAMAGE:
            App_Ctrl_LedStatusChange(); //restore led status
            break;

        case APP_CTRL_LED_ALWAYS_ON:
            break;

#if 1   // 20200528, Terence change offline led as same as NOT_CNT_SRV
        case APP_CTRL_LED_OFFLINE_OFF_1:
            g_ubAppCtrlLedStatus = APP_CTRL_LED_OFFLINE_ON_2;
            break;
        case APP_CTRL_LED_OFFLINE_ON_2:
            g_ubAppCtrlLedStatus = APP_CTRL_LED_OFFLINE_OFF_2;
            break;
        case APP_CTRL_LED_OFFLINE_OFF_2:
            g_ubAppCtrlLedStatus = APP_CTRL_LED_OFFLINE_ON_1;
            break;
#else
        case BLEWIFI_CTRL_LED_OFFLINE_OFF_1:
            g_ubAppCtrlLedStatus = BLEWIFI_CTRL_LED_OFFLINE_ON_1;
            break;
#endif
#if 0  //Goter
        case BLEWIFI_CTRL_LED_SHORT_PRESS_ON:
            g_ubAppCtrlLedStatus = APP_CTRL_LED_ALWAYS_OFF;
            break;

        case BLEWIFI_CTRL_LED_BOOT_ON_1:
            g_ubAppCtrlLedStatus = APP_CTRL_LED_BOOT_OFF_1;
            break;

        case APP_CTRL_LED_BOOT_OFF_1:
            g_ubAppCtrlLedStatus = APP_CTRL_LED_BOOT_ON_2;
            break;

        case APP_CTRL_LED_BOOT_ON_2:
            g_ubAppCtrlLedStatus = APP_CTRL_LED_ALWAYS_OFF;
            break;

#endif

        default:
            break;
    }
}


static void App_Ctrl_TaskEvtHandler_HttpPostDataInd(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
    BLEWIFI_INFO("BLEWIFI: MSG BLEWIFI_CTRL_MSG_HTTP_POST_DATA_IND \r\n");

    // Trigger to send http data
    App_Sensor_Post_To_Cloud(TIMER_POST);
    if((true == g_u8MpScanProcessFinished)
        && (false == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_WIFI_GOT_IP)
        && (false == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_NETWORK))))
    {
        printf("---->AutoConn...TimerPost\n");
        // the idle of the connection retry
        BleWifi_Wifi_Start_Conn(NULL);
    }
}

static void App_Ctrl_TaskEvtHandler_DailyBatteryPost(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
    BLEWIFI_INFO("BLEWIFI: MSG BLEWIFI_CTRL_MSG_HTTP_POST_DATA_IND \r\n");

    // Trigger to send http data
    App_Sensor_Post_To_Cloud(TIMER_POST);
    if((true == g_u8MpScanProcessFinished)
        && (false == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_WIFI_GOT_IP)
        && (false == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_NETWORK))))
    {
        printf("---->AutoConn...DailyBatteryPost\n");
        // the idle of the connection retry
        BleWifi_Wifi_Start_Conn(NULL);
    }
}

static void App_Ctrl_TaskEvtHandler_SetHoldRemider(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
    T_MwFim_GP17_Hold_Reminder *pstHoldReminder = (T_MwFim_GP17_Hold_Reminder *)pData;
    memcpy(&g_tHoldReminder , pstHoldReminder , sizeof(T_MwFim_GP17_Hold_Reminder));

    if((g_tHoldReminder.u8Enable == false) || (g_tSystemDefenseEn.u8En == DOOR_DEFENSE_OFF))
    {
        osTimerStop(g_cloud_DoorStatusWarning_timeout_timer);
    }
    else
    {
        if(BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_DOOR) == true) //door close
        {
            if(g_tHoldReminder.u8Switch == DOOR_WARING_FLAG_CLOSE)
            {
                osTimerStop(g_cloud_DoorStatusWarning_timeout_timer);
                osTimerStart(g_cloud_DoorStatusWarning_timeout_timer, g_tHoldReminder.u16Time * CONVERT_MINUTE_TO_MILLISECOND);
            }
        }
        else // door open
        {
            if(g_tHoldReminder.u8Switch == DOOR_WARING_FLAG_OPEN)
            {
                osTimerStop(g_cloud_DoorStatusWarning_timeout_timer);
                osTimerStart(g_cloud_DoorStatusWarning_timeout_timer, g_tHoldReminder.u16Time * CONVERT_MINUTE_TO_MILLISECOND);
            }
        }
    }

    if(MW_FIM_OK != MwFim_FileWrite(MW_FIM_IDX_GP17_PROJECT_HOLD_REMINDER, 0, MW_FIM_GP17_HOLD_REMINDER_SIZE, (uint8_t*)&g_tHoldReminder))
    {
        printf("Write fim HOLD_REMINDER fail\r\n");
    }
}

static void App_Ctrl_TaskEvtHandler_NetworkStop_Delay(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
    App_Ctrl_MsgSend(APP_CTRL_MSG_NETWORKING_STOP , NULL , 0);
}

static void App_Ctrl_TaskEvtHandler_AutoConnectTimeOut(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
    BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_WIFI_AUTOCONN_LED, false);
    App_Ctrl_LedStatusChange();
    BleWifi_Wifi_Stop_Conn();
}

static void APP_Ctrl_TaskEvtHandler_PostFail(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
#ifdef __SONOFF__
    BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_NOT_CNT_SRV, true);
    App_Ctrl_LedStatusChange();
    osTimerStop(g_tAppPostFailLedTimeOutTimerId);
    osTimerStart(g_tAppPostFailLedTimeOutTimerId , BLEWIFI_POST_FAIL_LED_MAX);
#endif
}

#ifdef __SONOFF__
static void APP_Ctrl_TaskEvtHandler_IsTestModeAtFactory(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
    if ( g_s8IsTestMode != BLEWIFI_CTRL_IS_TEST_MODE_UNCHECK)
    {   // Never write data or timeout
        msg_print_uart1("\r\n>");
        return;
    }
    g_s8IsTestMode = BLEWIFI_CTRL_IS_TEST_MODE_YES;

    // stop the sys timer
    osTimerStop(g_tAppCtrlSysTimer);
    // stop the test mode timer
    osTimerStop(g_tAppCtrlTestModeId);

    // start test mode of LED blink
    BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_TEST_MODE , true);
    App_Ctrl_LedStatusChange();

    msg_print_uart1("factory mode\r\n");
}

static void APP_Ctrl_TaskEvtHandler_IsTestModeTimeout(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
    if ( g_s8IsTestMode != BLEWIFI_CTRL_IS_TEST_MODE_UNCHECK)
    { // at+factory
        msg_print_uart1("\r\n>");
        return;
    }
    g_s8IsTestMode = BLEWIFI_CTRL_IS_TEST_MODE_NO;

    msg_print_uart1("normal mode\r\n");

    g_u8IsSonoffFactoryDone = true;
    App_Ctrl_MsgSend(APP_CTRL_MSG_WIFI_INIT_COMPLETE, NULL, 0);
}
#endif

static void App_Ctrl_TaskEvtHandler_OtherLedTimer(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
    BLEWIFI_INFO("BLEWIFI: MSG BLEWIFI_CTRL_MSG_OTHER_LED_TIMER \r\n");
    App_Ctrl_LedStatusAutoUpdate();
    App_Ctrl_DoLedDisplay();
}

void App_Ctrl_LedTimeout(void const *argu)
{
    App_Ctrl_MsgSend(APP_CTRL_MSG_OTHER_LED_TIMER, NULL, 0);
}

void App_Wifi_AutoConnect_Timeout(void const *argu)
{
    App_Ctrl_MsgSend(APP_CTRL_MSG_AUTO_CONNECT_TIMEOUT, NULL, 0);
}

static void App_PostFailLedTimeOutCallBack(void const *argu)
{
    BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_OFFLINE, false);
    BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_NOT_CNT_SRV, false);
    App_Ctrl_LedStatusChange();
}

static void App_Ota_TimeOutCallBack(void const *argu)
{
    BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_OTA, false);
}

static void App_Ble_Disconnect_Delay_TimeOutCallBack(void const *argu)
{
    if ((false == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup,APP_CTRL_EVENT_BIT_NETWORK)) && (true == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_BLE_CONNECTED)))
    {
        BleWifi_Ble_Stop();
    }
}

static void App_Door_Status_Warning_TimeOutCallBack(void const *argu)
{
    App_Ctrl_MsgSend(APP_CTRL_MSG_DOOR_STATUSWARNING_TIMEOUT, NULL, 0);
}

static void App_Daily_Battery_Post_TimeOutCallBack(void const *argu)
{
    App_Ctrl_MsgSend(APP_CTRL_MSG_DOOR_DAILY_BATTERY_POST_TIMEOUT, NULL, 0);
}

static void App_Network_Stop_Delay_TimeOutCallBack(void const *argu)
{
    App_Ctrl_MsgSend(APP_CTRL_MSG_NETWORK_STOP_DELAY_TIMEOUT, NULL, 0);
}

void App_Ctrl_SysModeSet(uint8_t u8Mode)
{
    g_u8AppCtrlSysMode = u8Mode;
}

uint8_t App_Ctrl_SysModeGet(void)
{
    return g_u8AppCtrlSysMode;
}

void App_Ctrl_SysStatusChange(void)
{
    uint8_t ubSysMode;

    // get the settings of system mode
    ubSysMode = App_Ctrl_SysModeGet();

    // change from init to normal
    if (g_u8AppCtrlSysStatus == APP_CTRL_SYS_INIT)
    {
        g_u8AppCtrlSysStatus = APP_CTRL_SYS_NORMAL;

        /* Power saving settings */
        if (MW_FIM_SYS_MODE_USER == ubSysMode)
        {
#if (APP_CTRL_WAKEUP_IO_EN == 1)
            App_Ps_Smart_Init(BLEWIFI_APP_CTRL_WAKEUP_IO_PORT, ubSysMode, BLEWIFI_COM_POWER_SAVE_EN);
#else
            ps_smart_sleep(BLEWIFI_COM_POWER_SAVE_EN);
#endif
        }

//        // start the sys timer
//        osTimerStop(g_tAppCtrlSysTimer);
//        osTimerStart(g_tAppCtrlSysTimer, APP_COM_SYS_TIME_NORMAL);
    }
    // change from normal to ble off
    else if (g_u8AppCtrlSysStatus == APP_CTRL_SYS_NORMAL)
    {
        g_u8AppCtrlSysStatus = APP_CTRL_SYS_BLE_OFF;

//        // change the advertising time
//        BleWifi_Ble_AdvertisingTimeChange(BLEWIFI_BLE_ADVERTISEMENT_INTERVAL_PS_MIN, BLEWIFI_BLE_ADVERTISEMENT_INTERVAL_PS_MAX);
    }
}

uint8_t App_Ctrl_SysStatusGet(void)
{
    return g_u8AppCtrlSysStatus;
}

void App_Ctrl_SysTimeout(void const *argu)
{
    App_Ctrl_MsgSend(APP_CTRL_MSG_OTHER_SYS_TIMER, NULL, 0);
}

#ifdef __SONOFF__
static void App_Ctrl_TestMode_Timeout(void const *argu)
{
    App_Ctrl_MsgSend(APP_CTRL_MSG_IS_TEST_MODE_TIMEOUT, NULL, 0);
}
#endif

static void App_Ctrl_TaskEvtHandler_BleInitComplete(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
//    uint32_t u32BleTimeOut = APP_BLE_ADV_TIMEOUT;

    printf("[ATS]BLE init complete\r\n");

    BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_BLE_INIT_DONE , true);

//    App_Ctrl_MsgSend(APP_CTRL_MSG_NETWORKING_START, (uint8_t *)&u32BleTimeOut, sizeof(u32BleTimeOut));
}

static void App_Ctrl_TaskEvtHandler_BleStartComplete(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
    printf("[ATS]BLE start\r\n");

    BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_BLE_START , true);
}

static void App_Ctrl_TaskEvtHandler_BleStopComplete(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
    printf("[ATS]BLE stop\r\n");

    BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_BLE_START , false);
}

static void App_Ctrl_TaskEvtHandler_BleConnection(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
    printf("[ATS]BLE connected\r\n");

#ifdef __BLEWIFI_TRANSPARENT__
    msg_print_uart1("+BLECONN:PEER CONNECTION\n");
#endif

    BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_BLE_CONNECTED , true);
}

static void App_Ctrl_TaskEvtHandler_BleDisconnection(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
    uint8_t *pu8Reason = (uint8_t *)(pData);

    printf("[ATS]BLE disconnect\r\n");
    printf("Ble disconnect reason %d \r\n", *pu8Reason);

#ifdef __BLEWIFI_TRANSPARENT__
    msg_print_uart1("+BLECONN:PEER DISCONNECTION\n");
#endif

    if (false == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_NETWORK))
    {
        BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_WAIT_UPDATE_HOST, false);
        BleWifi_Ble_Stop();
    }

    BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_BLE_CONNECTED , false);
    App_Ctrl_LedStatusChange();

    /* stop the OTA behavior */
    if (gTheOta)
    {
        MwOta_DataGiveUp();
        free(gTheOta);
        gTheOta = NULL;

        App_Ctrl_MsgSend(APP_CTRL_MSG_OTHER_OTA_OFF_FAIL, NULL, 0);
    }
}

static void App_Ctrl_TaskEvtHandler_BleDataInd(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
    BLEWIFI_INFO("BLEWIFI: MSG BLEWIFI_APP_CTRL_MSG_BLE_DATA_IND \r\n");
    BleWifi_Ble_DataRecvHandler(pData, u32Len);
}

static void App_Ctrl_TaskEvtHandler_WifiInitComplete(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
    uint8_t u8AutoCount = 0;
    wifi_scan_config_t stScanConfig ={0};

    BLEWIFI_INFO("BLEWIFI: MSG APP_CTRL_MSG_WIFI_INIT_COMPLETE \r\n");
    BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_WIFI_INIT_DONE , true);

#ifdef __SONOFF__
    if(false == g_u8IsSonoffFactoryDone)
    {
        return ;
    }
#endif
    if(false == g_u8MpScanProcessFinished)     // mp test
    {
        g_u8MpScanTestFlag = true;
        stScanConfig.ssid = MP_WIFI_DEFAULT_SSID;
        stScanConfig.show_hidden = 1;
        stScanConfig.scan_type = WIFI_SCAN_TYPE_MIX;
        g_u8MpScanCnt = 1;
        BleWifi_Wifi_Scan_Req(&stScanConfig);
    }
    else
    {
        BleWifi_Wifi_Query_Status(BLEWFII_WIFI_GET_AUTO_CONN_LIST_NUM , &u8AutoCount);
        //u8AutoCount = BleWifi_Wifi_AutoConnectListNum();
        if ((0 == u8AutoCount) && (0 == g_RoamingApInfoTotalCnt))
        {
            /* When do wifi scan, set wifi auto connect is false */
            printf("u8AutoCount ==0\r\n");
            BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_WIFI_AUTOCONN_LED , false);
        }
        else
        {
            printf("u8AutoCount !=0\r\n");
            BleWifi_Wifi_Start_Conn(NULL);
            BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_WIFI_AUTOCONN_LED , true);
        }
        App_Ctrl_LedStatusChange();
    }
}

static void App_Ctrl_TaskEvtHandler_WifiScanDone(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
    uint8_t i = 0;
    uint8_t u8IsMatched = false;
    wifi_scan_list_t stScanList = {0};
    wifi_scan_config_t stScanConfig = {0};
    wifi_conn_config_t stWifiConnConfig = {0};

    BLEWIFI_INFO("BLEWIFI: MSG APP_CTRL_MSG_WIFI_SCAN_DONE \r\n");
    BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_WIFI_SCANNING , false);

#ifdef __SONOFF__
    if (( true == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_TEST_MODE))
        && (true == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_AT_WIFI_MODE)))
    {
        /* Get APs list */
        wifi_scan_get_ap_list(&stScanList);

        /* Search if AP matched */
        for (i=0; i< stScanList.num; i++)
        {
            if (memcmp(stScanList.ap_record[i].ssid, g_WifiSSID, strlen((char *)g_WifiSSID)) == 0)
            {
                g_WifiRSSI = stScanList.ap_record[i].rssi;
                u8IsMatched = true;
                break;
            }
        }
        if(true == u8IsMatched)
        {
            stWifiConnConfig.ssid_length = strlen((char *)g_WifiSSID);
            stWifiConnConfig.password_length = strlen((char *)g_WifiPassword);
            memcpy(stWifiConnConfig.ssid , g_WifiSSID , stWifiConnConfig.ssid_length);
            memcpy(stWifiConnConfig.password , g_WifiPassword , stWifiConnConfig.password_length);
            stWifiConnConfig.conn_timeout = TIMEOUT_WIFI_CONN_TIME;

            BleWifi_Wifi_Start_Conn(&stWifiConnConfig);
        }
        else
        {
            msg_print_uart1("AT+SIGNAL=0\r\n");
            BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_AT_WIFI_MODE , false);
#ifdef TEST_MODE_DEBUG_ENABLE
            msg_print_uart1("Fail to scan the AP for AT_WIFI\r\n");
#endif
            FinalizedATWIFIcmd();
        }
        return;
    }
#endif

    stScanConfig.ssid = MP_WIFI_DEFAULT_SSID;
    stScanConfig.show_hidden = 1;
    stScanConfig.scan_type = WIFI_SCAN_TYPE_MIX;

    if(g_u8MpScanTestFlag == true)
    {
        /* Get APs list */
        wifi_scan_get_ap_list(&stScanList);

        /* Search if AP matched */
        for (i=0; i< stScanList.num; i++)
        {
            if (memcmp(stScanList.ap_record[i].ssid, stScanConfig.ssid, strlen((char *)stScanConfig.ssid)) == 0)
            {
                u8IsMatched = true;
                break;
            }
        }
        if (true == u8IsMatched)  //find the specified SSID
        {
            g_u8MpIsFindTargetAP = true;
            g_u8MpScanTestFlag = false;
            g_u8MpScanProcessFinished = true;

            printf("MP mode start\r\n");

            if(stScanList.ap_record[i].rssi <= -60)
            {
                printf("NO_ROUTER\r\n");
                g_u8MpBlinkingMode = MP_BLINKING_NO_ROUTER;
            }
            else
            {
                printf("NO_SERVER\r\n");
                g_u8MpBlinkingMode = MP_BLINKING_NO_SERVER;
            }
            App_Ctrl_LedStatusChange();
        }
        else   //can not find the specified SSID, trigger wifi scan again
        {
            g_u8MpIsFindTargetAP = false;

            if(g_u8MpScanCnt < MP_WIFI_SCAN_RETRY_TIMES)
            {
                g_u8MpScanCnt ++;
                BleWifi_Wifi_Scan_Req(&stScanConfig);
            }
            else
            {
                g_u8MpScanCnt = 0;
                g_u8MpScanTestFlag = false;
                g_u8MpScanProcessFinished = true;
                App_Ctrl_MsgSend(APP_CTRL_MSG_WIFI_INIT_COMPLETE, NULL, 0);
            }
        }
    }
    else
    {
        if(true == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_MANUALLY_CONNECT_SCAN)) // do manually connect
        {
            if(g_u8IsManuallyConnectScanRetry == true)
            {
                /* Get APs list */
                wifi_scan_get_ap_list(&stScanList);

                /* Search if AP matched */
                for (i=0; i< stScanList.num; i++)
                {
                    // if find target AP or Hidden AP in the scan list
                    if ((memcmp(stScanList.ap_record[i].ssid, g_stStartConnConfig.ssid , sizeof(g_stStartConnConfig.ssid)) == 0) ||
                        (strlen((char *)stScanList.ap_record[i].ssid) == 0))
                    {
                        u8IsMatched = true;
                        break;
                    }
                }
                if(true == u8IsMatched)
                {
                    stWifiConnConfig.ssid_length = strlen((char *)g_stStartConnConfig.ssid);
                    stWifiConnConfig.password_length = strlen((char *)g_stStartConnConfig.password);
                    memcpy(stWifiConnConfig.ssid , g_stStartConnConfig.ssid , stWifiConnConfig.ssid_length);
                    memcpy(stWifiConnConfig.password , g_stStartConnConfig.password , stWifiConnConfig.password_length);
                    stWifiConnConfig.conn_timeout = g_stStartConnConfig.conn_timeout;

                    BleWifi_Wifi_Start_Conn(&stWifiConnConfig);
                    BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_WIFI_USER_CONNECTING_EXEC , true);
                }
                else
                {
                    BleWifi_Ble_SendResponse(BLEWIFI_RSP_CONNECT, BLEWIFI_WIFI_AP_NOT_FOUND);
                }
                BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_MANUALLY_CONNECT_SCAN , false);
                g_u8IsManuallyConnectScanRetry = false;
            }
            else
            {
                /* Get APs list */
                wifi_scan_get_ap_list(&stScanList);

                /* Search if AP matched */
                for (i=0; i< stScanList.num; i++)
                {
                    if (memcmp(stScanList.ap_record[i].ssid, g_stStartConnConfig.ssid , sizeof(g_stStartConnConfig.ssid)) == 0)
                    {
                        u8IsMatched = true;
                        break;
                    }
                }
                if(true == u8IsMatched)
                {
                    stWifiConnConfig.ssid_length = strlen((char *)g_stStartConnConfig.ssid);
                    stWifiConnConfig.password_length = strlen((char *)g_stStartConnConfig.password);
                    memcpy(stWifiConnConfig.ssid , g_stStartConnConfig.ssid , stWifiConnConfig.ssid_length);
                    memcpy(stWifiConnConfig.password , g_stStartConnConfig.password , stWifiConnConfig.password_length);
                    stWifiConnConfig.conn_timeout = g_stStartConnConfig.conn_timeout;

                    BleWifi_Wifi_Start_Conn(&stWifiConnConfig);
                    BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_WIFI_USER_CONNECTING_EXEC , true);
                    BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_MANUALLY_CONNECT_SCAN , false);
                }
                else
                {
                    memset(&stScanConfig , 0 ,sizeof(stScanConfig));
                    stScanConfig.show_hidden = 1;
                    stScanConfig.scan_type = WIFI_SCAN_TYPE_MIX;
                    g_u8IsManuallyConnectScanRetry = true;

                    BleWifi_Wifi_Scan_Req(&stScanConfig);
                }
            }
        }
        else
        {
            BleWifi_Wifi_SendScanReport();
            BleWifi_Ble_SendResponse(BLEWIFI_RSP_SCAN_END, 0);
        }
    }
}

static void App_Ctrl_TaskEvtHandler_WifiConnection(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
    blewifi_wifi_set_dtim_t stSetDtim = {0};

    printf("BLEWIFI: MSG APP_CTRL_MSG_WIFI_CONNECTION \r\n");
#ifdef __BLEWIFI_TRANSPARENT__
    msg_print_uart1("WIFI CONNECTED\n");
#endif
    BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_WIFI_CONNECTING , false);
    BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_WIFI_CONNECTED , true);
    BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_WIFI_GOT_IP , true);
    BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_WIFI_USER_CONNECTING_EXEC , false);
    g_eLedIOPort = LED_IO_PORT_WIFI_CONNECTED;

#ifdef __SONOFF__
    if (( true == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_TEST_MODE))
        && (true == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_AT_WIFI_MODE)))
    {
        msg_print_uart1("AT+SIGNAL=%d\r\n", g_WifiRSSI);
        BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_AT_WIFI_MODE , false);
#ifdef TEST_MODE_DEBUG_ENABLE
        msg_print_uart1("WifiConnection\r\n");
#endif
        FinalizedATWIFIcmd();
        return ;
    }
#endif

    BleWifi_Ble_SendResponse(BLEWIFI_RSP_CONNECT, BLEWIFI_WIFI_CONNECTED_DONE);


    BleWifi_Wifi_SendStatusInfo(BLEWIFI_IND_IP_STATUS_NOTIFY);

    UpdateBatteryContent();

    BleWifi_Wifi_UpdateBeaconInfo();
    stSetDtim.u32DtimValue = (uint32_t)BleWifi_Wifi_GetDtimSetting();
    stSetDtim.u32DtimEventBit = BW_WIFI_DTIM_EVENT_BIT_DHCP_USE;
    BleWifi_Wifi_Set_Config(BLEWIFI_WIFI_SET_DTIM , (void *)&stSetDtim);

    BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_WIFI_AUTOCONN_LED, false);
    App_Ctrl_LedStatusChange();

//    Sensor_Data_Push(BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_DOOR), TIMER_POST,  BleWifi_COM_SntpGetRawData());
//    Iot_Data_TxTask_MsgSend(IOT_DATA_TX_MSG_CLOUD_POST, NULL, 0);
//    App_Sensor_Post_To_Cloud(TIMER_POST);

    if (false == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_WAIT_UPDATE_HOST))
    {
        if(strncmp(g_tHostInfo.ubaHostInfoURL, HOSTINFO_URL, sizeof(HOSTINFO_URL))!=0)
        {
            Iot_Data_TxTask_MsgSend(IOT_DATA_TX_MSG_CLOUD_CONNECTION, NULL, 0);
        }
        else
        {
            printf("URL is default value, skip it.\n");
        }
    }

    // if support cloud, send message cloud connect to app_ctrl or iot_data
#if(STRESS_TEST_AUTO_CONNECT == 1)
    g_u32StressTestCount++;
    osDelay(APP_CTRL_RESET_DELAY);
    printf("Auto test count = %u\r\n",g_u32StressTestCount);
    BleWifi_Wifi_Stop_Conn();
    BleWifi_Wifi_Start_Conn(NULL);
#endif

}

static void App_Ctrl_TaskEvtHandler_WifiDisconnection(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
    blewifi_wifi_set_dtim_t stSetDtim = {0};
    uint8_t *pu8Reason = (uint8_t*)(pData);

    printf("BLEWIFI: MSG APP_CTRL_MSG_WIFI_DISCONNECTION\r\n");
    printf("reason %d\r\n", *pu8Reason);
    g_eLedIOPort = LED_IO_PORT_WIFI_DISCONNECTED;

#ifdef __BLEWIFI_TRANSPARENT__
    msg_print_uart1("WIFI DISCONNECTION\n");
#endif

    BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_NOT_CNT_SRV, false);
    BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_WIFI_CONNECTING , false);
    BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_WIFI_CONNECTED , false);
    BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_WIFI_GOT_IP , false);

    if(true == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_WAIT_UPDATE_HOST) &&
       true == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_WIFI_USER_CONNECTING_EXEC))
    {
        BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_WAIT_UPDATE_HOST, false);
        BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_WIFI_USER_CONNECTING_EXEC , false);

        if(*pu8Reason == WIFI_REASON_CODE_PREV_AUTH_INVALID ||
           *pu8Reason == WIFI_REASON_CODE_4_WAY_HANDSHAKE_TIMEOUT ||
           *pu8Reason == WIFI_REASON_CODE_GROUP_KEY_UPDATE_TIMEOUT)
        {

            BleWifi_Ble_SendResponse(BLEWIFI_RSP_CONNECT, BLEWIFI_WIFI_PASSWORD_FAIL);
        }
        else if(*pu8Reason == WIFI_REASON_CODE_CONNECT_NOT_FOUND)
        {
            BleWifi_Ble_SendResponse(BLEWIFI_RSP_CONNECT, BLEWIFI_WIFI_AP_NOT_FOUND);
        }
        else
        {
            BleWifi_Ble_SendResponse(BLEWIFI_RSP_CONNECT, BLEWIFI_WIFI_CONNECTED_FAIL);
        }
    }
    else
    {
        BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_WIFI_USER_CONNECTING_EXEC , false);
    }

#ifdef __SONOFF__
    if (( true == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_TEST_MODE))
        && (true == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_AT_WIFI_MODE)))
    {
        msg_print_uart1("AT+SIGNAL=0\r\n");
        BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_AT_WIFI_MODE , false);
#ifdef TEST_MODE_DEBUG_ENABLE
        msg_print_uart1("WifiDisconnection\r\n");
#endif
        FinalizedATWIFIcmd();
        return ;
    }
#endif

    stSetDtim.u32DtimValue = 0;
    stSetDtim.u32DtimEventBit = BW_WIFI_DTIM_EVENT_BIT_DHCP_USE;
    BleWifi_Wifi_Set_Config(BLEWIFI_WIFI_SET_DTIM , (void *)&stSetDtim);
    BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_WIFI_AUTOCONN_LED , true);
    App_Ctrl_LedStatusChange();
}

static void App_Ctrl_TaskEvtHandler_WifiStopComplete(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
    wifi_scan_config_t stScanConfig ={0};

    printf("Wifi StopComplete\r\n");
    if(true == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_WAIT_UPDATE_HOST))
    {
        if(true == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_MANUALLY_CONNECT_SCAN)) //manually connect need scan first
        {
            stScanConfig.ssid = g_stStartConnConfig.ssid;
            stScanConfig.show_hidden = 1;
            stScanConfig.scan_type = WIFI_SCAN_TYPE_MIX;

            g_u8IsManuallyConnectScanRetry = false;

            BleWifi_Wifi_Scan_Req(&stScanConfig);
        }
        else
        {
            BleWifi_Wifi_Start_Conn(&g_stStartConnConfig);
            BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_WIFI_USER_CONNECTING_EXEC , true);
        }
    }
}

static void App_Ctrl_TaskEvtHandler_OtherOtaOn(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
    printf("BLEWIFI: MSG APP_CTRL_MSG_OTHER_OTA_ON \r\n");
    BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_OTA , true);
    App_Ctrl_LedStatusChange();

    // Start OTA timeout timer
    osTimerStop(g_ota_timeout_timer);
    osTimerStart(g_ota_timeout_timer, OTA_TOTAL_TIMEOUT);
}

static void App_Ctrl_TaskEvtHandler_OtherOtaOff(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
    printf("BLEWIFI: MSG APP_CTRL_MSG_OTHER_OTA_OFF \r\n");
    BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_OTA , false);
    msg_print_uart1("OK\r\n");

    App_Ctrl_LedStatusChange();

    // Stop OTA timeout timer
    osTimerStop(g_ota_timeout_timer);

    // restart the system
    osDelay(APP_CTRL_RESET_DELAY);
    Hal_Sys_SwResetAll();
}

static void App_Ctrl_TaskEvtHandler_OtherOtaOffFail(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
    printf("BLEWIFI: MSG APP_CTRL_MSG_OTHER_OTA_OFF_FAIL \r\n");
    BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_OTA , false);
    msg_print_uart1("ERROR\r\n");

    App_Ctrl_LedStatusChange();

    // Stop OTA timeout timer
    osTimerStop(g_ota_timeout_timer);
}

static void App_Ctrl_TaskEvtHandler_OtherSysTimer(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
    BLEWIFI_INFO("BLEWIFI: MSG APP_CTRL_MSG_OTHER_SYS_TIMER \r\n");
    App_Ctrl_SysStatusChange();
}

void App_Ctrl_NetworkingStart(uint32_t u32ExpireTime)
{
    g_u8ShortPressButtonProcessed = 0;
    if (false == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_NETWORK))
    {
        printf("[%s %d] start\n", __func__, __LINE__);

        BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_NETWORK , true);
        //Let LED change to network mode
        BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_BUTTON_PRESS , false);

        /* When button press long than 5 second, then change ble time from ps mode to work mode */
        App_Ctrl_LedStatusChange();

        BleWifi_Ble_Start(BLEWIFI_BLE_ADVERTISEMENT_INTERVAL_MIN); //100 ms

        osTimerStop(g_tAppCtrlNetworkTimerId);
        if ( u32ExpireTime > 0 )
        {
            /* BLE Adv */
            osTimerStart(g_tAppCtrlNetworkTimerId, u32ExpireTime);
        }
    }
    else
    {
        BLEWIFI_WARN("[%s %d] BLEWIFI_APP_CTRL_EVENT_BIT_NETWORK already true\n", __func__, __LINE__);
    }
}

void App_Ctrl_NetworkingStop(void)
{
    if (true == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_NETWORK))
    {
        printf("[%s %d] start\n", __func__, __LINE__);

        osTimerStop(g_tAppCtrlNetworkTimerId);
        BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_NETWORK , false);

        if (false == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_BLE_CONNECTED))
        {
            BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_WAIT_UPDATE_HOST, false);
            BleWifi_Ble_Stop();
        }
    }
    else
    {
        BLEWIFI_WARN("[%s %d] BLEWIFI_APP_CTRL_EVENT_BIT_NETWORK already false\n", __func__, __LINE__);
    }
    App_Ctrl_LedStatusChange();
}

void App_Ctrl_NetworkingTimeout(void const *argu)
{
    App_Ctrl_MsgSend(APP_CTRL_MSG_NETWORKING_STOP, NULL, 0);
}

static void App_Ctrl_TaskEvtHandler_NetworkingStart(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
    uint32_t *pu32ExpireTime;

    if((false == g_u8MpScanProcessFinished) || (true == g_u8MpIsFindTargetAP)) //ignore normal process
    {
    }
    else
    {
        pu32ExpireTime = (uint32_t *)pData;
        App_Ctrl_NetworkingStart(*pu32ExpireTime);
    }
}

static void App_Ctrl_TaskEvtHandler_NetworkingStop(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
    if((false == g_u8MpScanProcessFinished) || (true == g_u8MpIsFindTargetAP)) //ignore normal process
    {
    }
    else
    {
        App_Ctrl_NetworkingStop();
    }
}

#if (APP_CTRL_BUTTON_SENSOR_EN == 1)
static void App_Ctrl_TaskEvtHandler_ButtonStateChange(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
    BLEWIFI_INFO("BLEWIFI: MSG APP_CTRL_MSG_BUTTON_STATE_CHANGE \r\n");

    /* Start timer to debounce */
    osTimerStop(g_tAppButtonDebounceTimerId);
    osTimerStart(g_tAppButtonDebounceTimerId, APP_CTRL_BUTTON_TIMEOUT_DEBOUNCE_TIME);
}

static void App_Ctrl_TaskEvtHandler_ButtonDebounceTimeOut(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
    uint32_t u32PinLevel = 0;

    BLEWIFI_INFO("BLEWIFI: MSG BLEWIFI_CTRL_MSG_BUTTON_DEBOUNCE_TIMEOUT \r\n");

    // Get the status of GPIO (Low / High)
    u32PinLevel = Hal_Vic_GpioInput(APP_CTRL_BUTTON_IO_PORT);
    BLEWIFI_INFO("BLEWIFI_CTRL_BUTTON_IO_PORT pin level = %s\r\n", u32PinLevel ? "GPIO_LEVEL_HIGH" : "GPIO_LEVEL_LOW");

    if(GPIO_LEVEL_LOW == u32PinLevel)
    {
        /* Disable - Invert gpio interrupt signal */
        Hal_Vic_GpioIntInv(APP_CTRL_BUTTON_IO_PORT, 0);
        // Enable Interrupt
        Hal_Vic_GpioIntEn(APP_CTRL_BUTTON_IO_PORT, 1);
    }
    else
    {
        /* Enable - Invert gpio interrupt signal */
        Hal_Vic_GpioIntInv(APP_CTRL_BUTTON_IO_PORT, 1);
        // Enable Interrupt
        Hal_Vic_GpioIntEn(APP_CTRL_BUTTON_IO_PORT, 1);
    }

    if(APP_CTRL_BUTTON_PRESS_LEVEL == u32PinLevel)
    {
        BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_BUTTON_PRESS , true);
        App_Ctrl_LedStatusChange();

        g_u8ShortPressButtonProcessed = 1;

        App_ButtonFsm_Run(APP_BUTTON_EVENT_PRESS);
    }
    else
    {
        BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_BUTTON_PRESS , false);
        App_Ctrl_LedStatusChange();

        Hal_Vic_GpioOutput(g_eLedIOPort, GPIO_LEVEL_LOW);
        g_u8ShortPressButtonProcessed = 0;

        App_ButtonFsm_Run(APP_BUTTON_EVENT_RELEASE);
    }
}
static void App_Ctrl_TaskEvtHandler_DoorStateChange(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
    BLEWIFI_INFO("BLEWIFI: MSG BLEWIFI_CTRL_MSG_DOOR_STATECHANGE \r\n");
    /* Start timer to debounce */
    osTimerStop(g_tAppDoorTimerId);
    osTimerStart(g_tAppDoorTimerId, DOOR_DEBOUNCE_TIMEOUT);
}

static void App_Ctrl_TaskEvtHandler_DoorDebounceTimeOut(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
    BLEWIFI_INFO("BLEWIFI: MSG BLEWIFI_CTRL_MSG_DOOR_DEBOUNCETIMEOUT \r\n");
    unsigned int u32PinLevel = 0;

    // Get the status of GPIO (Low / High)
    u32PinLevel = Hal_Vic_GpioInput(MAGNETIC_IO_PORT);
    printf("MAG_IO_PORT pin level = %s\r\n", u32PinLevel ? "GPIO_LEVEL_HIGH:OPEN" : "GPIO_LEVEL_LOW:CLOSE");

    // When detect falling edge, then modify to raising edge.

    // Voltage Low   / DoorStatusSet - True  / Door Status - CLose - switch on  - type = 2
    // Voltage Hight / DoorStatusSet - False / Door Status - Open  - switch off - type = 3

    if(GPIO_LEVEL_LOW == u32PinLevel)
    {
        /* Disable - Invert gpio interrupt signal */
        Hal_Vic_GpioIntInv(MAGNETIC_IO_PORT, 0);
        // Enable Interrupt
        Hal_Vic_GpioIntEn(MAGNETIC_IO_PORT, 1);

        if (BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_DOOR) == false)
        {
            BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_DOOR, true); // true is Door Close

#ifdef __SONOFF__
            App_Ctrl_LedStatusChange();
#else
            g_ubAppCtrlLedStatus = APP_CTRL_LED_DOOR_SWITCH;
            if(true == g_u8MpIsFindTargetAP)
            {
                g_eLedIOPort = LED_IO_PORT_WIFI_CONNECTED;
            }
            App_Ctrl_DoLedDisplay();
#endif

            osTimerStop(g_cloud_DailyBatteryPost_timeout_timer);
            osTimerStart(g_cloud_DailyBatteryPost_timeout_timer, IDLE_POST_TIME);

            if((g_tHoldReminder.u8Enable == true) && (g_tSystemDefenseEn.u8En == DOOR_DEFENSE_ON))
            {
                if(g_tHoldReminder.u8Switch == DOOR_WARING_FLAG_CLOSE)
                {
                    osTimerStop(g_cloud_DoorStatusWarning_timeout_timer);
                    osTimerStart(g_cloud_DoorStatusWarning_timeout_timer, g_tHoldReminder.u16Time * CONVERT_MINUTE_TO_MILLISECOND);
                }
            }

            /* Send to IOT task to post data */
//          Sensor_Data_Push(BleWifi_Ctrl_EventStatusGet(BLEWIFI_CTRL_EVENT_BIT_DOOR), BleWifi_Ctrl_EventStatusGet(BLEWIFI_CTRL_EVENT_BIT_DOOR) ? DOOR_OPEN:DOOR_CLOSE,  BleWifi_SntpGetRawData());
//          Iot_Data_TxTask_MsgSend(IOT_DATA_TX_MSG_CLOUD_POST, NULL, 0);
            App_Sensor_Post_To_Cloud(BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_DOOR) ? DOOR_OPEN:DOOR_CLOSE);
            if((true == g_u8MpScanProcessFinished)
                && (false == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_WIFI_GOT_IP)
                && (false == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_NETWORK))))
            {
                printf("---->AutoConn...DoorClose\n");
                // the idle of the connection retry
                BleWifi_Wifi_Start_Conn(NULL);
            }
        }
    }
    else
    {
        /* Enable - Invert gpio interrupt signal */
        Hal_Vic_GpioIntInv(MAGNETIC_IO_PORT, 1);
        // Enable Interrupt
        Hal_Vic_GpioIntEn(MAGNETIC_IO_PORT, 1);

        if (BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_DOOR) == true)
        {
            BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup,APP_CTRL_EVENT_BIT_DOOR, false); // false is Door Open

#ifdef __SONOFF__
            Hal_Vic_GpioOutput(g_eLedIOPort, GPIO_LEVEL_LOW); //disable LED immediately
            App_Ctrl_LedStatusChange();
#else
            g_ubAppCtrlLedStatus = APP_CTRL_LED_DOOR_SWITCH;
            if(true == g_u8MpIsFindTargetAP)
            {
                g_eLedIOPort = LED_IO_PORT_WIFI_CONNECTED;
            }
            App_Ctrl_DoLedDisplay();
#endif

            if((g_tHoldReminder.u8Enable == true) && (g_tSystemDefenseEn.u8En == DOOR_DEFENSE_ON))
            {
                if(g_tHoldReminder.u8Switch == DOOR_WARING_FLAG_OPEN)
                {
                    osTimerStop(g_cloud_DoorStatusWarning_timeout_timer);
                    osTimerStart(g_cloud_DoorStatusWarning_timeout_timer, g_tHoldReminder.u16Time * CONVERT_MINUTE_TO_MILLISECOND);
                }
            }

            /* Send to IOT task to post data */
//          Sensor_Data_Push(BleWifi_Ctrl_EventStatusGet(BLEWIFI_CTRL_EVENT_BIT_DOOR), BleWifi_Ctrl_EventStatusGet(BLEWIFI_CTRL_EVENT_BIT_DOOR) ? DOOR_OPEN:DOOR_CLOSE,  BleWifi_SntpGetRawData());
//          Iot_Data_TxTask_MsgSend(IOT_DATA_TX_MSG_CLOUD_POST, NULL, 0);
            App_Sensor_Post_To_Cloud(BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_DOOR) ? DOOR_OPEN:DOOR_CLOSE);
            if((true == g_u8MpScanProcessFinished)
                && (false == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_WIFI_GOT_IP)
                && (false == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_NETWORK))))
            {
                printf("---->AutoConn...DoorOpen\n");
                // the idle of the connection retry
                BleWifi_Wifi_Start_Conn(NULL);
            }
        }
    }
}

static void App_Ctrl_TaskEvtHandler_DoorStatusWarningTimeOut(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
    printf("g_tHoldReminder.u8Switch = %u\r\n",g_tHoldReminder.u8Switch);

    // Trigger to send http data
    if((g_tHoldReminder.u8Enable == false) || (g_tSystemDefenseEn.u8En == DOOR_DEFENSE_OFF))
    {
        osTimerStop(g_cloud_DoorStatusWarning_timeout_timer);
    }
    else
    {
        if(BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_DOOR) == true) //door close
        {
            if(g_tHoldReminder.u8Switch == DOOR_WARING_FLAG_CLOSE)
            {
                App_Sensor_Post_To_Cloud(DOOR_HOLD_REMINDER_WARING);

                osTimerStop(g_cloud_DoorStatusWarning_timeout_timer);
                osTimerStart(g_cloud_DoorStatusWarning_timeout_timer, g_tHoldReminder.u16Time * CONVERT_MINUTE_TO_MILLISECOND);
            }
        }
        else // door open
        {
            if(g_tHoldReminder.u8Switch == DOOR_WARING_FLAG_OPEN)
            {
                App_Sensor_Post_To_Cloud(DOOR_HOLD_REMINDER_WARING);

                osTimerStop(g_cloud_DoorStatusWarning_timeout_timer);
                osTimerStart(g_cloud_DoorStatusWarning_timeout_timer, g_tHoldReminder.u16Time * CONVERT_MINUTE_TO_MILLISECOND);
            }
        }
    }

    if((true == g_u8MpScanProcessFinished)
        && (false == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_WIFI_GOT_IP)
        && (false == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_NETWORK))))
    {
        printf("---->AutoConn...DoorReminder\n");
        // the idle of the connection retry
        BleWifi_Wifi_Start_Conn(NULL);
    }
}

static void App_Ctrl_TaskEvtHandler_DoorDamageChange(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
    BLEWIFI_INFO("BLEWIFI: MSG BLEWIFI_CTRL_MSG_DOOR_DAMAGE_CHANGE \r\n");
    /* Start timer to debounce */
    osTimerStop(g_tAppDoorDamageTimerId);
    osTimerStart(g_tAppDoorDamageTimerId, DOOR_DEBOUNCE_TIMEOUT);
}

static void App_Ctrl_TaskEvtHandler_DoorDamageTimeOut(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
    unsigned int u32PinLevel = 0;

    // Get the status of GPIO (Low / High)
    u32PinLevel = Hal_Vic_GpioInput(DOOR_SENSOR_DAMAGE_IO_PORT);

    if(GPIO_LEVEL_LOW == u32PinLevel)
    {
        /* Disable - Invert gpio interrupt signal */
        Hal_Vic_GpioIntInv(DOOR_SENSOR_DAMAGE_IO_PORT, 0);
        // Enable Interrupt
        Hal_Vic_GpioIntEn(DOOR_SENSOR_DAMAGE_IO_PORT, 1);
    }
    else
    {
        printf("DoorDamageTim\r\n");

        /* Disable - Invert gpio interrupt signal */
        Hal_Vic_GpioIntInv(DOOR_SENSOR_DAMAGE_IO_PORT, 1);
        // Enable Interrupt
        Hal_Vic_GpioIntEn(DOOR_SENSOR_DAMAGE_IO_PORT, 1);

        g_ubAppCtrlLedStatus = APP_CTRL_LED_DAMAGE;
        if(true == g_u8MpIsFindTargetAP)
        {
            g_eLedIOPort = LED_IO_PORT_WIFI_CONNECTED;
        }
        App_Ctrl_DoLedDisplay();

        // Trigger to send http data
        App_Sensor_Post_To_Cloud(DOOR_SENSOR_DAMAGE);
        if((true == g_u8MpScanProcessFinished)
            && (false == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_WIFI_GOT_IP)
            && (false == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_NETWORK))))
        {
            printf("---->AutoConn...DoorDamage\n");
            // the idle of the connection retry
            BleWifi_Wifi_Start_Conn(NULL);
        }
    }
}

static void App_Ctrl_TaskEvtHandler_ButtonReleaseTimeOut(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
    // used in short/long press
    App_ButtonFsm_Run(APP_BUTTON_EVENT_TIMEOUT);
}

void App_Ctrl_ButtonReleaseHandle(uint8_t u8ReleaseCount)
{
    // if the state is not at ble connection and network (ble adv), then do post data
    if(false == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_BLE_CONNECTED) && false == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_NETWORK))
    {
        if(u8ReleaseCount == 1)
        {
            printf("[%s %d] release once\n", __func__, __LINE__);

//            Sensor_Data_Push(BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_DOOR), SHORT_TRIG,  BleWifi_COM_SntpGetRawData());
//            Iot_Data_TxTask_MsgSend(IOT_DATA_TX_MSG_CLOUD_POST, NULL, 0);
            App_Sensor_Post_To_Cloud(SHORT_TRIG);
            if((true == g_u8MpScanProcessFinished)
                && (false == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_WIFI_GOT_IP)
                && (false == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_NETWORK))))
            {
                printf("---->AutoConn...Button\n");
                // the idle of the connection retry
                BleWifi_Wifi_Start_Conn(NULL);
            }

        }
        else if(u8ReleaseCount == 2)
        {
            printf("[%s %d] release twice\n", __func__, __LINE__);

//            Sensor_Data_Push(BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_DOOR), DOUBLE_SHORT_TRIG,  BleWifi_COM_SntpGetRawData());
            Iot_Data_TxTask_MsgSend(IOT_DATA_TX_MSG_CLOUD_POST, NULL, 0);
        }
        else if(u8ReleaseCount >= 3)
        {
            printf("[%s %d] release %u times\n", __func__, __LINE__, u8ReleaseCount);
        }
    }
    else
    {
        App_Ctrl_NetworkingStop();
    }
}
#endif
#if (APP_CTRL_WAKEUP_IO_EN == 1)
static void App_Ctrl_TaskEvtHandler_PsSmartStateChange(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
    BLEWIFI_INFO("APP: MSG APP_CTRL_MSG_PS_SMART_STATE_CHANGE \r\n");

    /* Start timer to debounce */
    osTimerStop(g_tAppPsSmartDebounceTimerId);
    osTimerStart(g_tAppPsSmartDebounceTimerId, APP_CTRL_WAKEUP_IO_TIMEOUT_DEBOUNCE_TIME);

}
static void App_Ctrl_TaskEvtHandler_PsSmartDebounceTimeOut(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
    uint32_t u32PinLevel = 0;
    BLEWIFI_INFO("APP: MSG APP_CTRL_MSG_PS_PMART_DEBOUNCE_TIMEOUT \r\n");

    u32PinLevel = Hal_Vic_GpioInput(APP_CTRL_WAKEUP_IO_PORT);
    if(APP_CTRL_WAKEUP_IO_LEVEL == u32PinLevel)
    {
        BLEWIFI_INFO("Ps_Smart_Off_callback, Wakeup !!!!!!\r\n");
        ps_smart_sleep(0);
    }
    else
    {
        /* Power saving settings */
        BLEWIFI_INFO("Ps_Smart_Off_callback, Sleep !!!!!!\r\n");
        ps_smart_sleep(BLEWIFI_COM_POWER_SAVE_EN);
    }
    App_Ps_Smart_Pin_Config(APP_CTRL_WAKEUP_IO_PORT, u32PinLevel);
}
#endif

#ifdef __BLEWIFI_TRANSPARENT__
int App_Ctrl_BleCastWithExpire(uint8_t u8BleCastEnable, uint32_t u32ExpireTime)
{
    if ((u8BleCastEnable != 0) && (u8BleCastEnable != 1))
    {
        return -1;
    }

    if (((0 < u32ExpireTime) && (u32ExpireTime < 1000)) || (u32ExpireTime > 3600000))
    {
        return -1;
    }

    if (u8BleCastEnable == 1)
    {
        App_Ctrl_MsgSend(APP_CTRL_MSG_NETWORKING_START, (void *)&u32ExpireTime, sizeof(u32ExpireTime));
    }
    else
    {
        App_Ctrl_MsgSend(APP_CTRL_MSG_NETWORKING_STOP, NULL, 0);
    }

    return 0;
}

int App_Ctrl_BleCastParamSet(uint32_t u32CastInteval)
{
    // Range: 0x0020 to 0x4000
    // Time Range: 20 ms to 10.24 sec

    if ((u32CastInteval < 20) || (u32CastInteval > 10240))
    {
        return -1;
    }

    BleWifi_Ble_AdvertisingIntervalChange((uint16_t)u32CastInteval);

    return 0;
}
#endif

void App_Ctrl_TaskEvtHandler(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
    uint32_t i = 0;

    while (g_tAppCtrlEvtHandlerTbl[i].u32EventId != 0xFFFFFFFF)
    {
        // match
        if (g_tAppCtrlEvtHandlerTbl[i].u32EventId == u32EvtType)
        {
            g_tAppCtrlEvtHandlerTbl[i].fpFunc(u32EvtType, pData, u32Len);
            break;
        }

        i++;
    }

    // not match
    if (g_tAppCtrlEvtHandlerTbl[i].u32EventId == 0xFFFFFFFF)
    {
    }
}

void App_Ctrl_Task(void *args)
{
    osEvent rxEvent;
    xAppCtrlMessage_t *rxMsg;
#ifdef __SONOFF__
    uint8_t i;
    osTimerDef_t tTimerTestModeDef;

    // create the timer
    tTimerTestModeDef.ptimer = App_Ctrl_TestMode_Timeout;
    g_tAppCtrlTestModeId = osTimerCreate(&tTimerTestModeDef, osTimerOnce, NULL);
    if (g_tAppCtrlTestModeId == NULL)
    {
        printf("To create the timer for AppTimer is fail.\n");
    }

    osDelay(100);
    /* print "factory?\r\n" */
    for (i = 0; i < SONOFF_FACTORY_PRINT_TIMES ; i++)
    {
        msg_print_uart1("factory?\r\n");
        osDelay(20);
    }

    if ( strcmp(g_tHttpPostContent.ubaDeviceId, DEVICE_ID) == 0 || strcmp(g_tHttpPostContent.ubaChipId, CHIP_ID) == 0 )
    {// if no write data, enter to factory mode.
        g_s8IsTestMode = BLEWIFI_CTRL_IS_TEST_MODE_YES;

        // stop the sys timer
        osTimerStop(g_tAppCtrlSysTimer);

        // start test mode of LED blink
        BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_TEST_MODE , true);
        App_Ctrl_LedStatusChange();

        msg_print_uart1("factory mode\r\n");
    }
    else
    {
        // start the test mode timer
        osTimerStop(g_tAppCtrlTestModeId);
        osTimerStart(g_tAppCtrlTestModeId, TEST_MODE_TIMER_DEF);
    }
#endif

    for(;;)
    {
        /* Wait event */
        rxEvent = osMessageGet(g_tAppCtrlQueueId, osWaitForever);
        if (rxEvent.status != osEventMessage)
            continue;

        rxMsg = (xAppCtrlMessage_t *)rxEvent.value.p;
        App_Ctrl_TaskEvtHandler(rxMsg->u32Event, rxMsg->u8aMessage, rxMsg->u32Length);

        /* Release buffer */
        if (rxMsg != NULL)
            free(rxMsg);
    }
}

int App_Ctrl_MsgSend(uint32_t u32MsgType, uint8_t *pu8Data, uint32_t u32DataLen)
{
    xAppCtrlMessage_t *pMsg = NULL;

	if (NULL == g_tAppCtrlQueueId)
	{
        BLEWIFI_ERROR("APP: ctrl task No queue \r\n");
        goto error;
	}

    /* Mem allocate */
    pMsg = malloc(sizeof(xAppCtrlMessage_t) + u32DataLen);
    if (pMsg == NULL)
	{
        BLEWIFI_ERROR("APP: ctrl task pmsg allocate fail \r\n");
	    goto error;
    }

    pMsg->u32Event = u32MsgType;
    pMsg->u32Length = u32DataLen;
    if (u32DataLen > 0)
    {
        memcpy(pMsg->u8aMessage, pu8Data, u32DataLen);
    }

    if (osMessagePut(g_tAppCtrlQueueId, (uint32_t)pMsg, 0) != osOK)
    {
        printf("APP: ctrl task message send fail \r\n");
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

void App_Ctrl_Init(void)
{
    osThreadDef_t task_def;
    osMessageQDef_t queue_def;

    osTimerDef_t timer_sys_def;
    osTimerDef_t timer_network_def;

    osTimerDef_t timer_led_def;
    osTimerDef_t timer_post_fail_led_timeout_def;
    osTimerDef_t timer_ota_timeout_def;
    osTimerDef_t timer_ble_disconnect_delay_timeout_def;
    osTimerDef_t timer_door_status_warning_timeout_def;
    osTimerDef_t timer_daily_battery_post_timeout_def;
    osTimerDef_t timer_network_stop_delay_timeout_def;

    memset(&g_tHostInfo ,0, sizeof(T_MwFim_GP12_HttpHostInfo));
    memset(&g_tHttpPostContent ,0, sizeof(T_MwFim_GP12_HttpPostContent));
    memset(&g_tHoldReminder ,0, sizeof(T_MwFim_GP17_Hold_Reminder));
    memset(&g_tSystemDefenseEn ,0, sizeof(T_MwFim_GP17_System_Defense_En));

    if (MW_FIM_OK != MwFim_FileRead(MW_FIM_IDX_GP12_PROJECT_DEVICE_AUTH_CONTENT, 0, MW_FIM_GP12_HTTP_POST_CONTENT_SIZE, (uint8_t *)&g_tHttpPostContent))
    {
        memcpy(&g_tHttpPostContent, &g_tMwFimDefaultGp12HttpPostContent, MW_FIM_GP12_HTTP_POST_CONTENT_SIZE);
    }

    if (MW_FIM_OK != MwFim_FileRead(MW_FIM_IDX_GP12_PROJECT_HOST_INFO, 0, MW_FIM_GP12_HTTP_HOST_INFO_SIZE, (uint8_t *)&g_tHostInfo) ) {
        memcpy(&g_tHostInfo, &g_tMwFimDefaultGp12HttpHostInfo, MW_FIM_GP12_HTTP_HOST_INFO_SIZE);
    }

    if (MW_FIM_OK != MwFim_FileRead(MW_FIM_IDX_GP17_PROJECT_HOLD_REMINDER, 0, MW_FIM_GP17_HOLD_REMINDER_SIZE, (uint8_t *)&g_tHoldReminder) )
    {
        memcpy(&g_tHoldReminder, &g_tMwFimDefaultGp17HoldReminder, MW_FIM_GP17_HOLD_REMINDER_SIZE);
    }

    if (MW_FIM_OK != MwFim_FileRead(MW_FIM_IDX_GP17_PROJECT_SYSTEM_DEFENSE_EN, 0, MW_FIM_GP17_SYSTEM_DEFENSE_EN_SIZE, (uint8_t *)&g_tSystemDefenseEn) )
    {
        memcpy(&g_tSystemDefenseEn, &g_tMwFimDefaultGp17SystemDefenseEn, MW_FIM_GP17_SYSTEM_DEFENSE_EN_SIZE);
    }

    /* Create message queue*/
    queue_def.item_sz = sizeof(xAppCtrlMessage_t);
    queue_def.queue_sz = APP_CTRL_QUEUE_SIZE;
    g_tAppCtrlQueueId = osMessageCreate(&queue_def, NULL);
    if (g_tAppCtrlQueueId == NULL)
    {
        BLEWIFI_ERROR("APP: ctrl task create queue fail \r\n");
    }

    /* create timer to trig the sys state */
    timer_sys_def.ptimer = App_Ctrl_SysTimeout;
    g_tAppCtrlSysTimer = osTimerCreate(&timer_sys_def, osTimerOnce, NULL);
    if (g_tAppCtrlSysTimer == NULL)
    {
        BLEWIFI_ERROR("APP: ctrl task create SYS timer fail \r\n");
    }

    /* create timer to trig the ble adv timeout */
    timer_network_def.ptimer = App_Ctrl_NetworkingTimeout;
    g_tAppCtrlNetworkTimerId = osTimerCreate(&timer_network_def, osTimerOnce, NULL);
    if (g_tAppCtrlNetworkTimerId == NULL)
    {
        BLEWIFI_ERROR("APP: ctrl task create Network timeout timer fail \r\n");
    }

    /* create timer to trig led */
    timer_led_def.ptimer = App_Ctrl_LedTimeout;
    g_tAppCtrlLedTimer = osTimerCreate(&timer_led_def, osTimerOnce, NULL);
    if(g_tAppCtrlLedTimer == NULL)
    {
        BLEWIFI_ERROR("BLEWIFI: ctrl task create LED timer fail \r\n");
    }

    /* create post fail led timeout timer */
    timer_post_fail_led_timeout_def.ptimer = App_PostFailLedTimeOutCallBack;
    g_tAppPostFailLedTimeOutTimerId = osTimerCreate(&timer_post_fail_led_timeout_def, osTimerOnce, NULL);
    if (g_tAppPostFailLedTimeOutTimerId == NULL)
    {
        BLEWIFI_ERROR("BLEWIFI: create post fail led timeout timer fail \r\n");
    }

    /* Create the event group */
    if (false == BleWifi_COM_EventCreate(&g_tAppCtrlEventGroup))
    {
        BLEWIFI_ERROR("APP: create event group fail \r\n");
    }

    /* create ota timeout timer */
    timer_ota_timeout_def.ptimer = App_Ota_TimeOutCallBack;
    g_ota_timeout_timer = osTimerCreate(&timer_ota_timeout_def, osTimerOnce, NULL);
    if (g_ota_timeout_timer == NULL)
    {
        BLEWIFI_ERROR("BLEWIFI: create OTA timeout timer fail \r\n");
    }

    /* create ble disconnect delay timeout timer */
    timer_ble_disconnect_delay_timeout_def.ptimer = App_Ble_Disconnect_Delay_TimeOutCallBack;
    g_ble_disconnect_delay_timeout_timer = osTimerCreate(&timer_ble_disconnect_delay_timeout_def, osTimerOnce, NULL);
    if (g_ble_disconnect_delay_timeout_timer == NULL)
    {
        BLEWIFI_ERROR("BLEWIFI: create ble disconnect delay timeout timer fail \r\n");
    }

    /* create door status warning timer */
    timer_door_status_warning_timeout_def.ptimer = App_Door_Status_Warning_TimeOutCallBack;
    g_cloud_DoorStatusWarning_timeout_timer = osTimerCreate(&timer_door_status_warning_timeout_def, osTimerOnce, NULL);
    if (g_cloud_DoorStatusWarning_timeout_timer == NULL)
    {
        BLEWIFI_ERROR("BLEWIFI: create door status warning timeout timer fail \r\n");
    }

    /* create daily battery post timer */
    timer_daily_battery_post_timeout_def.ptimer = App_Daily_Battery_Post_TimeOutCallBack;
    g_cloud_DailyBatteryPost_timeout_timer = osTimerCreate(&timer_daily_battery_post_timeout_def, osTimerOnce, NULL);
    if (g_cloud_DailyBatteryPost_timeout_timer == NULL)
    {
        BLEWIFI_ERROR("BLEWIFI: create door daily post timeout timer fail \r\n");
    }

    /* create network stop delay timer */
    timer_network_stop_delay_timeout_def.ptimer = App_Network_Stop_Delay_TimeOutCallBack;
    g_tAppCtrl_network_stop_delay_timeout_timer = osTimerCreate(&timer_network_stop_delay_timeout_def, osTimerOnce, NULL);
    if (g_tAppCtrl_network_stop_delay_timeout_timer == NULL)
    {
        BLEWIFI_ERROR("BLEWIFI: create network stop delay timeout timer fail \r\n");
    }

    /* the init stat of LED is none off */
    g_ubAppCtrlLedStatus = APP_CTRL_LED_ALWAYS_OFF;
    App_Ctrl_DoLedDisplay();


    /* Create ble-wifi task */
    task_def.name = "app_ctrl";
    task_def.stacksize = APP_CTRL_TASK_STACK_SIZE;
    task_def.tpriority = APP_CTRL_TASK_PRIORITY;
    task_def.pthread = App_Ctrl_Task;
    g_tAppCtrlTaskId = osThreadCreate(&task_def, NULL);
    if (g_tAppCtrlTaskId == NULL)
    {
        printf("APP: ctrl task create fail \r\n");
    }
    else
    {
        printf("APP: ctrl task create successful \r\n");
    }


    /* the init state of system mode is init */
    g_u8AppCtrlSysMode = MW_FIM_SYS_MODE_INIT;

    /* the init state of SYS is init */
    g_u8AppCtrlSysStatus = APP_CTRL_SYS_INIT;
    // start the sys timer
    osTimerStop(g_tAppCtrlSysTimer);
    osTimerStart(g_tAppCtrlSysTimer, APP_COM_SYS_TIME_INIT);

#if (APP_CTRL_BUTTON_SENSOR_EN == 1)
    App_ButtonPress_Init();
#endif

    /* Init Door Action */
    Sensor_DoorPress_Init();

    /* Init Auxadc for battery */
    Sensor_Auxadc_Init();

    g_u8ShortPressButtonProcessed = 0; //It means the time of button pressed is less than 5s 20191018EL

    App_Ctrl_MsgSend(APP_CTRL_MSG_DOOR_SET_HOLD_REMIDER, (uint8_t *)&g_tHoldReminder, sizeof(T_MwFim_GP17_Hold_Reminder));
#ifdef __SONOFF__
#else
    App_Ctrl_MsgSend(APP_CTRL_MSG_DOOR_DAMAGE_TIMEOUT, NULL , 0);
#endif
}
