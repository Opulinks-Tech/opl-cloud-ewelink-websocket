/******************************************************************************
*  Copyright 2017 - 2018, Opulinks Technology Ltd.
*  ---------------------------------------------------------------------------
*  Statement:
*  ----------
*  This software is protected by Copyright and the information contained
*  herein is confidential. The software may not be copied and the information
*  contained herein may not be used or disclosed except with the written
*  permission of Opulinks Technology Ltd. (C) 2018
******************************************************************************/

#ifndef __APP_HTTP_OTA_H__
#define __APP_HTTP_OTA_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "iot_configuration.h"

#if (IOT_WIFI_OTA_FUNCTION_EN == 1)
#define LOG_I(tag, fmt, arg...)             printf("[%s]:" fmt, tag, ##arg)
#define LOG_E(tag, fmt, arg...)             printf("[%s]:" fmt, tag, ##arg)

#define IOT_OTA_HTTP_BUF_SIZE       (1024 + 1)
#define IOT_OTA_HTTP_URL_BUF_LEN    (256)

#define IOT_OTA_HTTP_QUEUE_SIZE         (20)
#define IOT_OTA_HTTP_STACK_SIZE         (1024)
#define IOT_OTA_HTTP_TASK_PRIORITY      (osPriorityNormal)

typedef enum {
    IOT_OTA_HTTP_SUCCESS,
    IOT_OTA_HTTP_FAILURE,
} iot_ota_http_type_id_e;

typedef enum {
    IOT_OTA_HTTP_MSG_TRIG = 0,

    IOT_OTA_HTTP_MSG_NUM
} iot_ota_http_msg_type_e;

typedef struct
{
    uint32_t u32Event;
    uint32_t u32Length;
    uint8_t u8aMessage[];
} xIotOtaHttpMessage_t;


void IoT_OTA_HTTP_TrigReq(uint8_t *data);
void IoT_OTA_HTTP_Init(void);

#endif  /* #if (IOT_WIFI_OTA_FUNCTION_EN == 1) */

#ifdef __cplusplus
}
#endif

#endif /* __APP_HTTP_OTA_H__ */
