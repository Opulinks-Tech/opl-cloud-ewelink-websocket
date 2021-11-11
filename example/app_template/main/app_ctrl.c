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
#include "ps_public.h"
#if (APP_CTRL_BUTTON_SENSOR_EN == 1)
#include "btn_press_ctrl.h"
#endif
#if (APP_CTRL_WAKEUP_IO_EN == 1)
#include "smart_sleep.h"
#endif
#include "blewifi_wifi_FSM.h"

#define APP_CTRL_RESET_DELAY    (3000)  // ms

osThreadId   g_tAppCtrlTaskId;
osMessageQId g_tAppCtrlQueueId;

osTimerId    g_tAppCtrlSysTimer;
osTimerId    g_tAppCtrlNetworkTimerId;

EventGroupHandle_t g_tAppCtrlEventGroup;

uint8_t g_u8AppCtrlSysMode;
uint8_t g_u8AppCtrlSysStatus;


extern blewifi_ota_t *gTheOta;

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

static void App_Ctrl_TaskEvtHandler_OtherOtaOn(uint32_t u32EvtType, void *pData, uint32_t u32Len);
static void App_Ctrl_TaskEvtHandler_OtherOtaOff(uint32_t u32EvtType, void *pData, uint32_t u32Len);
static void App_Ctrl_TaskEvtHandler_OtherOtaOffFail(uint32_t u32EvtType, void *pData, uint32_t u32Len);

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

    {APP_CTRL_MSG_OTHER_OTA_ON,                     App_Ctrl_TaskEvtHandler_OtherOtaOn},
    {APP_CTRL_MSG_OTHER_OTA_OFF,                    App_Ctrl_TaskEvtHandler_OtherOtaOff},
    {APP_CTRL_MSG_OTHER_OTA_OFF_FAIL,               App_Ctrl_TaskEvtHandler_OtherOtaOffFail},

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

    {0xFFFFFFFF,                                    NULL}
};

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
    uint32_t u32BleTimeOut = APP_BLE_ADV_TIMEOUT;

    printf("[ATS]BLE init complete\r\n");

    BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_BLE_INIT_DONE , true);

    App_Ctrl_MsgSend(APP_CTRL_MSG_NETWORKING_START, (uint8_t *)&u32BleTimeOut, sizeof(u32BleTimeOut));
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

    BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_BLE_CONNECTED , false);

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
    BLEWIFI_INFO("BLEWIFI: MSG APP_CTRL_MSG_WIFI_INIT_COMPLETE \r\n");
    BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_WIFI_INIT_DONE , true);

    BleWifi_Wifi_Start_Conn(NULL);
}

static void App_Ctrl_TaskEvtHandler_WifiScanDone(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
    BLEWIFI_INFO("BLEWIFI: MSG APP_CTRL_MSG_WIFI_SCAN_DONE \r\n");
    BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_WIFI_SCANNING , false);

    BleWifi_Wifi_SendScanReport();
    BleWifi_Ble_SendResponse(BLEWIFI_RSP_SCAN_END, 0);

}

static void App_Ctrl_TaskEvtHandler_WifiConnection(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
    blewifi_wifi_set_dtim_t stSetDtim = {0};

    BLEWIFI_INFO("BLEWIFI: MSG APP_CTRL_MSG_WIFI_CONNECTION \r\n");
#ifdef __BLEWIFI_TRANSPARENT__
    msg_print_uart1("WIFI CONNECTED\n");
#endif
    BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_WIFI_CONNECTING , false);
    BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_WIFI_CONNECTED , true);

    BleWifi_Ble_SendResponse(BLEWIFI_RSP_CONNECT, BLEWIFI_WIFI_CONNECTED_DONE);


    BleWifi_Wifi_SendStatusInfo(BLEWIFI_IND_IP_STATUS_NOTIFY);

    BleWifi_Wifi_UpdateBeaconInfo();
    stSetDtim.u32DtimValue = (uint32_t)BleWifi_Wifi_GetDtimSetting();
    stSetDtim.u32DtimEventBit = BW_WIFI_DTIM_EVENT_BIT_DHCP_USE;
    BleWifi_Wifi_Set_Config(BLEWIFI_WIFI_SET_DTIM , (void *)&stSetDtim);

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
//    uint8_t *pu8Reason = (uint8_t*)(pData);

    BLEWIFI_INFO("BLEWIFI: MSG APP_CTRL_MSG_WIFI_DISCONNECTION\r\n");
    BLEWIFI_INFO("reason %d\r\n", *pu8Reason);

#ifdef __BLEWIFI_TRANSPARENT__
    msg_print_uart1("WIFI DISCONNECTION\n");
#endif

    BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_WIFI_CONNECTING , false);
    BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_WIFI_CONNECTED , false);
    BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_WIFI_GOT_IP , false);

    stSetDtim.u32DtimValue = 0;
    stSetDtim.u32DtimEventBit = BW_WIFI_DTIM_EVENT_BIT_DHCP_USE;
    BleWifi_Wifi_Set_Config(BLEWIFI_WIFI_SET_DTIM , (void *)&stSetDtim);

    BleWifi_Ble_SendResponse(BLEWIFI_RSP_DISCONNECT, BLEWIFI_WIFI_DISCONNECTED_DONE);
}

