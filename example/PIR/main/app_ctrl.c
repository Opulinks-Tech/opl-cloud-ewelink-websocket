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
#include "sensor.h"
//#include "sensor_https.h"
//#include "sensor_data.h"
#include "sensor_common.h"
#include "sensor_battery.h"
#include "iot_data.h"
#include "iot_rb_data.h"
#include "wifi_api.h"
#include "iot_data.h"
#include "iot_handle.h"

#define APP_CTRL_RESET_DELAY    (3000)  // ms

osThreadId   g_tAppCtrlTaskId;
osMessageQId g_tAppCtrlQueueId;

osTimerId    g_tAppCtrlSysTimer;
osTimerId    g_tAppCtrlNetworkTimerId;
osTimerId    g_tAppCtrlLedTimer;
osTimerId    g_tAppPostFailLedTimeOutTimerId;
osTimerId    g_ota_timeout_timer = NULL;
osTimerId    g_ble_disconnect_delay_timeout_timer = NULL;
osTimerId    g_cloud_disconnect_timeout_timer = NULL;
osTimerId    g_cloud_DailyBatteryPost_timeout_timer = NULL;
osTimerId    g_tAppCtrl_network_stop_delay_timeout_timer = NULL;

EventGroupHandle_t g_tAppCtrlEventGroup;

uint8_t g_u8AppCtrlSysMode;
uint8_t g_u8AppCtrlSysStatus;
uint8_t g_ubAppCtrlLedStatus;
uint8_t g_u8ButtonProcessed;
uint8_t g_u8ShortPressButtonProcessed; //It means the time of button pressed is less than 5s 20191018EL
uint8_t g_nLastPostDatatType = TIMER_POST;

extern uint8_t g_WifiSSID[WIFI_MAX_LENGTH_OF_SSID];
extern uint8_t g_WifiPassword[64];
extern uint8_t g_WifiSSIDLen;
extern uint8_t g_WifiPasswordLen;

int8_t g_WifiRSSI;

T_MwFim_GP12_HttpPostContent g_tHttpPostContent;
T_MwFim_GP12_HttpHostInfo g_tHostInfo;
T_MwFim_GP17_System_Defense_En g_tSystemDefenseEn;

extern blewifi_ota_t *gTheOta;
extern osTimerId g_tAppFactoryWifiConnTimerId;
extern IoT_Ring_buffer_t g_stIotRbData;
extern uint32_t g_RoamingApInfoTotalCnt;

uint32_t g_u32TickStart = 0, g_u32TickEnd = 0;

E_GpioIdx_t g_eLedIOPort = LED_IO_PORT;

uint8_t g_u8MpScanProcessFinished = false; //true : don't need to execute mp scan test , false : need to execute mp scan test
uint8_t g_u8MpScanTestFlag = false; //true : do mp scan  , false : don't do mp scan
uint8_t g_u8MpScanCnt = 0;
uint8_t g_u8MpIsFindTargetAP = false;
uint8_t g_u8MpBlinkingMode = MP_BLINKING_NONE;

#ifdef CUS_AUTO_CONNECT_TABLE
uint32_t g_u32AutoConnIntervalTable[AUTO_CONNECT_INTERVAL_TABLE_SIZE] =
{
    30000,
    30000,
    60000,
    60000,
    900000
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

    LED_PIR_ONLINE_ON_1,
    LED_PIR_ONLINE_OFF_1,
    LED_PIR_OFFLINE_ON_1,
    LED_PIR_OFFLINE_OFF_1,
    LED_PIR_OFFLINE_ON_2,
    LED_PIR_OFFLINE_OFF_2,
    LED_PIR_MP_ON_1,
    LED_PIR_MP_OFF_1,

#if(DAMAGE_EN == 1)
    LED_DAMAGE_ONLINE_ON_1,
    LED_DAMAGE_ONLINE_OFF_1,
    LED_DAMAGE_OFFLINE_ON_1,
    LED_DAMAGE_OFFLINE_OFF_1,
    LED_DAMAGE_OFFLINE_ON_2,
    LED_DAMAGE_OFFLINE_OFF_2,
#endif

    LED_ALWAYS_ON
};


#if(STRESS_TEST_AUTO_CONNECT == 1)
uint32_t g_u32StressTestCount = 0;
#endif

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

static void App_Ctrl_TaskEvtHandler_PIRStateChange(uint32_t u32EvtType, void *pData, uint32_t u32Len);
static void App_Ctrl_TaskEvtHandler_PIRDebounceTimeOut(uint32_t u32EvtType, void *pData, uint32_t u32Len);
#ifdef __IDR_EN__  //from build config
static void App_Ctrl_TaskEvtHandler_IDRChange(uint32_t u32EvtType, void *pData, uint32_t u32Len);
static void App_Ctrl_TaskEvtHandler_IDRTimeOut(uint32_t u32EvtType, void *pData, uint32_t u32Len);
#endif

static void App_Ctrl_TaskEvtHandler_HttpPostDataInd(uint32_t u32EvtType, void *pData, uint32_t u32Len);

