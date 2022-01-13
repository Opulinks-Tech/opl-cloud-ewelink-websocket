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

#ifndef __IOT_DATA_H__
#define __IOT_DATA_H__

#include <stdint.h>
#include <stdbool.h>
#include "iot_configuration.h"
#include "event_groups.h"
#include "iot_rb_data.h"

typedef enum iot_data_tx_msg_type
{
    IOT_DATA_TX_MSG_CLOUD_INIT = 0,
    IOT_DATA_TX_MSG_CLOUD_BINDING,
    IOT_DATA_TX_MSG_CLOUD_CONNECTION,
    IOT_DATA_TX_MSG_CLOUD_KEEP_ALIVE,
    IOT_DATA_TX_MSG_CLOUD_DISCONNECTION,
    IOT_DATA_TX_MSG_CLOUD_WAIT_RX_RSP_TIMEOUT,
    IOT_DATA_TX_MSG_CLOUD_POST,
    IOT_DATA_TX_MSG_CLOUD_POST_ACK_TIMEOUT,

    IOT_DATA_TX_MSG_NUM
} iot_data_tx_msg_type_e;

typedef enum iot_data_waiting_rsp_type
{
    IOT_DATA_WAITING_TYPE_NONE                 = 0,
    IOT_DATA_WAITING_TYPE_KEEPALIVE            = 1,
    IOT_DATA_WAITING_TYPE_DATA_POST            = 2,

    IOT_DATA_WAITING_TYPE_MAX
} iot_data_waiting_rps_type_e;


typedef struct
{
    uint32_t event;
    uint32_t length;
    uint8_t ucaMessage[];
} xIotDataMessage_t;

// event group bit (0 ~ 23 bits)
#define IOT_DATA_EVENT_BIT_CLOUD_CONNECTED      0x00000001U
#define IOT_DATA_EVENT_BIT_WAITING_RX_RSP       0x00000002U
#define IOT_DATA_EVENT_BIT_POST_FAIL_RECONNECT  0x00000004U  // trigger by post fail
#define IOT_DATA_EVENT_BIT_LAST_POST_RETRY      0x00000008U  // true: last post fail, then retry
#define IOT_DATA_EVENT_BIT_TIME_QUERY_WHEN_BOOT 0x00000010U

// the return value of data post
#define IOT_DATA_POST_RET_CONTINUE_DELETE       (0) // continue post + delete data
#define IOT_DATA_POST_RET_CONTINUE_KEEP         (1) // continue post + keep data
#define IOT_DATA_POST_RET_STOP_DELETE           (2) // stop post + delete data
#define IOT_DATA_POST_RET_STOP_KEEP             (3) // stop post + keep data

#define IOT_DATA_POST_RETRY_MAX                 (1) // the max of post retry times
#define IOT_DATA_LAST_DATA_POST_RETRY_MAX       (2) // the max of last data post retry times
#define IOT_DATA_KEEP_ALIVE_RETRY_MAX           (1) // the max of keep alive retry times
#define IOT_DATA_KEEP_ALIVE_FAIL_ROUND_MAX      (2) // if keep alive fail round > IOT_DATA_KEEP_ALIVE_FAIL_ROUND_MAX , cloud disconnect
#define IOT_DATA_QUEUE_LAST_DATA_CNT            (1) // the last data count of queue

#define IOT_DATA_POST_CHECK_CLOUD_CONNECTION    (1)

extern osTimerId g_iot_tx_wait_timeout_timer;
extern IoT_Ring_buffer_t g_stCloudRspQ;
extern IoT_Ring_buffer_t g_stKeepAliveQ;
extern IoT_Ring_buffer_t g_stIotRbData;
extern uint8_t g_u8WaitingRspType;
extern uint8_t g_u8PostRetry_KeepAlive_Cnt;
extern uint8_t g_u8PostRetry_IotRbData_Cnt;
extern uint8_t g_u8CloudRetryIntervalIdx;

extern osTimerId g_iot_tx_ack_post_timeout_timer;

extern uint8_t g_u8PostRetry_KeepAlive_Fail_Round;

typedef void (*T_IoT_Data_EvtHandler_Fp)(uint32_t evt_type, void *data, int len);
typedef struct
{
    uint32_t ulEventId;
    T_IoT_Data_EvtHandler_Fp fpFunc;
} T_IoT_Data_EvtHandlerTbl;

#if (IOT_DEVICE_DATA_TX_EN == 1)
int Iot_Data_TxTask_MsgSend(int msg_type, uint8_t *data, int data_len);
#endif  // end of #if (IOT_DEVICE_DATA_TX_EN == 1)

#if (IOT_DEVICE_DATA_TX_EN == 1) || (IOT_DEVICE_DATA_RX_EN == 1)
extern EventGroupHandle_t g_tIotDataEventGroup;

void Iot_Data_Init(void);
#endif

void Iot_Data_TxTaskEvtHandler_CloudConnectRetry(void);

#endif  // end of __IOT_DATA_H__