static void App_Ctrl_TaskEvtHandler_OtherOtaOn(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
    BLEWIFI_INFO("BLEWIFI: MSG APP_CTRL_MSG_OTHER_OTA_ON \r\n");
    BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_OTA , true);
}

static void App_Ctrl_TaskEvtHandler_OtherOtaOff(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
    BLEWIFI_INFO("BLEWIFI: MSG APP_CTRL_MSG_OTHER_OTA_OFF \r\n");
    BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_OTA , false);
    msg_print_uart1("OK\r\n");

    // restart the system
    osDelay(APP_CTRL_RESET_DELAY);
    Hal_Sys_SwResetAll();
}

static void App_Ctrl_TaskEvtHandler_OtherOtaOffFail(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
    BLEWIFI_INFO("BLEWIFI: MSG APP_CTRL_MSG_OTHER_OTA_OFF_FAIL \r\n");
    BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_OTA , false);
    msg_print_uart1("ERROR\r\n");
}

static void App_Ctrl_TaskEvtHandler_OtherSysTimer(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
    BLEWIFI_INFO("BLEWIFI: MSG APP_CTRL_MSG_OTHER_SYS_TIMER \r\n");
    App_Ctrl_SysStatusChange();
}

void App_Ctrl_NetworkingStart(uint32_t u32ExpireTime)
{
    if (false == BleWifi_COM_EventStatusGet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_NETWORK))
    {
        BLEWIFI_INFO("[%s %d] start\n", __func__, __LINE__);

        BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_NETWORK , true);

        BleWifi_Ble_Start(100); //100 ms

        osTimerStop(g_tAppCtrlNetworkTimerId);
        if ( u32ExpireTime > 0 )
        {
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
        BLEWIFI_INFO("[%s %d] start\n", __func__, __LINE__);

        osTimerStop(g_tAppCtrlNetworkTimerId);
        BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup , APP_CTRL_EVENT_BIT_NETWORK , false);

        BleWifi_Ble_Stop();
    }
    else
    {
        BLEWIFI_WARN("[%s %d] BLEWIFI_APP_CTRL_EVENT_BIT_NETWORK already false\n", __func__, __LINE__);
    }
}

void App_Ctrl_NetworkingTimeout(void const *argu)
{
    App_Ctrl_MsgSend(APP_CTRL_MSG_NETWORKING_STOP, NULL, 0);
}

static void App_Ctrl_TaskEvtHandler_NetworkingStart(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
    uint32_t *pu32ExpireTime;

    pu32ExpireTime = (uint32_t *)pData;
    App_Ctrl_NetworkingStart(*pu32ExpireTime);
}

static void App_Ctrl_TaskEvtHandler_NetworkingStop(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
    App_Ctrl_NetworkingStop();
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

    BLEWIFI_INFO("BLEWIFI: MSG APP_CTRL_MSG_BUTTON_DEBOUNCE_TIMEOUT \r\n");

    // Get the status of GPIO (Low / High)
    u32PinLevel = Hal_Vic_GpioInput(APP_CTRL_BUTTON_IO_PORT);
    BLEWIFI_INFO("APP_CTRL_BUTTON_IO_PORT pin level = %s\r\n", u32PinLevel ? "GPIO_LEVEL_HIGH" : "GPIO_LEVEL_LOW");

    if(GPIO_LEVEL_LOW == u32PinLevel)
    {
        /* Disable - Invert gpio interrupt signal */
        Hal_Vic_GpioIntInv(CTRL_BUTTON_IO_PORT, 0);
        // Enable Interrupt
        Hal_Vic_GpioIntEn(CTRL_BUTTON_IO_PORT, 1);
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
        App_ButtonFsm_Run(APP_BUTTON_EVENT_PRESS);
    }
    else
    {
        App_ButtonFsm_Run(APP_BUTTON_EVENT_RELEASE);
    }
}

static void App_Ctrl_TaskEvtHandler_ButtonReleaseTimeOut(uint32_t u32EvtType, void *pData, uint32_t u32Len)
{
    // used in short/long press
    App_ButtonFsm_Run(APP_BUTTON_EVENT_TIMEOUT);
}

void BleWifi_APP_Ctrl_ButtonReleaseHandle(uint8_t u8ReleaseCount)
{
    if(u8ReleaseCount == 1)
    {
        BLEWIFI_INFO("[%s %d] release once\n", __func__, __LINE__);
    }
    else if(u8ReleaseCount >= 2)
    {
        BLEWIFI_INFO("[%s %d] release %u times\n", __func__, __LINE__, u8ReleaseCount);
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

    /* Create the event group */
    if (false == BleWifi_COM_EventCreate(&g_tAppCtrlEventGroup))
    {
        BLEWIFI_ERROR("APP: create event group fail \r\n");
    }

    /* Create ble-wifi task */
    task_def.name = "app_ctrl";
    task_def.stacksize = APP_CTRL_TASK_STACK_SIZE;
    task_def.tpriority = APP_CTRL_TASK_PRIORITY;
    task_def.pthread = App_Ctrl_Task;
    g_tAppCtrlTaskId = osThreadCreate(&task_def, NULL);
    if (g_tAppCtrlTaskId == NULL)
    {
        BLEWIFI_ERROR("APP: ctrl task create fail \r\n");
    }
    else
    {
        BLEWIFI_INFO("APP: ctrl task create successful \r\n");
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
}