static void App_Ctrl_TaskEvtHandler_DailyBatteryPost(uint32_t u32EvtType, void *pData, uint32_t u32Len);
static void App_Ctrl_TaskEvtHandler_NetworkStop_Delay(uint32_t u32EvtType, void *pData, uint32_t u32Len);

#if(DAMAGE_EN == 1)
static void App_Ctrl_TaskEvtHandler_DoorDamageChange(uint32_t u32EvtType, void *pData, uint32_t u32Len);
static void App_Ctrl_TaskEvtHandler_DoorDamageTimeOut(uint32_t u32EvtType, void *pData, uint32_t u32Len);
#endif
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

    {APP_CTRL_MSG_PIR_STATECHANGE,                  App_Ctrl_TaskEvtHandler_PIRStateChange},
    {APP_CTRL_MSG_PIR_DEBOUNCETIMEOUT,              App_Ctrl_TaskEvtHandler_PIRDebounceTimeOut},
#ifdef __IDR_EN__  //from build config
    {APP_CTRL_MSG_IDR_CHANGE,                       App_Ctrl_TaskEvtHandler_IDRChange},
    {APP_CTRL_MSG_IDR_TIMEOUT,                      App_Ctrl_TaskEvtHandler_IDRTimeOut},
#endif

    {APP_CTRL_MSG_HTTP_POST_DATA_IND,               App_Ctrl_TaskEvtHandler_HttpPostDataInd},

    {APP_CTRL_MSG_DAILY_BATTERY_POST_TIMEOUT,       App_Ctrl_TaskEvtHandler_DailyBatteryPost},

	{APP_CTRL_MSG_NETWORK_STOP_DELAY_TIMEOUT,       App_Ctrl_TaskEvtHandler_NetworkStop_Delay},

#if(DAMAGE_EN == 1)
    {APP_CTRL_MSG_DOOR_DAMAGE_CHANGE,               App_Ctrl_TaskEvtHandler_DoorDamageChange},
    {APP_CTRL_MSG_DOOR_DAMAGE_TIMEOUT,              App_Ctrl_TaskEvtHandler_DoorDamageTimeOut},
#endif
    {APP_CTRL_MSG_AUTO_CONNECT_TIMEOUT,             App_Ctrl_TaskEvtHandler_AutoConnectTimeOut},
    {APP_CTRL_MSG_DATA_POST_FAIL,                   APP_Ctrl_TaskEvtHandler_PostFail},

    {0xFFFFFFFF,                                    NULL}
};

void App_Sensor_Set_Defense(uint8_t u8En)
{
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

    tSensorData.ubaPIRStatus = BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_PIR_DETECT);
#ifdef __IDR_EN__  //from build config
    tSensorData.ubaIDRstatus = BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_IDR_DETECT);
#endif
    tSensorData.ubaType = ubType;
    tSensorData.aTimeStamp = BleWifi_COM_SntpTimestampGet();

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
    printf("debug -- ubType %u Post PIR status: %u  (ts=%lu)\n",ubType , tSensorData.ubaPIRStatus, BleWifi_COM_SntpTimestampGet());

    IoT_Ring_Buffer_Push(&g_stIotRbData, &tProperity);

    //not ready, disable it fist
    Iot_Data_TxTask_MsgSend(IOT_DATA_TX_MSG_CLOUD_POST, NULL, 0);
}

void App_Ctrl_DoLedDisplay(void)
{
    if(!g_u8ShortPressButtonProcessed) //if short press button is processed, showing this light status is the 1st priority 20191018EL
    {
        switch (g_ubAppCtrlLedStatus)
        {
            case APP_CTRL_LED_BLE_ON_1:          // pair #1
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
            case APP_CTRL_LED_PIR_ONLINE_ON_1:
            case APP_CTRL_LED_PIR_OFFLINE_ON_1:
            case APP_CTRL_LED_PIR_OFFLINE_ON_2:
            case APP_CTRL_LED_PIR_MP_ON_1:
#if(DAMAGE_EN == 1)
            case APP_CTRL_LED_DAMAGE_ONLINE_ON_1:
            case APP_CTRL_LED_DAMAGE_OFFLINE_ON_1:
            case APP_CTRL_LED_DAMAGE_OFFLINE_ON_2:
#endif
            case APP_CTRL_LED_ALWAYS_ON:
                Hal_Vic_GpioOutput(LED_IO_PORT, GPIO_LEVEL_HIGH);
                break;
            case APP_CTRL_LED_BLE_OFF_1:          // pair #1
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
            case APP_CTRL_LED_PIR_ONLINE_OFF_1:
            case APP_CTRL_LED_PIR_OFFLINE_OFF_1:
            case APP_CTRL_LED_PIR_OFFLINE_OFF_2:
            case APP_CTRL_LED_PIR_MP_OFF_1:
#if(DAMAGE_EN == 1)
            case APP_CTRL_LED_DAMAGE_ONLINE_OFF_1:
            case APP_CTRL_LED_DAMAGE_OFFLINE_OFF_1:
            case APP_CTRL_LED_DAMAGE_OFFLINE_OFF_2:
#endif
            case APP_CTRL_LED_ALWAYS_OFF:         // LED always off
                Hal_Vic_GpioOutput(LED_IO_PORT, GPIO_LEVEL_LOW);
                break;
            // error handle
            default:
                Hal_Vic_GpioOutput(LED_IO_PORT, GPIO_LEVEL_LOW);
                return;
        }
    }

    // start the led timer
    osTimerStop(g_tAppCtrlLedTimer);
    osTimerStart(g_tAppCtrlLedTimer, g_ulaAppCtrlLedInterval[g_ubAppCtrlLedStatus]);
}

