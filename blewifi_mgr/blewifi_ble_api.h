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

#ifndef __BLEWIFI_BLE_API_H__
#define __BLEWIFI_BLE_API_H__

#include <stdint.h>
#include "cmsis_os.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum blewifi_ble_mode
{
    BLEWIFI_BLE_NORMAL_MODE                = 0x0,
    BLEWIFI_BLE_SMART_SLEEP_MODE           = 0x1
} blewifi_ble_mode_e;

extern osSemaphoreId g_tBleInternalSemaphoreId;

uint8_t BleWifi_Ble_Init(void);
uint8_t BleWifi_Ble_Start(uint16_t u16Interval);
uint8_t BleWifi_Ble_Stop(void);
uint8_t	BleWifi_Ble_AdvertisingIntervalChange(uint16_t u16Interval);
uint8_t	BleWifi_Ble_AdvertisingDataChange(uint8_t *pu8Data, uint32_t u32DataLen);
uint8_t BleWifi_Ble_ScanDataChange(uint8_t *pu8Data, uint32_t u32DataLen);
uint8_t BleWifi_Ble_MacAddrWrite(uint8_t *data);
uint8_t BleWifi_Ble_MacAddrRead(uint8_t *data);


#ifdef __cplusplus
}
#endif

#endif  // end of __BLEWIFI_BLE_API_H__
