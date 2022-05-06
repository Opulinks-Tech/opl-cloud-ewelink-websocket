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

#ifndef __IOT_CONFIGURATION_H__
#define __IOT_CONFIGURATION_H__

/*
WIFI OTA FLAG
*/
#define IOT_WIFI_OTA_FUNCTION_EN    (1)  // WIFI OTA Function Enable (1: enable / 0: disable)
#define IOT_WIFI_OTA_HTTP_URL       "http://192.168.0.100/ota.bin"

/*
IoT device
    1. if want to send data to server, set the Tx path to enable
    2. if want to receive data from server, set the Rx path to enable
*/
#define IOT_DEVICE_DATA_TX_EN               (1)     // 1: enable / 0: disable
#define IOT_DEVICE_DATA_RX_EN               (1)     // 1: enable / 0: disable
#define IOT_DEVICE_DATA_TASK_STACK_SIZE_TX  (2048)
#define IOT_DEVICE_DATA_TASK_STACK_SIZE_RX  (1792)
#define IOT_DEVICE_DATA_QUEUE_SIZE_TX       (48)
#define IOT_DEVICE_DATA_TASK_PRIORITY_TX    (osPriorityNormal)
#define IOT_DEVICE_DATA_TASK_PRIORITY_RX    (osPriorityNormal)

/*
Cloud HTTP
*/
#define CLOUD_HTTP_POST_ENHANCEMENT         (1)

/*
Iot TX wait rsp timeout
*/
#define IOT_TX_WAIT_TIMEOUT_TIME            (40000) //ms

#endif /* __IOT_CONFIGURATION_H__ */

