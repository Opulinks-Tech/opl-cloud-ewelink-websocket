/******************************************************************************
*  Copyright 2017 - 2021, Opulinks Technology Ltd.
*  ----------------------------------------------------------------------------
*  Statement:
*  ----------
*  This software is protected by Copyright and the information contained
*  herein is confidential. The software may not be copied and the information
*  contained herein may not be used or disclosed except with the written
*  permission of Opulinks Technology Ltd. (C) 2021
******************************************************************************/

#include "blewifi_handle.h"
#include "app_ctrl.h"

void BleWifi_Wifi_Init_Done_CB(void)
{
    printf("BleWifi_Wifi_Init_Done_CB\r\n");
    App_Ctrl_MsgSend(APP_CTRL_MSG_WIFI_INIT_COMPLETE, NULL, 0);
}

void BleWifi_Wifi_Scan_Done_CB(void)
{
    printf("BleWifi_Wifi_Scan_Done_CB\r\n");
    App_Ctrl_MsgSend(APP_CTRL_MSG_WIFI_SCAN_DONE, NULL, 0);
}

void BleWifi_Wifi_Connected_CB(void)
{
    printf("BleWifi_Wifi_Connected_CB\r\n");
    App_Ctrl_MsgSend(APP_CTRL_MSG_WIFI_CONNECTION, NULL, 0);
}

void BleWifi_Wifi_Disconnected_CB(uint8_t reason)
{
    printf("BleWifi_Wifi_Disconnected_CB\r\n");
    App_Ctrl_MsgSend(APP_CTRL_MSG_WIFI_DISCONNECTION, (void *)&reason, sizeof(reason));
}

void BleWifi_Wifi_Req_Disconnected_CB(void)
{
    BleWifi_Wifi_Disconnected_CB(0);
}

void BleWifi_Wifi_Stop_CB(void)
{
    printf("BleWifi_Wifi_Stop_CB\r\n");
    App_Ctrl_MsgSend(APP_CTRL_MSG_WIFI_STOP_COMPLETE, NULL , 0);
}

void BleWifi_Ble_Init_Done_CB(void)
{
    printf("BleWifi_Ble_Init_Done_CB\r\n");
    App_Ctrl_MsgSend(APP_CTRL_MSG_BLE_INIT_COMPLETE, NULL, 0);
}

void BleWifi_Ble_Start_Cfm_CB(void)
{
    printf("BleWifi_Ble_Start_Cfm_CB\r\n");
    App_Ctrl_MsgSend(APP_CTRL_MSG_BLE_START_COMPLETE, NULL, 0);
}

void BleWifi_Ble_Stop_Cfm_CB(void)
{
    printf("BleWifi_Ble_Stop_Cfm_CB\r\n");
    App_Ctrl_MsgSend(APP_CTRL_MSG_BLE_STOP_COMPLETE, NULL, 0);
}

void BleWifi_Ble_Connected_CB(void)
{
    printf("BleWifi_Ble_Connected_CB\r\n");
    App_Ctrl_MsgSend(APP_CTRL_MSG_BLE_CONNECTION, NULL, 0);
}

void BleWifi_Ble_Disconnected_CB(uint32_t reason)
{
    printf("BleWifi_Ble_Disconnected_CB\r\n");
    App_Ctrl_MsgSend(APP_CTRL_MSG_BLE_DISCONNECTION, (void *)&reason, sizeof(reason));
}

