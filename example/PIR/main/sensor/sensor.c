
/******************************************************************************
*  Copyright 2019 - 2019, Opulinks Technology Ltd.
*  ----------------------------------------------------------------------------
*  Statement:
*  ----------
*  This software is protected by Copyright and the information contained
*  herein is confidential. The software may not be copied and the information
*  contained herein may not be used or disclosed except with the written
*  permission of Opulinks Technology Ltd. (C) 2018
******************************************************************************/
#include "cmsis_os.h"
#include "sensor.h"
#include "hal_vic.h"
#include "blewifi_common.h"
#include "app_ctrl.h"

osTimerId g_tAppPIRTimerId;
#ifdef __IDR_EN__  //from build config
osTimerId g_tAppIDRTimerId;
#endif
#if(DAMAGE_EN == 1)
osTimerId g_tAppDoorDamageTimerId;
#endif
extern EventGroupHandle_t g_tAppCtrlEventGroup;
/*************************************************************************
* FUNCTION:
*   Sensor_PIR_GPIOCallBack
*
* DESCRIPTION:
*   GPIO call back function
*
* PARAMETERS
*   tGpioIdx  : [In] The GPIO pin
*
* RETURNS
*   none
*
*************************************************************************/
static void Sensor_PIR_GPIOCallBack(E_GpioIdx_t tGpioIdx)
{
    // printf("tGpioIdx = %d\r\n",tGpioIdx);
    /* Disable GPIO Interrupt */
    Hal_Vic_GpioIntEn(PIR_IO_PORT, 0);
    // send the result to the task of blewifi control.
    App_Ctrl_MsgSend(APP_CTRL_MSG_PIR_STATECHANGE, NULL, 0);
}
#ifdef __IDR_EN__  //from build config
static void Sensor_IDR_GPIOCallBack(E_GpioIdx_t tGpioIdx)
{
    // printf("tGpioIdx = %d\r\n",tGpioIdx);
    /* Disable GPIO Interrupt */
    Hal_Vic_GpioIntEn(IDR_IO_PORT, 0);
    // send the result to the task of blewifi control.
    App_Ctrl_MsgSend(APP_CTRL_MSG_IDR_CHANGE, NULL, 0);
}
#endif
#if(DAMAGE_EN == 1)
static void Sensor_Door_Damage_GPIOCallBack(E_GpioIdx_t tGpioIdx)
{
    // printf("tGpioIdx = %d\r\n",tGpioIdx);
    /* Disable GPIO Interrupt */
    Hal_Vic_GpioIntEn(DOOR_SENSOR_DAMAGE_IO_PORT, 0);
    // send the result to the task of blewifi control.
    App_Ctrl_MsgSend(APP_CTRL_MSG_DOOR_DAMAGE_CHANGE, NULL, 0);
}
#endif
/*************************************************************************
* FUNCTION:
*   Sensor_PIR_TimerCallBack
*
* DESCRIPTION:
*   Timer call back function
*
* PARAMETERS
*   argu  : [In] argument to the timer call back function.
*
* RETURNS
*   none
*
*************************************************************************/
static void Sensor_PIR_TimerCallBack(void const *argu)
{
    App_Ctrl_MsgSend(APP_CTRL_MSG_PIR_DEBOUNCETIMEOUT, NULL, 0);
}
#ifdef __IDR_EN__  //from build config
static void Sensor_IDR_TimerCallBack(void const *argu)
{
    App_Ctrl_MsgSend(APP_CTRL_MSG_IDR_TIMEOUT, NULL, 0);
}
#endif
#if(DAMAGE_EN == 1)
static void Sensor_DoorDamage_TimerCallBack(void const *argu)
{
    App_Ctrl_MsgSend(APP_CTRL_MSG_DOOR_DAMAGE_TIMEOUT, NULL, 0);
}
#endif
/*************************************************************************
* FUNCTION:
*   Sensor_TimerInit
*
* DESCRIPTION:
*   Timer initializaion setting
*
* PARAMETERS
*   none
*
* RETURNS
*   none
*
*************************************************************************/
void Sensor_TimerInit(void)
{
    osTimerDef_t tTimerDef;

    // create the timer
    tTimerDef.ptimer = Sensor_PIR_TimerCallBack;
    g_tAppPIRTimerId = osTimerCreate(&tTimerDef, osTimerOnce, NULL);
    if (g_tAppPIRTimerId == NULL)
    {
        printf("To create the timer for AppTimer is fail.\n");
    }

#ifdef __IDR_EN__  //from build config
    // create the timer
    tTimerDef.ptimer = Sensor_IDR_TimerCallBack;
    g_tAppIDRTimerId = osTimerCreate(&tTimerDef, osTimerOnce, NULL);
    if (g_tAppIDRTimerId == NULL)
    {
        printf("To create the timer for g_tAppIDRTimerId is fail.\n");
    }
#endif
#if(DAMAGE_EN == 1)
    // create the timer
    tTimerDef.ptimer = Sensor_DoorDamage_TimerCallBack;
    g_tAppDoorDamageTimerId = osTimerCreate(&tTimerDef, osTimerOnce, NULL);
    if (g_tAppDoorDamageTimerId == NULL)
    {
        printf("To create the timer for g_tAppDoorDamageTimerId is fail.\n");
    }
#endif
}