void App_Ctrl_LedStatusChange(void)
{
    // LED status:BLE MP mode > Normal mode
    // MP mode : Button > PIR > no server = no router
    // Normal mode : Button > Adv > DAMAGE > PIR > IDR > OTA > BLE > Wifi > WiFi Autoconnect > None

    if(true == g_u8MpIsFindTargetAP)
    {
        if (true == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_BUTTON_PRESS))
        {
            g_ubAppCtrlLedStatus = APP_CTRL_LED_ALWAYS_ON;
        }
        //PIR
        else if (true == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_PIR_DETECT))
        {
            g_ubAppCtrlLedStatus = APP_CTRL_LED_PIR_MP_ON_1;
        }
        else if(g_u8MpBlinkingMode == MP_BLINKING_NO_SERVER)
        {
            g_ubAppCtrlLedStatus = APP_CTRL_LED_MP_NO_SERVER_ON_1;
        }
        else if(g_u8MpBlinkingMode == MP_BLINKING_NO_ROUTER)
        {
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
    // Network now
    else if (true == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_NETWORK))
    {
        // status change
        g_ubAppCtrlLedStatus = APP_CTRL_LED_BLE_OFF_1;
        App_Ctrl_DoLedDisplay();
    }
#if(DAMAGE_EN == 1)
    else if (true == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_DAMAGE_TRIGGER))
    {
        if(true == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_WIFI_GOT_IP))
        {
            g_ubAppCtrlLedStatus = APP_CTRL_LED_DAMAGE_ONLINE_ON_1;
        }
        else
        {
            g_ubAppCtrlLedStatus = APP_CTRL_LED_DAMAGE_OFFLINE_ON_1;
        }
        App_Ctrl_DoLedDisplay();
    }
#endif
    else if (true == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_PIR_DETECT))
    {
        if(true == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_WIFI_GOT_IP))
        {
            g_ubAppCtrlLedStatus = APP_CTRL_LED_PIR_ONLINE_ON_1;
        }
        else
        {
            g_ubAppCtrlLedStatus = APP_CTRL_LED_PIR_OFFLINE_ON_1;
        }
        App_Ctrl_DoLedDisplay();
    }
#ifdef __IDR_EN__  //from build config
    else if (true == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_IDR_DETECT))
    {
        // status change
        g_ubAppCtrlLedStatus = APP_CTRL_LED_ALWAYS_ON;
        App_Ctrl_DoLedDisplay();
    }
#endif
    // OTA
    else if (true == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_OTA))
    {
        // status change
        g_ubAppCtrlLedStatus = APP_CTRL_LED_OTA_OFF;
        App_Ctrl_DoLedDisplay();
    }
#if 0
    // Wifi Auto
    else if (true == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_WIFI_AUTOCONN_LED))
    {
        // status change
        g_ubAppCtrlLedStatus = APP_CTRL_LED_AUTOCONN_OFF_1;
        App_Ctrl_DoLedDisplay();
    }
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
    #if 0  //Goter
    // Short Press
    else if (true == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, BLEWIFI_CTRL_EVENT_BIT_SHORT_PRESS))
    {
        // status change
        g_ubAppCtrlLedStatus = APP_CTRL_LED_SHORT_PRESS_ON;
        App_Ctrl_DoLedDisplay();
    }
    #endif
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

        case APP_CTRL_LED_PIR_ONLINE_ON_1:
            g_ubAppCtrlLedStatus = APP_CTRL_LED_PIR_ONLINE_OFF_1;
            break;

        case APP_CTRL_LED_PIR_ONLINE_OFF_1:
            g_ubAppCtrlLedStatus = APP_CTRL_LED_ALWAYS_OFF;
            break;

        case APP_CTRL_LED_PIR_OFFLINE_ON_1:
            g_ubAppCtrlLedStatus = APP_CTRL_LED_PIR_OFFLINE_OFF_1;
            break;

        case APP_CTRL_LED_PIR_OFFLINE_OFF_1:
            g_ubAppCtrlLedStatus = APP_CTRL_LED_PIR_OFFLINE_ON_2;
            break;

        case APP_CTRL_LED_PIR_OFFLINE_ON_2:
            g_ubAppCtrlLedStatus = APP_CTRL_LED_PIR_OFFLINE_OFF_2;
            break;

        case APP_CTRL_LED_PIR_OFFLINE_OFF_2:
            g_ubAppCtrlLedStatus = APP_CTRL_LED_ALWAYS_OFF;
            break;

        case APP_CTRL_LED_PIR_MP_ON_1:
            g_ubAppCtrlLedStatus = APP_CTRL_LED_PIR_ONLINE_OFF_1;
            break;

        case APP_CTRL_LED_PIR_MP_OFF_1:
            g_ubAppCtrlLedStatus = APP_CTRL_LED_ALWAYS_OFF;
            break;

