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

#ifndef __BLEWIFI_HANDLE_H__
#define __BLEWIFI_HANDLE_H__

#include <stdint.h>
#include <stdbool.h>

void BleWifi_Wifi_Init_Done_CB(void);
void BleWifi_Wifi_Scan_Done_CB(void);
void BleWifi_Wifi_Connected_CB(void);
void BleWifi_Wifi_Disconnected_CB(uint8_t reason);
void BleWifi_Wifi_Req_Disconnected_CB(void);
void BleWifi_Wifi_Stop_CB(void);
void BleWifi_Ble_Init_Done_CB(void);
void BleWifi_Ble_Start_Cfm_CB(void);
void BleWifi_Ble_Stop_Cfm_CB(void);
void BleWifi_Ble_Connected_CB(void);
void BleWifi_Ble_Disconnected_CB(uint32_t reason);

#endif // __BLEWIFI_HANDLE_H__
