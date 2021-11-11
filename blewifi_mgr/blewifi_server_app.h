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

#ifndef _BLEWIFI_SERVER_APP_H_
#define _BLEWIFI_SERVER_APP_H_

#include "ble.h"
#include "ble_msg.h"


// Minimum connection interval (units of 1.25ms) if automatic parameter update request is enabled
#define DEFAULT_DESIRED_MIN_CONN_INTERVAL     	100
// Maximum connection interval (units of 1.25ms) if automatic parameter update request is enabled
#define DEFAULT_DESIRED_MAX_CONN_INTERVAL     	200
// Maximum connection interval (units of 1.25ms) if automatic parameter update request is enabled
#define DEFAULT_DESIRED_SLAVE_LATENCY			0
// Maximum connection interval (units of 1.25ms) if automatic parameter update request is enabled
#define DEFAULT_DESIRED_SUPERVERSION_TIMEOUT   	500

#define LE_GATT_DATA_OUT_BUF_CNT                50
#define LE_GATT_DATA_OUT_BUF_BLOCK_SIZE         20
#define LE_GATT_DATA_OUT_BUF_SIZE				(LE_GATT_DATA_OUT_BUF_CNT * LE_GATT_DATA_OUT_BUF_BLOCK_SIZE)
#define LE_GATT_DATA_SENDING_BUF_MAX_SIZE       256

#define BLE_ADV_SCAN_BUF_SIZE                   31


// the BLE state is used in the Le stack task
enum
{
	APP_STATE_INIT = 0,
	APP_STATE_IDLE,
	APP_STATE_ADVERTISING,
	APP_STATE_ADVERTISING_TIME_CHANGE,
	APP_STATE_CONNECTED,

    APP_STATE_TOP
};

// the BLE message is used in the Le stack task
enum
{
    BW_APP_MSG_BASE = 0x4000,

    BW_APP_MSG_SEND_DATA,          // copy data to buffer
    BW_APP_MSG_SEND_TO_PEER,       // send data from buffer to peer
    BW_APP_MSG_SEND_TO_PEER_CFM,   // send data is confirmed
    BW_APP_MSG_BLE_EXEC_CMD,

    BW_APP_MSG_TOP
};


typedef struct
{
	UINT16		len;
	UINT8		*data;
} BLEWIFI_MESSAGE_T;

typedef struct
{
	UINT16			send_hdl;
    UINT8           u8WriteIdx;
    UINT8           u8ReadIdx;
    UINT8           u8CurrentPackageCnt[LE_GATT_DATA_OUT_BUF_CNT];
    UINT8           u8TotalPackageCnt[LE_GATT_DATA_OUT_BUF_CNT];
    UINT8           u8BufDataLen[LE_GATT_DATA_OUT_BUF_CNT];
	UINT8			buf[LE_GATT_DATA_OUT_BUF_SIZE];
} BLEWIFI_DATA_OUT_STORE_T;

typedef struct
{
	UINT16			len;
	UINT8			buf[BLE_ADV_SCAN_BUF_SIZE];
} BLE_ADV_SCAN_T;

typedef struct
{
	UINT16			interval_min;
	UINT16			interval_max;
} BLE_ADV_TIME_T;

typedef struct
{
	TASKPACK		task;
	UINT16			state;
	UINT16			conn_hdl;
	LE_BT_ADDR_T	bt_addr;

	UINT16			curr_mtu;
	BOOL			encrypted;
    BOOL			paired;

    UINT16			min_itvl;
    UINT16			max_itvl;
    UINT16			latency;
    UINT16			sv_tmo;

	BLEWIFI_DATA_OUT_STORE_T store;

	BLE_ADV_SCAN_T	adv_data;
    BLE_ADV_SCAN_T	scn_data;

} BLE_APP_DATA_T;

void BleWifi_Ble_TaskHandler(TASK task, MESSAGEID id, MESSAGE message);
BLE_APP_DATA_T* BleWifi_Ble_GetEntity(void);

void BleWifi_Ble_SendAppMsgToBle(UINT32 id, UINT16 len, void *data);

#endif
