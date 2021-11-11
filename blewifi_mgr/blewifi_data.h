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
 * @file blewifi_data.h
 * @author Michael Liao
 * @date 20 Feb 2018
 * @brief File includes the function declaration of blewifi task.
 *
 */

#ifndef __BLEWIFI_DATA_H__
#define __BLEWIFI_DATA_H__

#include <stdint.h>
#include <stdbool.h>
#include "blewifi_wifi_api.h"

#define DELIMITER_NUM (4)  // The total count of ',' in colud info data.
#define RSP_CLOUD_INFO_READ_PAYLOAD_SIZE (DEVICE_ID_LEN+API_KEY_LEN+CHIP_ID_LEN+CHIP_ID_LEN+MODEL_ID_LEN + DELIMITER_NUM + 1)
#define MAX_AUTH_DATA_SIZE (64) // CBC(UUID(36)) = 48 , BASE64(CBC(UUID(36))) = 64
#define MAX_AUTH_TOKEN_DATA_SIZE (108) // CBC(UUID(36) + "_"(1) + UUID(36)) = 80 , BASE(80) = 108
#define UUID_SIZE (36)  // 8-4-4-4-12
#define SECRETKEY_LEN (16)
#define IV_SIZE (16)
#define UUID_ENC_SIZE (48)  // CBC(UUID(36)) = 48
#define ENC_UUID_TO_BASE64_SIZE (64)  //base64 max size ((4 * n / 3) + 3) & ~3
#define MAX_RSP_BASE64_API_KEY_LEN (108) //BASE(80) = 108
#define MAX_RSP_BASE64_CHIP_ID_LEN (44) //BASE(32) = 44

#define BLEWIFI_WIFI_MAX_REC_PASSWORD_SIZE (108) // WIFI_LENGTH_PASSPHRASE = 64  CBC(64)= 80  BASE64(80) = 108

#define BLEWIFI_WIFI_CONNECTED_DONE     0
#define BLEWIFI_WIFI_CONNECTED_FAIL     1
#define BLEWIFI_WIFI_PASSWORD_FAIL      2
#define BLEWIFI_WIFI_AP_NOT_FOUND       3
#define BLEWIFI_WIFI_CONNECT_TIMEOUT    4

typedef struct
{
	uint16_t proj_id;
	uint16_t chip_id;
	uint16_t fw_id;
	uint32_t chksum;
	uint32_t curr_chksum;

	uint32_t total;
	uint32_t curr;
	uint16_t pkt_idx;
	uint16_t rx_pkt;
	uint16_t flag;

	uint8_t  buf[300];
	uint16_t idx;
} blewifi_ota_t;

enum
{
	BLEWIFI_OTA_SUCCESS,
	BLEWIFI_OTA_ERR_NOT_ACTIVE,
	BLEWIFI_OTA_ERR_HW_FAILURE,
	BLEWIFI_OTA_ERR_IN_PROGRESS,
	BLEWIFI_OTA_ERR_INVALID_LEN,
	BLEWIFI_OTA_ERR_CHECKSUM,
	BLEWIFI_OTA_ERR_MEM_CAPACITY_EXCEED,


};