#if(DAMAGE_EN == 1)
        case APP_CTRL_LED_DAMAGE_ONLINE_ON_1:
            g_ubAppCtrlLedStatus = APP_CTRL_LED_DAMAGE_ONLINE_OFF_1;
            break;

        case APP_CTRL_LED_DAMAGE_ONLINE_OFF_1:
            g_ubAppCtrlLedStatus = APP_CTRL_LED_ALWAYS_OFF;
            break;

        case APP_CTRL_LED_DAMAGE_OFFLINE_ON_1:
            g_ubAppCtrlLedStatus = APP_CTRL_LED_DAMAGE_OFFLINE_OFF_1;
            break;

        case APP_CTRL_LED_DAMAGE_OFFLINE_OFF_1:
            g_ubAppCtrlLedStatus = APP_CTRL_LED_DAMAGE_OFFLINE_ON_2;
            break;

        case APP_CTRL_LED_DAMAGE_OFFLINE_ON_2:
            g_ubAppCtrlLedStatus = APP_CTRL_LED_DAMAGE_OFFLINE_ON_2;
            break;

        case APP_CTRL_LED_DAMAGE_OFFLINE_OFF_2:
            g_ubAppCtrlLedStatus = APP_CTRL_LED_ALWAYS_OFF;
            break;
#endif
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

    if((true == g_u8MpScanProcessFinished) && (false == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_WIFI_GOT_IP)))
    {
        printf("---->AutoConn...HttpPostData\n");
        // the idle of the connection retry
        BleWifi_Wifi_Start_Conn(NULL);
    }
}

static void App_Ctrl_TaskEvtHandler_DailyBatteryPost(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
    BLEWIFI_INFO("BLEWIFI: MSG BLEWIFI_CTRL_MSG_HTTP_POST_DATA_IND \r\n");

    // Trigger to send http data
    App_Sensor_Post_To_Cloud(TIMER_POST);
    if((true == g_u8MpScanProcessFinished) && (false == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_WIFI_GOT_IP)))
    {
        printf("---->AutoConn...DailyBatteryPost\n");
        // the idle of the connection retry
        BleWifi_Wifi_Start_Conn(NULL);
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
}

#if(DAMAGE_EN == 1)
static void App_Ctrl_TaskEvtHandler_DoorDamageChange(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
    BLEWIFI_INFO("BLEWIFI: MSG BLEWIFI_CTRL_MSG_DOOR_DAMAGE_CHANGE \r\n");
    /* Start timer to debounce */
    osTimerStop(g_tAppDoorDamageTimerId);
    osTimerStart(g_tAppDoorDamageTimerId, DAMAGE_DEBOUNCE_TIMEOUT);
}

static void App_Ctrl_TaskEvtHandler_DoorDamageTimeOut(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
    unsigned int u32PinLevel = 0;

    // Get the status of GPIO (Low / High)
    u32PinLevel = Hal_Vic_GpioInput(DOOR_SENSOR_DAMAGE_IO_PORT);

    if(GPIO_LEVEL_LOW == u32PinLevel)
    {
        printf("DoorDamage\r\n");
        /* Disable - Invert gpio interrupt signal */
        Hal_Vic_GpioIntInv(DOOR_SENSOR_DAMAGE_IO_PORT, 0);
        // Enable Interrupt
        Hal_Vic_GpioIntEn(DOOR_SENSOR_DAMAGE_IO_PORT, 1);

        BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup,APP_CTRL_EVENT_BIT_DAMAGE_TRIGGER, true);
        App_Ctrl_LedStatusChange();

        // Trigger to send http data
        App_Sensor_Post_To_Cloud(DOOR_SENSOR_DAMAGE);
        if((true == g_u8MpScanProcessFinished) && (false == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_WIFI_GOT_IP)))
        {
            printf("---->AutoConn...DamageTimeOut\n");
            // the idle of the connection retry
            BleWifi_Wifi_Start_Conn(NULL);
        }
    }
    else
    {
        BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup,APP_CTRL_EVENT_BIT_DAMAGE_TRIGGER, false);
        /* Disable - Invert gpio interrupt signal */
        Hal_Vic_GpioIntInv(DOOR_SENSOR_DAMAGE_IO_PORT, 1);
        // Enable Interrupt
        Hal_Vic_GpioIntEn(DOOR_SENSOR_DAMAGE_IO_PORT, 1);
    }
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