/*************************************************************************
* FUNCTION:
*   Sensor_GPIOInit
*
* DESCRIPTION:
*   Initialization the function of GPIO
*
* PARAMETERS
*   epinIdx  : [In] The GPIO pin
*
* RETURNS
*   none
*
*************************************************************************/
void Sensor_GPIOInit(E_GpioIdx_t epinIdx)
{
    unsigned int u32PinLevel = 0;

    // Get the status of GPIO (Low / High)
    u32PinLevel = Hal_Vic_GpioInput(epinIdx);

    // PIR IDR / LOW Level
    if(GPIO_LEVEL_LOW == u32PinLevel)
    {
        if(epinIdx == PIR_IO_PORT)
        {
            Hal_Vic_GpioCallBackFuncSet(epinIdx, Sensor_PIR_GPIOCallBack);
            Hal_Vic_GpioDirection(epinIdx, GPIO_INPUT);
            Hal_Vic_GpioIntTypeSel(epinIdx, INT_TYPE_LEVEL);
            Hal_Vic_GpioIntInv(epinIdx, (APP_CTRL_PIR_IO_BLOCK == GPIO_LEVEL_HIGH)?0:1);
            Hal_Vic_GpioIntMask(epinIdx, 0);
            Hal_Vic_GpioIntEn(epinIdx, 1);
            BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_PIR_DETECT, false);  //PIR empty
        }
#ifdef __IDR_EN__  //from build config
        else if(epinIdx == IDR_IO_PORT)
        {
            Hal_Vic_GpioCallBackFuncSet(epinIdx, Sensor_IDR_GPIOCallBack);
            Hal_Vic_GpioDirection(epinIdx, GPIO_INPUT);
            Hal_Vic_GpioIntTypeSel(epinIdx, INT_TYPE_LEVEL);
            Hal_Vic_GpioIntInv(epinIdx, (APP_CTRL_IDR_IO_LIGHT == GPIO_LEVEL_HIGH)?0:1);
            Hal_Vic_GpioIntMask(epinIdx, 0);
            Hal_Vic_GpioIntEn(epinIdx, 1);
            BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_IDR_DETECT, false);  //IDR light
        }
#endif
#if(DAMAGE_EN == 1)
        else if(epinIdx == DOOR_SENSOR_DAMAGE_IO_PORT)
        {
            Hal_Vic_GpioCallBackFuncSet(epinIdx, Sensor_Door_Damage_GPIOCallBack);
            Hal_Vic_GpioDirection(epinIdx, GPIO_INPUT);
            Hal_Vic_GpioIntTypeSel(epinIdx, INT_TYPE_LEVEL);
            Hal_Vic_GpioIntInv(epinIdx, 0);
            Hal_Vic_GpioIntMask(epinIdx, 0);
            Hal_Vic_GpioIntEn(epinIdx, 1);
        }
#endif
    }
    // PIR IDR / HIGH Level
    else
    {
        if(epinIdx == PIR_IO_PORT)
        {
            Hal_Vic_GpioCallBackFuncSet(epinIdx, Sensor_PIR_GPIOCallBack);
            Hal_Vic_GpioDirection(epinIdx, GPIO_INPUT);
            Hal_Vic_GpioIntTypeSel(epinIdx, INT_TYPE_LEVEL);
            Hal_Vic_GpioIntInv(epinIdx, (APP_CTRL_PIR_IO_BLOCK == GPIO_LEVEL_HIGH)?1:0);
            Hal_Vic_GpioIntMask(epinIdx, 0);
            Hal_Vic_GpioIntEn(epinIdx, 1);
            BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_PIR_DETECT, true); //PIR block
        }
#ifdef __IDR_EN__  //from build config
        else if(epinIdx == IDR_IO_PORT)
        {
            Hal_Vic_GpioCallBackFuncSet(epinIdx, Sensor_IDR_GPIOCallBack);
            Hal_Vic_GpioDirection(epinIdx, GPIO_INPUT);
            Hal_Vic_GpioIntTypeSel(epinIdx, INT_TYPE_LEVEL);
            Hal_Vic_GpioIntInv(epinIdx, (APP_CTRL_IDR_IO_LIGHT == GPIO_LEVEL_HIGH)?1:0);
            Hal_Vic_GpioIntMask(epinIdx, 0);
            Hal_Vic_GpioIntEn(epinIdx, 1);
            BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_IDR_DETECT, false);  //IDR dark
        }
#endif
#if(DAMAGE_EN == 1)
        else if(epinIdx == DOOR_SENSOR_DAMAGE_IO_PORT)
        {
            Hal_Vic_GpioCallBackFuncSet(epinIdx, Sensor_Door_Damage_GPIOCallBack);
            Hal_Vic_GpioDirection(epinIdx, GPIO_INPUT);
            Hal_Vic_GpioIntTypeSel(epinIdx, INT_TYPE_LEVEL);
            Hal_Vic_GpioIntInv(epinIdx, 1);
            Hal_Vic_GpioIntMask(epinIdx, 0);
            Hal_Vic_GpioIntEn(epinIdx, 1);
        }
#endif
    }
}

void Sensor_Init(void)
{
    Sensor_GPIOInit(PIR_IO_PORT);
#ifdef __IDR_EN__  //from build config
    Sensor_GPIOInit(IDR_IO_PORT);
#endif
#if(DAMAGE_EN == 1)
    Sensor_GPIOInit(DOOR_SENSOR_DAMAGE_IO_PORT);
#endif
    Sensor_TimerInit();
}