typedef enum {

    BLEWIFI_REQ_AUTH                            = 0x0,  // auth
    BLEWIFI_RSP_AUTH                            = 0x1,  //
    BLEWIFI_REQ_AUTH_TOKEN                      = 0x2,  // Wifi scan
    BLEWIFI_RSP_AUTH_TOKEN                      = 0x3,  // Wifi scan
    BLEWIFI_REQ_SCAN                            = 0x4,  // Wifi scan
    BLEWIFI_RSP_SCAN_REPORT                     = 0x5,
    BLEWIFI_RSP_SCAN_END                        = 0x6,
    BLEWIFI_REQ_CONNECT                         = 0x7,  // Wifi connect
    BLEWIFI_RSP_CONNECT                         = 0x8,
    BLEWIFI_REQ_APP_DEVICE_INFO                 = 0x9,  //for CKS
    BLEWIFI_RSP_APP_DEVICE_INFO                 = 0xA,  //for CKS
    BLEWIFI_REQ_APP_HOST_INFO                   = 0xB,  //for CKS
    BLEWIFI_RSP_APP_HOST_INFO                   = 0xC,  //for CKS
    BLEWIFI_REQ_MANUAL_CONNECT_AP               = 0xD,  // Wifi connect AP by manual
    BLEWIFI_IND_IP_STATUS_NOTIFY                = 0xE,  // Wifi notify AP status


    //BLEWIFI_REQ_SCAN                          = 0x3000,          // Wifi scan
    //BLEWIFI_REQ_CONNECT                       = 0x3001,          // Wifi connect
    BLEWIFI_REQ_DISCONNECT                      = 0x3002,          // Wifi disconnect
    BLEWIFI_REQ_RECONNECT                       = 0x3003,          // Wifi reconnect
    BLEWIFI_REQ_READ_DEVICE_INFO                = 0x3004,          // Wifi read device information
    BLEWIFI_REQ_WRITE_DEVICE_INFO               = 0x3005,          // Wifi write device information
    BLEWIFI_REQ_WIFI_STATUS                     = 0x3006,          // Wifi read AP status
    BLEWIFI_REQ_RESET                           = 0x3007,          // Wifi reset AP
	//BLEWIFI_REQ_MANUAL_CONNECT_AP               = 0x8,          // Wifi connect AP by manual

    BLEWIFI_REQ_OTA_VERSION                     = 0x100,        // Ble OTA
    BLEWIFI_REQ_OTA_UPGRADE                     = 0x101,        // Ble OTA
    BLEWIFI_REQ_OTA_RAW                         = 0x102,        // Ble OTA
    BLEWIFI_REQ_OTA_END                         = 0x103,        // Ble OTA

    BLEWIFI_REQ_MP_START                        = 0x400,
    BLEWIFI_REQ_MP_CAL_VBAT                     = 0x401,
    BLEWIFI_REQ_MP_CAL_IO_VOLTAGE               = 0x402,
    BLEWIFI_REQ_MP_CAL_TMPR                     = 0x403,
    BLEWIFI_REQ_MP_SYS_MODE_WRITE               = 0x404,
    BLEWIFI_REQ_MP_SYS_MODE_READ                = 0x405,

    BLEWIFI_REQ_ENG_START                       = 0x600,
    BLEWIFI_REQ_ENG_SYS_RESET                   = 0x601,
    BLEWIFI_REQ_ENG_WIFI_MAC_WRITE              = 0x602,
    BLEWIFI_REQ_ENG_WIFI_MAC_READ               = 0x603,
    BLEWIFI_REQ_ENG_BLE_MAC_WRITE               = 0x604,
    BLEWIFI_REQ_ENG_BLE_MAC_READ                = 0x605,
    BLEWIFI_REQ_ENG_BLE_CMD                     = 0x606,
    BLEWIFI_REQ_ENG_MAC_SRC_WRITE               = 0x607,
    BLEWIFI_REQ_ENG_MAC_SRC_READ                = 0x608,
    BLEWIFI_REQ_ENG_TMPR_CAL_DATA_WRITE         = 0x609,
    BLEWIFI_REQ_ENG_TMPR_CAL_DATA_READ          = 0x60A,
    BLEWIFI_REQ_ENG_VDD_VOUT_VOLTAGE_READ       = 0x60B,
    //4 cmd ID unused
    BLEWIFI_REQ_ENG_BLE_CLOUD_INFO_WRITE        = 0x610,
    BLEWIFI_REQ_ENG_BLE_CLOUD_INFO_READ         = 0x611,

    BLEWIFI_REQ_APP_START                       = 0x800,
    BLEWIFI_REQ_APP_USER_DEF_TMPR_OFFSET_WRITE  = 0x801,
    BLEWIFI_REQ_APP_USER_DEF_TMPR_OFFSET_READ   = 0x802,

//    BLEWIFI_RSP_SCAN_REPORT                     = 0x1000,
//    BLEWIFI_RSP_SCAN_END                        = 0x1001,
//    BLEWIFI_RSP_CONNECT                         = 0x1002,
    BLEWIFI_RSP_DISCONNECT                      = 0x1003,
    BLEWIFI_RSP_RECONNECT                       = 0x1004,
    BLEWIFI_RSP_READ_DEVICE_INFO                = 0x1005,
    BLEWIFI_RSP_WRITE_DEVICE_INFO               = 0x1006,
    BLEWIFI_RSP_WIFI_STATUS                     = 0x1007,
    BLEWIFI_RSP_RESET                           = 0x1008,

    BLEWIFI_RSP_OTA_VERSION                     = 0x1100,
    BLEWIFI_RSP_OTA_UPGRADE                     = 0x1101,
    BLEWIFI_RSP_OTA_RAW                         = 0x1102,
    BLEWIFI_RSP_OTA_END                         = 0x1103,

    BLEWIFI_RSP_MP_START                        = 0x1400,
    BLEWIFI_RSP_MP_CAL_VBAT                     = 0x1401,
    BLEWIFI_RSP_MP_CAL_IO_VOLTAGE               = 0x1402,
    BLEWIFI_RSP_MP_CAL_TMPR                     = 0x1403,
    BLEWIFI_RSP_MP_SYS_MODE_WRITE               = 0x1404,
    BLEWIFI_RSP_MP_SYS_MODE_READ                = 0x1405,

    BLEWIFI_RSP_ENG_START                       = 0x1600,
    BLEWIFI_RSP_ENG_SYS_RESET                   = 0x1601,
    BLEWIFI_RSP_ENG_WIFI_MAC_WRITE              = 0x1602,
    BLEWIFI_RSP_ENG_WIFI_MAC_READ               = 0x1603,
    BLEWIFI_RSP_ENG_BLE_MAC_WRITE               = 0x1604,
    BLEWIFI_RSP_ENG_BLE_MAC_READ                = 0x1605,
    BLEWIFI_RSP_ENG_BLE_CMD                     = 0x1606,
    BLEWIFI_RSP_ENG_MAC_SRC_WRITE               = 0x1607,
    BLEWIFI_RSP_ENG_MAC_SRC_READ                = 0x1608,
    BLEWIFI_RSP_ENG_TMPR_CAL_DATA_WRITE         = 0x1609,
    BLEWIFI_RSP_ENG_TMPR_CAL_DATA_READ          = 0x160A,
    BLEWIFI_RSP_ENG_VDD_VOUT_VOLTAGE_READ       = 0x160B,
    //4 cmd ID unused
    BLEWIFI_RSP_ENG_BLE_CLOUD_INFO_WRITE        = 0x1610,
    BLEWIFI_RSP_ENG_BLE_CLOUD_INFO_READ         = 0x1611,

    BLEWIFI_RSP_APP_START                       = 0x1800,
    BLEWIFI_RSP_APP_USER_DEF_TMPR_OFFSET_WRITE  = 0x1801,
    BLEWIFI_RSP_APP_USER_DEF_TMPR_OFFSET_READ   = 0x1802,

//    BLEWIFI_IND_IP_STATUS_NOTIFY                = 0x2000,  // Wifi notify AP status

    BLEWIFI_TYPE_END                            = 0xFFFF
}blewifi_type_id_e;