void App_Ctrl_HttpPostData(void const *argu)
{
    App_Ctrl_MsgSend(APP_CTRL_MSG_HTTP_POST_DATA_IND, NULL, 0);
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

static void App_Cloud_Disconnect_TimeOutCallBack(void const *argu)
{
    Iot_Data_TxTask_MsgSend(IOT_DATA_TX_MSG_CLOUD_DISCONNECTION, NULL, 0);
}

static void App_Daily_Battery_Post_TimeOutCallBack(void const *argu)
{
    App_Ctrl_MsgSend(APP_CTRL_MSG_DAILY_BATTERY_POST_TIMEOUT, NULL, 0);
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
    }
    App_Ctrl_LedStatusChange();

}

static void App_Ctrl_TaskEvtHandler_WifiScanDone(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
    uint8_t i = 0;
    uint8_t u8IsMatched = false;
    wifi_scan_list_t stScanList = {0};
    wifi_scan_config_t stScanConfig = {0};

    stScanConfig.ssid = MP_WIFI_DEFAULT_SSID;
    stScanConfig.show_hidden = 1;
    stScanConfig.scan_type = WIFI_SCAN_TYPE_MIX;

    BLEWIFI_INFO("BLEWIFI: MSG APP_CTRL_MSG_WIFI_SCAN_DONE \r\n");
    BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_WIFI_SCANNING , false);

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
        BleWifi_Wifi_SendScanReport();
        BleWifi_Ble_SendResponse(BLEWIFI_RSP_SCAN_END, 0);
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

    BleWifi_Ble_SendResponse(BLEWIFI_RSP_CONNECT, BLEWIFI_WIFI_CONNECTED_DONE);


    BleWifi_Wifi_SendStatusInfo(BLEWIFI_IND_IP_STATUS_NOTIFY);

    UpdateBatteryContent();

    BleWifi_Wifi_UpdateBeaconInfo();
    stSetDtim.u32DtimValue = (uint32_t)BleWifi_Wifi_GetDtimSetting();
    stSetDtim.u32DtimEventBit = BW_WIFI_DTIM_EVENT_BIT_DHCP_USE;
    BleWifi_Wifi_Set_Config(BLEWIFI_WIFI_SET_DTIM , (void *)&stSetDtim);

    BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_WIFI_AUTOCONN_LED, false);
    App_Ctrl_LedStatusChange();

//    Sensor_Data_Push(BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_PIR_DETECT), TIMER_POST,  BleWifi_COM_SntpGetRawData());
//    Iot_Data_TxTask_MsgSend(IOT_DATA_TX_MSG_CLOUD_POST, NULL, 0);
//    App_Sensor_Post_To_Cloud(TIMER_POST);

    IoT_Ring_Buffer_ResetBuffer(&g_stIotRbData);

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

#ifdef __BLEWIFI_TRANSPARENT__
    msg_print_uart1("WIFI DISCONNECTION\n");
#endif

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

    stSetDtim.u32DtimValue = 0;
    stSetDtim.u32DtimEventBit = BW_WIFI_DTIM_EVENT_BIT_DHCP_USE;
    BleWifi_Wifi_Set_Config(BLEWIFI_WIFI_SET_DTIM , (void *)&stSetDtim);

    BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_WIFI_AUTOCONN_LED , true);
    App_Ctrl_LedStatusChange();

    BleWifi_Ble_SendResponse(BLEWIFI_RSP_DISCONNECT, BLEWIFI_WIFI_DISCONNECTED_DONE);
}

static void App_Ctrl_TaskEvtHandler_WifiStopComplete(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
    printf("Wifi StopComplete\r\n");
    if(true == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_WAIT_UPDATE_HOST))
    {
        BleWifi_Wifi_Start_Conn(&g_stStartConnConfig);
        BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_WIFI_USER_CONNECTING_EXEC , true);
    }
}

static void App_Ctrl_TaskEvtHandler_OtherOtaOn(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
    BLEWIFI_INFO("BLEWIFI: MSG APP_CTRL_MSG_OTHER_OTA_ON \r\n");
    BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_OTA , true);
    App_Ctrl_LedStatusChange();

    // Start OTA timeout timer
    osTimerStop(g_ota_timeout_timer);
    osTimerStart(g_ota_timeout_timer, OTA_TOTAL_TIMEOUT);
}

static void App_Ctrl_TaskEvtHandler_OtherOtaOff(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
    BLEWIFI_INFO("BLEWIFI: MSG APP_CTRL_MSG_OTHER_OTA_OFF \r\n");
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
    BLEWIFI_INFO("BLEWIFI: MSG APP_CTRL_MSG_OTHER_OTA_OFF_FAIL \r\n");
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

        // special case , let LED change to networking 
        BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_BUTTON_PRESS , false);
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
        Hal_Vic_GpioOutput(g_eLedIOPort, GPIO_LEVEL_LOW);
        App_Ctrl_LedStatusChange();

        g_u8ShortPressButtonProcessed = 0;

        App_ButtonFsm_Run(APP_BUTTON_EVENT_RELEASE);
    }
}
static void App_Ctrl_TaskEvtHandler_PIRStateChange(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
    unsigned int u32PinLevel = 0;

    BLEWIFI_INFO("BLEWIFI: MSG PIRStateChange \r\n");

    // Get the status of GPIO (Low / High)
    u32PinLevel = Hal_Vic_GpioInput(PIR_IO_PORT);
    //printf("PIR_IO_PORT pin level = %s\r\n", u32PinLevel ? "GPIO_LEVEL_HIGH" : "GPIO_LEVEL_LOW");

    if(APP_CTRL_PIR_IO_BLOCK == u32PinLevel)
    {
        if(BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_PIR_DETECT) == true) // status not change
        {
            /* Enable - Invert gpio interrupt signal */
            Hal_Vic_GpioIntInv(PIR_IO_PORT, (APP_CTRL_PIR_IO_BLOCK == GPIO_LEVEL_HIGH)?1:0);
            // Enable Interrupt
            Hal_Vic_GpioIntEn(PIR_IO_PORT, 1);
        }
        else if (BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_PIR_DETECT) == false)
        {
            BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup,APP_CTRL_EVENT_BIT_PIR_DETECT, true); // PIR block

            osTimerStop(g_tAppPIRTimerId);
            osTimerStart(g_tAppPIRTimerId, PIR_IDR_DEBOUNCE_TIMEOUT);

            osTimerStop(g_cloud_DailyBatteryPost_timeout_timer);
            osTimerStart(g_cloud_DailyBatteryPost_timeout_timer, IDLE_POST_TIME);

            if (false == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_NETWORK))
            {
                App_Ctrl_LedStatusChange();
                App_Sensor_Post_To_Cloud(PIR);
                if((true == g_u8MpScanProcessFinished) && (false == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_WIFI_GOT_IP)))
                {
                    printf("---->AutoConn...PIR\n");
                    // the idle of the connection retry
                    BleWifi_Wifi_Start_Conn(NULL);
                }
            }
        }
    }
    else
    {
        /* Disable - Invert gpio interrupt signal */
        Hal_Vic_GpioIntInv(PIR_IO_PORT, (APP_CTRL_PIR_IO_BLOCK == GPIO_LEVEL_HIGH)?0:1);
        // Enable Interrupt
        Hal_Vic_GpioIntEn(PIR_IO_PORT, 1);

        if (BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_PIR_DETECT) == true)
        {
            BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_PIR_DETECT, false); // PIR empty

            osTimerStop(g_cloud_DailyBatteryPost_timeout_timer);
            osTimerStart(g_cloud_DailyBatteryPost_timeout_timer, IDLE_POST_TIME);

            if (false == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_NETWORK))
            {
                App_Ctrl_LedStatusChange();
                App_Sensor_Post_To_Cloud(PIR);
                if((true == g_u8MpScanProcessFinished) && (false == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_WIFI_GOT_IP)))
                {
                    printf("---->AutoConn...PIR\n");
                    // the idle of the connection retry
                    BleWifi_Wifi_Start_Conn(NULL);
                }
            }
        }
    }

}