/* BLEWIF protocol */
typedef struct blewifi_hdr_tag
{
    uint16_t type;
    uint16_t data_len;
    uint8_t  data[]; //variable size
}blewifi_hdr_t;

typedef void (*T_BleWifi_Ble_ProtocolHandler_Fp)(uint16_t type, uint8_t *data, int len);
typedef struct
{
    uint32_t ulEventId;
    T_BleWifi_Ble_ProtocolHandler_Fp fpFunc;
} T_BleWifi_Ble_ProtocolHandlerTbl;

extern wifi_conn_config_t g_stStartConnConfig;

void BleWifi_Ble_DataRecvHandler(uint8_t *data, int len);
void BleWifi_Ble_DataSendEncap(uint16_t type_id, uint8_t *data, int total_data_len);
void BleWifi_Ble_SendResponse(uint16_t type_id, uint8_t status);

int BleWifi_Wifi_UpdateScanInfoToAutoConnList(uint8_t *pu8IsUpdate);
int BleWifi_Wifi_SendScanReport(void);
void BleWifi_Wifi_SendStatusInfo(uint16_t u16Type);

int32_t BleWifi_Ble_InitAdvData(uint8_t *pu8Data , uint8_t *pu8Len);
int32_t BleWifi_Ble_InitScanData(uint8_t *pu8Data , uint8_t *pu8Len);


int BleWifi_CBC_encrypt(void *src , int len , unsigned char *iv , const unsigned char *key , void *out);
int BleWifi_CBC_decrypt(void *src, int len , unsigned char *iv , const unsigned char *key, void *out);
int BleWifi_UUID_Generate(unsigned char *ucUuid , uint16_t u16BufSize);

#endif /* __BLEWIFI_DATA_H__ */