static void App_Ctrl_TaskEvtHandler_PIRDebounceTimeOut(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
    BLEWIFI_INFO("BLEWIFI: PIRDebounceTimeOut \r\n");
    unsigned int u32PinLevel = 0;

    // Get the status of GPIO (Low / High)
    u32PinLevel = Hal_Vic_GpioInput(PIR_IO_PORT);

    if(APP_CTRL_PIR_IO_BLOCK == u32PinLevel)
    {
        /* Enable - Invert gpio interrupt signal */
        Hal_Vic_GpioIntInv(PIR_IO_PORT, (APP_CTRL_PIR_IO_BLOCK == GPIO_LEVEL_HIGH)?1:0);
        // Enable Interrupt
        Hal_Vic_GpioIntEn(PIR_IO_PORT, 1);
        if (BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_PIR_DETECT) == false)
        {
            BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup,APP_CTRL_EVENT_BIT_PIR_DETECT, true);
            if (false == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_NETWORK))
            {
                App_Ctrl_LedStatusChange();
                App_Sensor_Post_To_Cloud(PIR);
                if((true == g_u8MpScanProcessFinished) && (false == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_WIFI_GOT_IP)))
                {
                    printf("---->AutoConn...PIR\n");
                    // the idle of the connection retry
                    BleWifi_Wifi_Start_Conn(NULL);
                }
            }
        }
    }
    else
    {
        /* Disable - Invert gpio interrupt signal */
        Hal_Vic_GpioIntInv(PIR_IO_PORT, (APP_CTRL_PIR_IO_BLOCK == GPIO_LEVEL_HIGH)?0:1);
        // Enable Interrupt
        Hal_Vic_GpioIntEn(PIR_IO_PORT, 1);
        if (BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_PIR_DETECT) == true)
        {
            BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup,APP_CTRL_EVENT_BIT_PIR_DETECT, false);
            if (false == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_NETWORK))
            {
                App_Ctrl_LedStatusChange();
                App_Sensor_Post_To_Cloud(PIR);
                if((true == g_u8MpScanProcessFinished) && (false == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_WIFI_GOT_IP)))
                {
                    printf("---->AutoConn...PIR\n");
                    // the idle of the connection retry
                    BleWifi_Wifi_Start_Conn(NULL);
                }
            }
        }
    }
}
#ifdef __IDR_EN__  //from build config
static void App_Ctrl_TaskEvtHandler_IDRChange(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
    unsigned int u32PinLevel = 0;

    BLEWIFI_INFO("BLEWIFI: MSG BLEWIFI_CTRL_MSG_IDRChange_CHANGE \r\n");

    // Get the status of GPIO (Low / High)
    u32PinLevel = Hal_Vic_GpioInput(IDR_IO_PORT);
    //printf("IDR_IO_PORT pin level = %s\r\n", u32PinLevel ? "GPIO_LEVEL_HIGH" : "GPIO_LEVEL_LOW");

    if(APP_CTRL_IDR_IO_LIGHT == u32PinLevel)
    {
        if(BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_IDR_DETECT) == true) // status not change
        {
            /* Enable - Invert gpio interrupt signal */
            Hal_Vic_GpioIntInv(IDR_IO_PORT, (APP_CTRL_IDR_IO_LIGHT == GPIO_LEVEL_HIGH)?1:0);
            // Enable Interrupt
            Hal_Vic_GpioIntEn(IDR_IO_PORT, 1);
        }
        else if (BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_IDR_DETECT) == false)
        {
            osTimerStop(g_tAppIDRTimerId);
            osTimerStart(g_tAppIDRTimerId, PIR_IDR_DEBOUNCE_TIMEOUT);

            BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup,APP_CTRL_EVENT_BIT_IDR_DETECT, true); // IDR light
            App_Ctrl_LedStatusChange();

            osTimerStop(g_cloud_DailyBatteryPost_timeout_timer);
            osTimerStart(g_cloud_DailyBatteryPost_timeout_timer, IDLE_POST_TIME);

            App_Sensor_Post_To_Cloud(IDR);
            if((true == g_u8MpScanProcessFinished) && (false == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_WIFI_GOT_IP)))
            {
                printf("---->AutoConn...IDR\n");
                // the idle of the connection retry
                BleWifi_Wifi_Start_Conn(NULL);
            }
        }
    }
    else
    {
        /* Disable - Invert gpio interrupt signal */
        Hal_Vic_GpioIntInv(IDR_IO_PORT, (APP_CTRL_IDR_IO_LIGHT == GPIO_LEVEL_HIGH)?0:1);
        // Enable Interrupt
        Hal_Vic_GpioIntEn(IDR_IO_PORT, 1);

        if (BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_IDR_DETECT) == true)
        {
            BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_IDR_DETECT, false); // IDR dark
            App_Ctrl_LedStatusChange();

            osTimerStop(g_cloud_DailyBatteryPost_timeout_timer);
            osTimerStart(g_cloud_DailyBatteryPost_timeout_timer, IDLE_POST_TIME);

            App_Sensor_Post_To_Cloud(IDR);
            if((true == g_u8MpScanProcessFinished) && (false == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_WIFI_GOT_IP)))
            {
                printf("---->AutoConn...IDR\n");
                // the idle of the connection retry
                BleWifi_Wifi_Start_Conn(NULL);
            }
        }
    }

}

static void App_Ctrl_TaskEvtHandler_IDRTimeOut(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
    printf("AppCtrl: MSG IDRTimeOut \r\n");

    unsigned int u32PinLevel = 0;

    // Get the status of GPIO (Low / High)
    u32PinLevel = Hal_Vic_GpioInput(IDR_IO_PORT);

    if(APP_CTRL_IDR_IO_LIGHT == u32PinLevel)
    {
        /* Enable - Invert gpio interrupt signal */
        Hal_Vic_GpioIntInv(IDR_IO_PORT, (APP_CTRL_IDR_IO_LIGHT == GPIO_LEVEL_HIGH)?1:0);
        // Enable Interrupt
        Hal_Vic_GpioIntEn(IDR_IO_PORT, 1);
        if (BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_IDR_DETECT) == false)
        {
            BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup,APP_CTRL_EVENT_BIT_IDR_DETECT, true);
            App_Ctrl_LedStatusChange();
            App_Sensor_Post_To_Cloud(IDR);
            if((true == g_u8MpScanProcessFinished) && (false == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_WIFI_GOT_IP)))
            {
                printf("---->AutoConn...IDR\n");
                // the idle of the connection retry
                BleWifi_Wifi_Start_Conn(NULL);
            }
        }
    }
    else
    {
        /* Disable - Invert gpio interrupt signal */
        Hal_Vic_GpioIntInv(IDR_IO_PORT, (APP_CTRL_IDR_IO_LIGHT == GPIO_LEVEL_HIGH)?0:1);
        // Enable Interrupt
        Hal_Vic_GpioIntEn(IDR_IO_PORT, 1);
        if (BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_IDR_DETECT) == true)
        {
            BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup,APP_CTRL_EVENT_BIT_IDR_DETECT, false);
            App_Ctrl_LedStatusChange();
            App_Sensor_Post_To_Cloud(IDR);
            if((true == g_u8MpScanProcessFinished) && (false == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_WIFI_GOT_IP)))
            {
                printf("---->AutoConn...IDR\n");
                // the idle of the connection retry
                BleWifi_Wifi_Start_Conn(NULL);
            }
        }
    }

}
#endif
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

//            Sensor_Data_Push(BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_PIR_DETECT), SHORT_TRIG,  BleWifi_COM_SntpGetRawData());
//            Iot_Data_TxTask_MsgSend(IOT_DATA_TX_MSG_CLOUD_POST, NULL, 0);
            App_Sensor_Post_To_Cloud(SHORT_TRIG);
            if((true == g_u8MpScanProcessFinished) && (false == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_WIFI_GOT_IP)))
            {
                printf("---->AutoConn...ButtonRelease\n");
                // the idle of the connection retry
                BleWifi_Wifi_Start_Conn(NULL);
            }
        }
        else if(u8ReleaseCount == 2)
        {
            printf("[%s %d] release twice\n", __func__, __LINE__);

//            Sensor_Data_Push(BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_PIR_DETECT), DOUBLE_SHORT_TRIG,  BleWifi_COM_SntpGetRawData());
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
    osTimerDef_t timer_cloud_disconnection_timeout_def;
    osTimerDef_t timer_daily_battery_post_timeout_def;
    osTimerDef_t timer_network_stop_delay_timeout_def;

     memset(&g_tHostInfo ,0, sizeof(T_MwFim_GP12_HttpHostInfo));
     memset(&g_tHttpPostContent ,0, sizeof(T_MwFim_GP12_HttpPostContent));
#if 0 // Defense functional implement by app
     memset(&g_tSystemDefenseEn ,0, sizeof(T_MwFim_GP17_System_Defense_En));
#endif

     // Goter, got MW_FIM_IDX_GP12_PROJECT_DEVICE_AUTH_CONTENT / MW_FIM_IDX_GP12_PROJECT_HOST_INFO
     if (MW_FIM_OK != MwFim_FileRead(MW_FIM_IDX_GP12_PROJECT_DEVICE_AUTH_CONTENT, 0, MW_FIM_GP12_HTTP_POST_CONTENT_SIZE, (uint8_t *)&g_tHttpPostContent))
     {
         memcpy(&g_tHttpPostContent, &g_tMwFimDefaultGp12HttpPostContent, MW_FIM_GP12_HTTP_POST_CONTENT_SIZE);
     }

     if (MW_FIM_OK != MwFim_FileRead(MW_FIM_IDX_GP12_PROJECT_HOST_INFO, 0, MW_FIM_GP12_HTTP_HOST_INFO_SIZE, (uint8_t *)&g_tHostInfo) ) {
         memcpy(&g_tHostInfo, &g_tMwFimDefaultGp12HttpHostInfo, MW_FIM_GP12_HTTP_HOST_INFO_SIZE);
     }

#if 0 // Defense functional implement by app
    if (MW_FIM_OK != MwFim_FileRead(MW_FIM_IDX_GP17_PROJECT_SYSTEM_DEFENSE_EN, 0, MW_FIM_GP17_SYSTEM_DEFENSE_EN_SIZE, (uint8_t *)&g_tSystemDefenseEn) )
    {
        memcpy(&g_tSystemDefenseEn, &g_tMwFimDefaultGp17SystemDefenseEn, MW_FIM_GP17_SYSTEM_DEFENSE_EN_SIZE);
    }
#endif
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

    /* create cloud disconnect timeout timer */
    timer_cloud_disconnection_timeout_def.ptimer = App_Cloud_Disconnect_TimeOutCallBack;
    g_cloud_disconnect_timeout_timer = osTimerCreate(&timer_cloud_disconnection_timeout_def, osTimerOnce, NULL);
    if (g_cloud_disconnect_timeout_timer == NULL)
    {
        BLEWIFI_ERROR("BLEWIFI: create cloud disconnect timeout timer fail \r\n");
    }

    /* create daily battery post timer */
    timer_daily_battery_post_timeout_def.ptimer = App_Daily_Battery_Post_TimeOutCallBack;
    g_cloud_DailyBatteryPost_timeout_timer = osTimerCreate(&timer_daily_battery_post_timeout_def, osTimerOnce, NULL);
    if (g_cloud_DailyBatteryPost_timeout_timer == NULL)
    {
        BLEWIFI_ERROR("BLEWIFI: create battery daily post timeout timer fail \r\n");
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

    /* Init Sensor Action */
    Sensor_Init();

    /* Init Auxadc for battery */
    Sensor_Auxadc_Init();

    g_u8ShortPressButtonProcessed = 0; //It means the time of button pressed is less than 5s 20191018EL

}
