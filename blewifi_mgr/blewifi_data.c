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
 * @file blewifi_data.c
 * @author Michael Liao
 * @date 20 Feb 2018
 * @brief File creates the wifible_data task architecture.
 *
 */

#include "sys_os_config.h"
#include "app_ctrl.h"
#include "blewifi_data.h"
#include "blewifi_server_app.h"
#include "blewifi_wifi_api.h"
#include "blewifi_ble_api.h"
#include "wifi_api.h"
#include "lwip/netif.h"
#include "mw_ota.h"
#include "hal_auxadc_patch.h"
#include "hal_system.h"
#include "mw_fim_default_group03.h"
#include "mw_fim_default_group03_patch.h"
#include "at_cmd_common.h"
#include "sys_common_types.h"
#include "sys_common_api.h"
#include "blewifi_wifi_FSM.h"
#include "ble_gap_if.h"
#include "mbedtls/aes.h"
#include "mbedtls/base64.h"
#include "mbedtls/md5.h"
#include "mw_fim_default_group12_project.h"
#include "app_ctrl.h"
//#include "sensor_data.h"
#include "iot_data.h"
#include "iot_rb_data.h"

blewifi_ota_t *gTheOta = NULL;

#define HI_UINT16(a) (((a) >> 8) & 0xFF)
#define LO_UINT16(a) ((a) & 0xFF)

#define AES_BLOCK_SIZE (16) //for CBC

uint8_t g_wifi_disconnectedDoneForAppDoWIFIScan = 1;

typedef struct {
    uint16_t total_len;
    uint16_t remain;
    uint16_t offset;
    uint8_t *aggr_buf;
} blewifi_rx_packet_t;

blewifi_rx_packet_t g_rx_packet = {0};

extern EventGroupHandle_t g_tAppCtrlEventGroup;
extern T_MwFim_GP12_HttpPostContent g_tHttpPostContent;
extern T_MwFim_GP12_HttpHostInfo g_tHostInfo;

extern IoT_Ring_buffer_t g_stIotRbData;

extern void App_Sensor_Post_To_Cloud(uint8_t ubType);

unsigned char g_ucSecretKey[SECRETKEY_LEN + 1] = {0};
unsigned char g_ucAppCode[UUID_SIZE + 1] = {0};
unsigned char g_ucDeviceCode[UUID_SIZE + 1] = {0};

wifi_conn_config_t g_stStartConnConfig = {0};

static void BleWifi_Ble_ProtocolHandler_Auth(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_AuthToken(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_Scan(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_Connect(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_Disconnect(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_Reconnect(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_ReadDeviceInfo(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_WriteDeviceInfo(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_WifiStatus(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_Reset(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_Manually_Connect_AP(uint16_t type, uint8_t *data, int len);

#if (BLE_OTA_FUNCTION_EN == 1)
static void BleWifi_Ble_ProtocolHandler_OtaVersion(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_OtaUpgrade(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_OtaRaw(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_OtaEnd(uint16_t type, uint8_t *data, int len);
#endif

static void BleWifi_Ble_ProtocolHandler_MpCalVbat(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_MpCalIoVoltage(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_MpCalTmpr(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_MpSysModeWrite(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_MpSysModeRead(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_EngSysReset(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_EngWifiMacWrite(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_EngWifiMacRead(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_EngBleMacWrite(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_EngBleMacRead(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_EngBleCmd(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_EngMacSrcWrite(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_EngMacSrcRead(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_AppDeviceInfo(uint16_t type, uint8_t *data, int len);
static void BleWifi_Ble_ProtocolHandler_AppWifiConnection(uint16_t type, uint8_t *data, int len);

static T_BleWifi_Ble_ProtocolHandlerTbl g_tBleProtocolHandlerTbl[] =
{
    {BLEWIFI_REQ_AUTH,                      BleWifi_Ble_ProtocolHandler_Auth},
    {BLEWIFI_REQ_AUTH_TOKEN,                BleWifi_Ble_ProtocolHandler_AuthToken},
    {BLEWIFI_REQ_SCAN,                      BleWifi_Ble_ProtocolHandler_Scan},
    {BLEWIFI_REQ_CONNECT,                   BleWifi_Ble_ProtocolHandler_Connect},
    {BLEWIFI_REQ_APP_DEVICE_INFO,           BleWifi_Ble_ProtocolHandler_AppDeviceInfo},
    {BLEWIFI_REQ_APP_HOST_INFO,             BleWifi_Ble_ProtocolHandler_AppWifiConnection},
    {BLEWIFI_REQ_MANUAL_CONNECT_AP,         BleWifi_Ble_ProtocolHandler_Manually_Connect_AP},


//    {BLEWIFI_REQ_SCAN,                      BleWifi_Ble_ProtocolHandler_Scan},
//    {BLEWIFI_REQ_CONNECT,                   BleWifi_Ble_ProtocolHandler_Connect},
    {BLEWIFI_REQ_DISCONNECT,                BleWifi_Ble_ProtocolHandler_Disconnect},
    {BLEWIFI_REQ_RECONNECT,                 BleWifi_Ble_ProtocolHandler_Reconnect},
    {BLEWIFI_REQ_READ_DEVICE_INFO,          BleWifi_Ble_ProtocolHandler_ReadDeviceInfo},
    {BLEWIFI_REQ_WRITE_DEVICE_INFO,         BleWifi_Ble_ProtocolHandler_WriteDeviceInfo},
    {BLEWIFI_REQ_WIFI_STATUS,               BleWifi_Ble_ProtocolHandler_WifiStatus},
    {BLEWIFI_REQ_RESET,                     BleWifi_Ble_ProtocolHandler_Reset},

#if (BLE_OTA_FUNCTION_EN == 1)
    {BLEWIFI_REQ_OTA_VERSION,               BleWifi_Ble_ProtocolHandler_OtaVersion},
    {BLEWIFI_REQ_OTA_UPGRADE,               BleWifi_Ble_ProtocolHandler_OtaUpgrade},
    {BLEWIFI_REQ_OTA_RAW,                   BleWifi_Ble_ProtocolHandler_OtaRaw},
    {BLEWIFI_REQ_OTA_END,                   BleWifi_Ble_ProtocolHandler_OtaEnd},
#endif

    {BLEWIFI_REQ_MP_CAL_VBAT,               BleWifi_Ble_ProtocolHandler_MpCalVbat},
    {BLEWIFI_REQ_MP_CAL_IO_VOLTAGE,         BleWifi_Ble_ProtocolHandler_MpCalIoVoltage},
    {BLEWIFI_REQ_MP_CAL_TMPR,               BleWifi_Ble_ProtocolHandler_MpCalTmpr},
    {BLEWIFI_REQ_MP_SYS_MODE_WRITE,         BleWifi_Ble_ProtocolHandler_MpSysModeWrite},
    {BLEWIFI_REQ_MP_SYS_MODE_READ,          BleWifi_Ble_ProtocolHandler_MpSysModeRead},

    {BLEWIFI_REQ_ENG_SYS_RESET,             BleWifi_Ble_ProtocolHandler_EngSysReset},
    {BLEWIFI_REQ_ENG_WIFI_MAC_WRITE,        BleWifi_Ble_ProtocolHandler_EngWifiMacWrite},
    {BLEWIFI_REQ_ENG_WIFI_MAC_READ,         BleWifi_Ble_ProtocolHandler_EngWifiMacRead},
    {BLEWIFI_REQ_ENG_BLE_MAC_WRITE,         BleWifi_Ble_ProtocolHandler_EngBleMacWrite},
    {BLEWIFI_REQ_ENG_BLE_MAC_READ,          BleWifi_Ble_ProtocolHandler_EngBleMacRead},
    {BLEWIFI_REQ_ENG_BLE_CMD,               BleWifi_Ble_ProtocolHandler_EngBleCmd},
    {BLEWIFI_REQ_ENG_MAC_SRC_WRITE,         BleWifi_Ble_ProtocolHandler_EngMacSrcWrite},
    {BLEWIFI_REQ_ENG_MAC_SRC_READ,          BleWifi_Ble_ProtocolHandler_EngMacSrcRead},

    {0xFFFFFFFF,                            NULL}
};

#if (1 == FLITER_STRONG_AP_EN)
static void _BleWifi_Wifi_FilterStrongRssiAP(blewifi_scan_info_t *pstScanInfo ,uint16_t u16apCount)
{
    uint8_t i = 0 , j = 0;

    for(i = 0 ; i < u16apCount ; i++)
    {
        for(j = 0 ; j < i ; j++)
        {
             //check whether ignore alreay
            if(false == pstScanInfo[j].u8IgnoreReport)
            {
                //compare the same ssid
                if((pstScanInfo[i].ssid_length != 0) && memcmp(pstScanInfo[j].ssid , pstScanInfo[i].ssid , WIFI_MAX_LENGTH_OF_SSID) == 0)
                {
                    //let
                    if(pstScanInfo[j].rssi < pstScanInfo[i].rssi)
                    {
                        pstScanInfo[j].u8IgnoreReport = true;
                    }
                    else
                    {
                        pstScanInfo[i].u8IgnoreReport = true;
                        break; //this AP sets ignore , don't need compare others.
                    }
                }
            }
        }
    }
}
#endif

int BleWifi_CBC_encrypt(void *src , int len , unsigned char *iv , const unsigned char *key , void *out)
{
    int len1 = len & 0xfffffff0;
    int len2 = len1 + 16;
    int pad = len2 - len;
    uint32_t u32Keybits = 128;
    uint16_t i = 0;
    uint16_t u16BlockNum = 0;
    int ret = 0;
    void * pTempSrcPos = src;
    void * pTempOutPos = out;

    if((pTempSrcPos == NULL) || (pTempOutPos == NULL))
    {
        return -1;
    }
    mbedtls_aes_context aes_ctx = {0};

    mbedtls_aes_init(&aes_ctx);
    mbedtls_aes_setkey_enc(&aes_ctx , key , u32Keybits);

    if (len1) //execute encrypt for n-1 block
    {
        u16BlockNum = len >> 4 ;
        for (i = 0; i < u16BlockNum ; ++i)
        {
            ret = mbedtls_aes_crypt_cbc(&aes_ctx , MBEDTLS_AES_ENCRYPT , AES_BLOCK_SIZE, iv , (unsigned char *)pTempSrcPos , (unsigned char *)pTempOutPos);
            pTempSrcPos = ((char*)pTempSrcPos)+16;
            pTempOutPos = ((char*)pTempOutPos)+16;
        }
    }
    if (pad) //padding & execute encrypt for last block
    {
        char buf[16];
        memcpy((char *)buf, (char *)src + len1, len - len1);
        memset((char *)buf + len - len1, pad, pad);
        ret = mbedtls_aes_crypt_cbc(&aes_ctx , MBEDTLS_AES_ENCRYPT , AES_BLOCK_SIZE, iv , (unsigned char *)buf , (unsigned char *)out + len1);
    }
    mbedtls_aes_free(&aes_ctx);

    if(ret != 0)
        return -1;
    else
        return 0;
}

int BleWifi_CBC_decrypt(void *src, int len , unsigned char *iv , const unsigned char *key, void *out)
{
    mbedtls_aes_context aes_ctx = {0};
    int n = len >> 4;
    char *out_c = NULL;
    int offset = 0;
    int ret = 0;
    uint32_t u32Keybits = 128;
    uint16_t u16BlockNum = 0;
    char pad = 0;
    void * pTempSrcPos = src;
    void * pTempOutPos = out;
    uint16_t i = 0;

    if((pTempSrcPos == NULL) || (pTempOutPos == NULL))
    {
        return -1;
    }

    mbedtls_aes_init(&aes_ctx);
    mbedtls_aes_setkey_dec(&aes_ctx , key , u32Keybits);

    //decrypt n-1 block
    u16BlockNum = n - 1;
    if (n > 1)
    {
        for (i = 0; i < u16BlockNum ; ++i)
        {
            ret = mbedtls_aes_crypt_cbc(&aes_ctx , MBEDTLS_AES_DECRYPT , AES_BLOCK_SIZE, iv , (unsigned char *)pTempSrcPos , (unsigned char *)pTempOutPos);
            pTempSrcPos = ((char*)pTempSrcPos)+16;
            pTempOutPos = ((char*)pTempOutPos)+16;
        }

    }

    out_c = (char *)out;
    offset = n > 0 ? ((n - 1) << 4) : 0;
    out_c[offset] = 0;

    //decrypt last block
    ret = mbedtls_aes_crypt_cbc(&aes_ctx , MBEDTLS_AES_DECRYPT , AES_BLOCK_SIZE, iv , (unsigned char *)src + offset , (unsigned char *)out_c + offset);

    //paddind data set 0
    pad = out_c[len - 1];
    out_c[len - pad] = 0;

    mbedtls_aes_free(&aes_ctx);

    if(ret != 0)
        return -1;
    else
        return 0;
}

int BleWifi_UUID_Generate(unsigned char *ucUuid , uint16_t u16BufSize)
{
    uint8_t i = 0;
    uint8_t u8Random = 0;
    if(u16BufSize < 36)
    {
        return false;
    }
    srand(osKernelSysTick());
    for(i = 0; i<36 ; i++)
    {
        if((i == 8) || (i == 13) || (i == 18) || (i == 23))
        {
            ucUuid[i] = '-';
        }
        else
        {
            u8Random = rand()%16;
            if(u8Random < 10)
            {
                ucUuid[i] = u8Random + '0';
            }
            else
            {
                ucUuid[i] = (u8Random - 10) + 'a';
            }
        }
    }
    return true;
}



static void _BleWifi_Wifi_SendDeviceInfo(blewifi_device_info_t *dev_info)
{
    uint8_t *pu8Data;
    int sDataLen;
    uint8_t *pu8Pos;

    pu8Pos = pu8Data = malloc(sizeof(blewifi_scan_info_t));
    if (pu8Data == NULL) {
        printf("malloc error\r\n");
        return;
    }

    memcpy(pu8Data, dev_info->device_id, WIFI_MAC_ADDRESS_LENGTH);
    pu8Pos += 6;

    if (dev_info->name_len > BLEWIFI_MANUFACTURER_NAME_LEN)
        dev_info->name_len = BLEWIFI_MANUFACTURER_NAME_LEN;

    *pu8Pos++ = dev_info->name_len;
    memcpy(pu8Pos, dev_info->manufacturer_name, dev_info->name_len);
    pu8Pos += dev_info->name_len;
    sDataLen = (pu8Pos - pu8Data);

    BLEWIFI_DUMP("device info data", pu8Data, sDataLen);

    /* create device info data packet */
    BleWifi_Ble_DataSendEncap(BLEWIFI_RSP_READ_DEVICE_INFO, pu8Data, sDataLen);

    free(pu8Data);
}


#if (BLE_OTA_FUNCTION_EN == 1)
static void BleWifi_OtaSendVersionRsp(uint8_t status, uint16_t pid, uint16_t cid, uint16_t fid)
{
	uint8_t data[7];
	uint8_t *p = (uint8_t *)data;

	*p++ = status;
	*p++ = LO_UINT16(pid);
	*p++ = HI_UINT16(pid);
	*p++ = LO_UINT16(cid);
	*p++ = HI_UINT16(cid);
	*p++ = LO_UINT16(fid);
	*p++ = HI_UINT16(fid);

	BleWifi_Ble_DataSendEncap(BLEWIFI_RSP_OTA_VERSION, data, 7);
}

static void BleWifi_OtaSendUpgradeRsp(uint8_t status)
{
	BleWifi_Ble_DataSendEncap(BLEWIFI_RSP_OTA_UPGRADE, &status, 1);
}

static void BleWifi_OtaSendEndRsp(uint8_t status, uint8_t stop)
{
	BleWifi_Ble_DataSendEncap(BLEWIFI_RSP_OTA_END, &status, 1);

    if (stop)
    {
        if (gTheOta)
        {
            if (status != BLEWIFI_OTA_SUCCESS)
                MwOta_DataGiveUp();
            free(gTheOta);
            gTheOta = NULL;

            if (status != BLEWIFI_OTA_SUCCESS)
                App_Ctrl_MsgSend(APP_CTRL_MSG_OTHER_OTA_OFF_FAIL, NULL, 0);
            else
                App_Ctrl_MsgSend(APP_CTRL_MSG_OTHER_OTA_OFF, NULL, 0);
        }
    }
}

static void BleWifi_HandleOtaVersionReq(uint8_t *data, int len)
{
	uint16_t pid;
	uint16_t cid;
	uint16_t fid;
	uint8_t state = MwOta_VersionGet(&pid, &cid, &fid);

	BLEWIFI_INFO("BLEWIFI: BLEWIFI_REQ_OTA_VERSION\r\n");

	if (state != MW_OTA_OK)
		BleWifi_OtaSendVersionRsp(BLEWIFI_OTA_ERR_HW_FAILURE, 0, 0, 0);
	else
		BleWifi_OtaSendVersionRsp(BLEWIFI_OTA_SUCCESS, pid, cid, fid);
}

static uint8_t BleWifi_MwOtaPrepare(uint16_t uwProjectId, uint16_t uwChipId, uint16_t uwFirmwareId, uint32_t ulImageSize, uint32_t ulImageSum)
{
	uint8_t state = MW_OTA_OK;

	state = MwOta_Prepare(uwProjectId, uwChipId, uwFirmwareId, ulImageSize, ulImageSum);
	return state;
}

static uint8_t BleWifi_MwOtaDatain(uint8_t *pubAddr, uint32_t ulSize)
{
	uint8_t state = MW_OTA_OK;

	state = MwOta_DataIn(pubAddr, ulSize);
	return state;
}

static uint8_t BleWifi_MwOtaDataFinish(void)
{
	uint8_t state = MW_OTA_OK;

	state = MwOta_DataFinish();
	return state;
}

static void BleWifi_HandleOtaUpgradeReq(uint8_t *data, int len)
{
	blewifi_ota_t *ota = gTheOta;
	uint8_t state = MW_OTA_OK;

	BLEWIFI_INFO("BLEWIFI: BLEWIFI_REQ_OTA_UPGRADE\r\n");

	if (len != 26)
	{
		BleWifi_OtaSendUpgradeRsp(BLEWIFI_OTA_ERR_INVALID_LEN);
		return;
	}

	if (ota)
	{
		BleWifi_OtaSendUpgradeRsp(BLEWIFI_OTA_ERR_IN_PROGRESS);
		return;
	}

	ota = malloc(sizeof(blewifi_ota_t));

	if (ota)
	{
		T_MwOtaFlashHeader *ota_hdr= (T_MwOtaFlashHeader*) &data[2];

		ota->pkt_idx = 0;
		ota->idx     = 0;
        ota->rx_pkt  = *(uint16_t *)&data[0];
        ota->proj_id = ota_hdr->uwProjectId;
        ota->chip_id = ota_hdr->uwChipId;
        ota->fw_id   = ota_hdr->uwFirmwareId;
        ota->total   = ota_hdr->ulImageSize;
        ota->chksum  = ota_hdr->ulImageSum;
		ota->curr 	 = 0;

		state = BleWifi_MwOtaPrepare(ota->proj_id, ota->chip_id, ota->fw_id, ota->total, ota->chksum);

        if (state == MW_OTA_OK)
        {
	        BleWifi_OtaSendUpgradeRsp(BLEWIFI_OTA_SUCCESS);
	        gTheOta = ota;

	        App_Ctrl_MsgSend(APP_CTRL_MSG_OTHER_OTA_ON, NULL, 0);
        }
        else
            BleWifi_OtaSendEndRsp(BLEWIFI_OTA_ERR_HW_FAILURE, TRUE);
    }
	else
	{
		BleWifi_OtaSendUpgradeRsp(BLEWIFI_OTA_ERR_MEM_CAPACITY_EXCEED);
	}
}

static uint32_t BleWifi_OtaAdd(uint8_t *data, int len)
{
	uint16_t i;
	uint32_t sum = 0;

	for (i = 0; i < len; i++)
    {
		sum += data[i];
    }

    return sum;
}

static void BleWifi_HandleOtaRawReq(uint8_t *data, int len)
{
	blewifi_ota_t *ota = gTheOta;
	uint8_t state = MW_OTA_OK;

	BLEWIFI_INFO("BLEWIFI: BLEWIFI_REQ_OTA_RAW\r\n");

	if (!ota)
	{
		BleWifi_OtaSendEndRsp(BLEWIFI_OTA_ERR_NOT_ACTIVE, FALSE);
        goto err;
	}

	if ((ota->curr + len) > ota->total)
	{
		BleWifi_OtaSendEndRsp(BLEWIFI_OTA_ERR_INVALID_LEN, TRUE);
		goto err;
    }

	ota->pkt_idx++;
	ota->curr += len;
	ota->curr_chksum += BleWifi_OtaAdd(data, len);

	if ((ota->idx + len) >= 256)
	{
		UINT16 total = ota->idx + len;
		UINT8 *s = data;
		UINT8 *e = data + len;
		UINT16 cpyLen = 256 - ota->idx;

		if (ota->idx)
		{
			MemCopy(&ota->buf[ota->idx], s, cpyLen);
			s += cpyLen;
			total -= 256;
			ota->idx = 0;

			state = BleWifi_MwOtaDatain(ota->buf, 256);
		}

		if (state == MW_OTA_OK)
		{
			while (total >= 256)
			{
				state = BleWifi_MwOtaDatain(s, 256);
				s += 256;
				total -= 256;

				if (state != MW_OTA_OK) break;
			}

			if (state == MW_OTA_OK)
			{
				MemCopy(ota->buf, s, e - s);
				ota->idx = e - s;

				if ((ota->curr == ota->total) && ota->idx)
				{
					state = BleWifi_MwOtaDatain(ota->buf, ota->idx);
				}
			}
		}
	}
	else
	{
		MemCopy(&ota->buf[ota->idx], data, len);
		ota->idx += len;

		if ((ota->curr == ota->total) && ota->idx)
		{
			state = BleWifi_MwOtaDatain(ota->buf, ota->idx);
		}
	}

	if (state == MW_OTA_OK)
	{
		if (ota->rx_pkt && (ota->pkt_idx >= ota->rx_pkt))
		{
	        BleWifi_Ble_DataSendEncap(BLEWIFI_RSP_OTA_RAW, 0, 0);
	    		ota->pkt_idx = 0;
    }
  }
    else
		BleWifi_OtaSendEndRsp(BLEWIFI_OTA_ERR_HW_FAILURE, TRUE);

err:
	return;
}

static void BleWifi_HandleOtaEndReq(uint8_t *data, int len)
{
	blewifi_ota_t *ota = gTheOta;
	uint8_t status = data[0];

	BLEWIFI_INFO("BLEWIFI: BLEWIFI_REQ_OTA_END\r\n");

	if (!ota)
	{
		BleWifi_OtaSendEndRsp(BLEWIFI_OTA_ERR_NOT_ACTIVE, FALSE);
        goto err;
    }

		if (status == BLEWIFI_OTA_SUCCESS)
		{
		if (ota->curr == ota->total)
				{
					if (BleWifi_MwOtaDataFinish() == MW_OTA_OK)
						BleWifi_OtaSendEndRsp(BLEWIFI_OTA_SUCCESS, TRUE);
                    else
						BleWifi_OtaSendEndRsp(BLEWIFI_OTA_ERR_CHECKSUM, TRUE);
	            }
	            else
				{
					BleWifi_OtaSendEndRsp(BLEWIFI_OTA_ERR_INVALID_LEN, TRUE);
	            }
	        }
			else
			{
		if (ota) MwOta_DataGiveUp();

			// APP stop OTA
			BleWifi_OtaSendEndRsp(BLEWIFI_OTA_SUCCESS, TRUE);
		}

err:
	return;
}
#endif /* #if (BLE_OTA_FUNCTION_EN == 1) */

static void BleWifi_Wifi_SendSingleScanReport(uint16_t apCount, blewifi_scan_info_t *ap_list)
{
    uint8_t *data;
    int data_len;
    uint8_t *pos;
    int malloc_size = sizeof(blewifi_scan_info_t) * apCount;

    pos = data = malloc(malloc_size);
    if (data == NULL) {
        printf("malloc error\r\n");
        return;
    }

    for (int i = 0; i < apCount; ++i)
    {
        uint8_t len = ap_list[i].ssid_length;

        data_len = (pos - data);

        *pos++ = len;
        memcpy(pos, ap_list[i].ssid, len);
        pos += len;
        memcpy(pos, ap_list[i].bssid,6);
        pos += 6;
        *pos++ = ap_list[i].auth_mode;
        *pos++ = ap_list[i].rssi;
#ifdef USE_CONNECTED
        *pos++ = ap_list[i].connected;
#else
        *pos++ = 0;
#endif
    }

    data_len = (pos - data);

    BLEWIFI_DUMP("scan report data", data, data_len);

    /* create scan report data packet */
    BleWifi_Ble_DataSendEncap(BLEWIFI_RSP_SCAN_REPORT, data, data_len);

    free(data);
}

static void _BleWifi_Wifi_UpdateAutoConnList(uint16_t apCount, wifi_scan_info_t *ap_list,uint8_t *pu8IsUpdate)
{
    wifi_auto_connect_info_t *info = NULL;
    uint8_t u8AutoCount;
    uint16_t i, j;
    blewifi_wifi_get_auto_conn_ap_info_t stAutoConnApInfo;

    // if the count of auto-connection list is empty, don't update the auto-connect list
    BleWifi_Wifi_Query_Status(BLEWFII_WIFI_GET_AUTO_CONN_LIST_NUM , &u8AutoCount);
    //u8AutoCount = BleWifi_Wifi_AutoConnectListNum();
    if (0 == u8AutoCount)
    {
        *pu8IsUpdate = false;
        return;
    }

    // compare and update the auto-connect list
    // 1. prepare the buffer of auto-connect information
    info = (wifi_auto_connect_info_t *)malloc(sizeof(wifi_auto_connect_info_t));
    if (NULL == info)
    {
        printf("malloc fail, prepare is NULL\r\n");
        *pu8IsUpdate = false;
        return;
    }
    // 2. comapre
    for (i=0; i<u8AutoCount; i++)
    {
        // get the auto-connect information
        memset(info, 0, sizeof(wifi_auto_connect_info_t));
        stAutoConnApInfo.u8Index = i;
        stAutoConnApInfo.pstAutoConnInfo = info;
        BleWifi_Wifi_Query_Status(BLEWIFI_WIFI_GET_AUTO_CONN_AP_INFO , &stAutoConnApInfo);
        //wifi_auto_connect_get_ap_info(i, info);
        for (j=0; j<apCount; j++)
        {
            if (0 == MemCmp(ap_list[j].bssid, info->bssid, sizeof(info->bssid)))
            {
                // if the channel is not the same, update it
                if (ap_list[j].channel != info->ap_channel)
                    wifi_auto_connect_update_ch(i, ap_list[j].channel);
                *pu8IsUpdate = true;
                continue;
            }
        }
    }
    // 3. free the buffer of auto-connect information
    free(info);
}

int BleWifi_Wifi_UpdateScanInfoToAutoConnList(uint8_t *pu8IsUpdate)
{
    wifi_scan_info_t *ap_list = NULL;
    blewifi_wifi_get_ap_record_t stAPRecord;
    uint16_t u16apCount = 0;
    int8_t ubAppErr = 0;

    BleWifi_Wifi_Query_Status(BLEWIFI_WIFI_GET_AP_NUM , (void *)&u16apCount);
    //wifi_scan_get_ap_num(&apCount);

    if (u16apCount == 0) {
        printf("No AP found\r\n");
        *pu8IsUpdate = false;
        goto err;
    }
    //printf("ap num = %d\n", apCount);
    ap_list = (wifi_scan_info_t *)malloc(sizeof(wifi_scan_info_t) * u16apCount);

    if (!ap_list) {
        printf("malloc fail, ap_list is NULL\r\n");
        ubAppErr = -1;
        *pu8IsUpdate = false;
        goto err;
    }

    stAPRecord.pu16apCount = &u16apCount;
    stAPRecord.pstScanInfo = ap_list;
    BleWifi_Wifi_Query_Status(BLEWIFI_WIFI_GET_AP_RECORD , (void *)&stAPRecord);
    //wifi_scan_get_ap_records(&u16apCount, ap_list);

    _BleWifi_Wifi_UpdateAutoConnList(u16apCount, ap_list,pu8IsUpdate);

err:
    if (ap_list)
        free(ap_list);

    return ubAppErr;
}


int BleWifi_Wifi_SendScanReport(void)
{
    wifi_scan_info_t *pstAPList = NULL;
    blewifi_scan_info_t *blewifi_ap_list = NULL;
    uint16_t u16apCount = 0;
    int8_t ubAppErr = 0;
    int32_t i = 0, j = 0;
    uint8_t u8APPAutoConnectGetApNum = 0;
    wifi_auto_connect_info_t *info = NULL;
    uint8_t u8IsUpdate = false;
    blewifi_wifi_get_ap_record_t stAPRecord;
    blewifi_wifi_get_auto_conn_ap_info_t stAutoConnApInfo;

    memset(&stAPRecord , 0 ,sizeof(blewifi_wifi_get_ap_record_t));

    BleWifi_Wifi_Query_Status(BLEWIFI_WIFI_GET_AP_NUM , (void *)&u16apCount);
    //wifi_scan_get_ap_num(&u16apCount);

    if (u16apCount == 0) {
        printf("No AP found\r\n");
        goto err;
    }
    printf("ap num = %d\n", u16apCount);
    pstAPList = (wifi_scan_info_t *)malloc(sizeof(wifi_scan_info_t) * u16apCount);

    if (!pstAPList) {
        printf("malloc fail, ap_list is NULL\r\n");
        ubAppErr = -1;
        goto err;
    }

    stAPRecord.pu16apCount = &u16apCount;
    stAPRecord.pstScanInfo = pstAPList;
    BleWifi_Wifi_Query_Status(BLEWIFI_WIFI_GET_AP_RECORD , (void *)&stAPRecord);
    //wifi_scan_get_ap_records(&u16apCount, pstAPList);

    _BleWifi_Wifi_UpdateAutoConnList(u16apCount, pstAPList,&u8IsUpdate);

    blewifi_ap_list = (blewifi_scan_info_t *)malloc(sizeof(blewifi_scan_info_t) *u16apCount);
    if (!blewifi_ap_list) {
        printf("malloc fail, blewifi_ap_list is NULL\r\n");
        ubAppErr = -1;
        goto err;
    }

    memset(blewifi_ap_list , 0 , sizeof(blewifi_scan_info_t) *u16apCount);

    BleWifi_Wifi_Query_Status(BLEWIFI_WIFI_GET_AUTO_CONN_AP_NUM , &u8APPAutoConnectGetApNum);
    //wifi_auto_connect_get_ap_num(&ubAPPAutoConnectGetApNum);
    if (u8APPAutoConnectGetApNum)
    {
        info = (wifi_auto_connect_info_t *)malloc(sizeof(wifi_auto_connect_info_t) * u8APPAutoConnectGetApNum);
        if (!info) {
            printf("malloc fail, info is NULL\r\n");
            ubAppErr = -1;
            goto err;
        }

        memset(info, 0, sizeof(wifi_auto_connect_info_t) * u8APPAutoConnectGetApNum);

        for (i = 0; i < u8APPAutoConnectGetApNum; i++)
        {
            stAutoConnApInfo.u8Index = i;
            stAutoConnApInfo.pstAutoConnInfo = info+i;
            BleWifi_Wifi_Query_Status(BLEWIFI_WIFI_GET_AUTO_CONN_AP_INFO , &stAutoConnApInfo);
            //wifi_auto_connect_get_ap_info(i, (info+i));
        }
    }

    /* build blewifi ap list */
    for (i = 0; i < u16apCount; ++i)
    {
        memcpy(blewifi_ap_list[i].ssid, pstAPList[i].ssid, sizeof(pstAPList[i].ssid));
        memcpy(blewifi_ap_list[i].bssid, pstAPList[i].bssid, WIFI_MAC_ADDRESS_LENGTH);
        blewifi_ap_list[i].rssi = pstAPList[i].rssi;
        blewifi_ap_list[i].auth_mode = pstAPList[i].auth_mode;
        blewifi_ap_list[i].ssid_length = strlen((const char *)pstAPList[i].ssid);
        blewifi_ap_list[i].connected = 0;
#if (1 == FLITER_STRONG_AP_EN)
        blewifi_ap_list[i].u8IgnoreReport = false;
#endif
        for (j = 0; j < u8APPAutoConnectGetApNum; j++)
        {
            if ((info+j)->ap_channel)
            {
                if(!MemCmp(blewifi_ap_list[i].ssid, (info+j)->ssid, sizeof((info+j)->ssid)) && !MemCmp(blewifi_ap_list[i].bssid, (info+j)->bssid, sizeof((info+j)->bssid)))
                {
                    blewifi_ap_list[i].connected = 1;
                    break;
                }
            }
        }
    }

#if (1 == FLITER_STRONG_AP_EN)
    _BleWifi_Wifi_FilterStrongRssiAP(blewifi_ap_list , u16apCount);
#endif

    /* Send Data to BLE */
    /* Send AP inforamtion individually */
    for (i = 0; i < u16apCount; ++i)
    {
#if (1 == FLITER_STRONG_AP_EN)
        if(true == blewifi_ap_list[i].u8IgnoreReport)
        {
            continue;
        }
#endif
        if(blewifi_ap_list[i].ssid_length != 0)
        {
            BleWifi_Wifi_SendSingleScanReport(1, &blewifi_ap_list[i]);
            osDelay(100);
        }
    }

err:
    if (pstAPList)
        free(pstAPList);

    if (blewifi_ap_list)
        free(blewifi_ap_list);

    if (info)
        free(info);

    return ubAppErr;
}

static void BleWifi_MP_CalVbat(uint8_t *data, int len)
{
    float fTargetVbat;

    memcpy(&fTargetVbat, &data[0], 4);
    Hal_Aux_VbatCalibration(fTargetVbat);
    BleWifi_Ble_SendResponse(BLEWIFI_RSP_MP_CAL_VBAT, 0);
}

static void BleWifi_MP_CalIoVoltage(uint8_t *data, int len)
{
    float fTargetIoVoltage;
    uint8_t ubGpioIdx;

    memcpy(&ubGpioIdx, &data[0], 1);
    memcpy(&fTargetIoVoltage, &data[1], 4);
    Hal_Aux_IoVoltageCalibration(ubGpioIdx, fTargetIoVoltage);
    BleWifi_Ble_SendResponse(BLEWIFI_RSP_MP_CAL_IO_VOLTAGE, 0);
}

static void BleWifi_MP_CalTmpr(uint8_t *data, int len)
{
    BleWifi_Ble_SendResponse(BLEWIFI_RSP_MP_CAL_TMPR, 0);
}

static void BleWifi_MP_SysModeWrite(uint8_t *data, int len)
{
    T_MwFim_SysMode tSysMode;

    // set the settings of system mode
    tSysMode.ubSysMode = data[0];
    if (tSysMode.ubSysMode < MW_FIM_SYS_MODE_MAX)
    {
        if (MW_FIM_OK == MwFim_FileWrite(MW_FIM_IDX_GP03_PATCH_SYS_MODE, 0, MW_FIM_SYS_MODE_SIZE, (uint8_t*)&tSysMode))
        {
            App_Ctrl_SysModeSet(tSysMode.ubSysMode);
            BleWifi_Ble_SendResponse(BLEWIFI_RSP_MP_SYS_MODE_WRITE, 0);
            return;
        }
    }

    BleWifi_Ble_SendResponse(BLEWIFI_RSP_MP_SYS_MODE_WRITE, 1);
}

static void BleWifi_MP_SysModeRead(uint8_t *data, int len)
{
    uint8_t ubSysMode;

    ubSysMode = App_Ctrl_SysModeGet();
    BleWifi_Ble_SendResponse(BLEWIFI_RSP_MP_SYS_MODE_READ, ubSysMode);
}

static void BleWifi_Eng_SysReset(uint8_t *data, int len)
{
    BleWifi_Ble_SendResponse(BLEWIFI_RSP_ENG_SYS_RESET, 0);

    // wait the BLE response, then reset the system
    osDelay(3000);
    Hal_Sys_SwResetAll();
}

static void BleWifi_Eng_BleCmd(uint8_t *data, int len)
{
    msg_print_uart1("+BLE:%s\r\n", data);
    BleWifi_Ble_SendResponse(BLEWIFI_RSP_ENG_BLE_CMD, 0);
}

static void BleWifi_Eng_MacSrcWrite(uint8_t *data, int len)
{
    uint8_t sta_type, ble_type;
    int ret=0;
    u8 ret_st = true;
    blewifi_wifi_set_config_source_t stSetConfigSource;

    sta_type = data[0];
    ble_type = data[1];

    BLEWIFI_INFO("Enter BleWifi_Eng_MacSrcWrite: WiFi MAC Src=%d, BLE MAC Src=%d\n", sta_type, ble_type);

    stSetConfigSource.iface = MAC_IFACE_WIFI_STA;
    stSetConfigSource.type = (mac_source_type_t)sta_type;
    ret = BleWifi_Wifi_Set_Config(BLEWIFI_WIFI_SET_CONFIG_SOURCE , (void *)&stSetConfigSource);
    //ret = mac_addr_set_config_source(MAC_IFACE_WIFI_STA, (mac_source_type_t)sta_type);
    if (ret != 0) {
        ret_st = false;
        goto done;
    }

    stSetConfigSource.iface = MAC_IFACE_BLE;
    stSetConfigSource.type = (mac_source_type_t)ble_type;
    ret = BleWifi_Wifi_Set_Config(BLEWIFI_WIFI_SET_CONFIG_SOURCE , (void *)&stSetConfigSource);
    //ret = mac_addr_set_config_source(MAC_IFACE_BLE, (mac_source_type_t)ble_type);
    if (ret != 0) {
        ret_st = false;
        goto done;
    }


done:
    if (ret_st)
        BleWifi_Ble_SendResponse(BLEWIFI_RSP_ENG_MAC_SRC_WRITE, 0);
    else
        BleWifi_Ble_SendResponse(BLEWIFI_RSP_ENG_MAC_SRC_WRITE, 1);

}

static void BleWifi_Eng_MacSrcRead(uint8_t *data, int len)
{
    uint8_t sta_type, ble_type;
    blewifi_wifi_get_config_source_t stGetConfigSource;
    uint8_t MacSrc[2]={0};
    int ret=0;
    u8 ret_st = true;

    stGetConfigSource.iface = MAC_IFACE_WIFI_STA;
    stGetConfigSource.type = (mac_source_type_t *)&sta_type;
    ret = BleWifi_Wifi_Query_Status(BLEWIFI_WIFI_GET_CONFIG_SOURCE , (void *)&stGetConfigSource);
    //ret = mac_addr_get_config_source(MAC_IFACE_WIFI_STA, (mac_source_type_t *)&sta_type);
    if (ret != 0) {
        ret_st = false;
        goto done;
    }

    stGetConfigSource.iface = MAC_IFACE_BLE;
    stGetConfigSource.type = (mac_source_type_t *)&ble_type;
    ret = BleWifi_Wifi_Query_Status(BLEWIFI_WIFI_GET_CONFIG_SOURCE , (void *)&stGetConfigSource);
    //ret = mac_addr_get_config_source(MAC_IFACE_BLE, (mac_source_type_t *)&ble_type);
    if (ret != 0) {
        ret_st = false;
        goto done;
    }

    MacSrc[0] = sta_type;
    MacSrc[1] = ble_type;

    BLEWIFI_INFO("WiFi MAC Src=%d, BLE MAC Src=%d\n", MacSrc[0], MacSrc[1]);

done:
    if (ret_st)
        BleWifi_Ble_DataSendEncap(BLEWIFI_RSP_ENG_MAC_SRC_READ, MacSrc, 2);
    else{
        BleWifi_Ble_SendResponse(BLEWIFI_RSP_ENG_MAC_SRC_READ, 1);
    }


}

static void BleWifi_Ble_ProtocolHandler_Scan(uint16_t type, uint8_t *data, int len)
{
    wifi_scan_config_t stScanConfig={0};

    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_SCAN \r\n");

    stScanConfig.show_hidden = 1;
    stScanConfig.scan_type = WIFI_SCAN_TYPE_MIX;

    BleWifi_Wifi_Scan_Req(&stScanConfig);
}

static void BleWifi_Ble_ProtocolHandler_Connect(uint16_t type, uint8_t *data, int len)
{
    wifi_conn_config_t stConnConfig = {0};
    unsigned char ucDecPassword[BLEWIFI_WIFI_MAX_REC_PASSWORD_SIZE + 1] = {0};
    unsigned char iv[IV_SIZE + 1] = {0};
    size_t u16DecPasswordLen = 0;


    memcpy(stConnConfig.bssid, &data[0], WIFI_MAC_ADDRESS_LENGTH);

    stConnConfig.connected = 0; // ignore data[6]
    stConnConfig.conn_timeout = data[7] * 1000;

#if 0
    stConnConfig.password_length = data[7];
    if(stConnConfig.password_length != 0)
    {
        if(stConnConfig.password_length > WIFI_LENGTH_PASSPHRASE)
        {
            stConnConfig.password_length = WIFI_LENGTH_PASSPHRASE;
        }
        memcpy((char *)stConnConfig.password, &data[8], stConnConfig.password_length);
    }
#endif


    if(data[8] == 0) //password len = 0
    {
        printf("password_length = 0\r\n");
        stConnConfig.password_length = 0;
        memset((char *)stConnConfig.password, 0 , WIFI_LENGTH_PASSPHRASE);
    }
    else
    {
        if(data[8] > BLEWIFI_WIFI_MAX_REC_PASSWORD_SIZE)
        {
            BLEWIFI_INFO(" \r\n Not do Manually connect %d \r\n ",__LINE__);
            BleWifi_Ble_SendResponse(BLEWIFI_RSP_CONNECT, BLEWIFI_WIFI_PASSWORD_FAIL);
            return;
        }
        mbedtls_base64_decode(ucDecPassword , BLEWIFI_WIFI_MAX_REC_PASSWORD_SIZE + 1 , &u16DecPasswordLen , (unsigned char *)&data[9] , data[8]);
        memset(iv, '0' , IV_SIZE); //iv = "0000000000000000"
        BleWifi_CBC_decrypt((void *)ucDecPassword , u16DecPasswordLen , iv , g_ucSecretKey , (void *)stConnConfig.password);
        stConnConfig.password_length = strlen((char *)stConnConfig.password);
        //printf("password = %s\r\n" , wifi_config_req_connect.sta_config.password);
        //printf("password_length = %u\r\n" , wifi_config_req_connect.sta_config.password_length);
    }
//    wifi_auto_connect_reset();

    BLEWIFI_INFO("BLEWIFI: Recv Connect Request\r\n");
//    BleWifi_Ctrl_EventStatusSet(BLEWIFI_CTRL_EVENT_BIT_WIFI_CONNECTING, true);
//    wifi_set_config(WIFI_MODE_STA, &wifi_config_req_connect);
//    wifi_connection_connect(&wifi_config_req_connect);


    ////// for debug  ////////////////
    printf("[%s %d]conn_config.password=%s\n", __FUNCTION__, __LINE__, stConnConfig.password);
    printf("[%s %d]conn_config.connected=%d\n", __FUNCTION__, __LINE__, stConnConfig.connected);
    //////////////////////////////////
    BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_WIFI_USER_CONNECTING_EXEC, false);
    BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_WAIT_UPDATE_HOST, true);
    BleWifi_Wifi_Reset_Req();
    memset(&g_stStartConnConfig , 0 , sizeof(g_stStartConnConfig));
    memcpy(&g_stStartConnConfig , &stConnConfig , sizeof(stConnConfig));

}


static void BleWifi_Ble_ProtocolHandler_Manually_Connect_AP(uint16_t type, uint8_t *data, int len)
{
    wifi_conn_config_t stConnConfig = {0};
    unsigned char ucDecPassword[BLEWIFI_WIFI_MAX_REC_PASSWORD_SIZE + 1] = {0};
    unsigned char iv[IV_SIZE + 1] = {0};
    size_t u16DecPasswordLen = 0;
    uint8_t u8PasswordLen = 0;
    uint8_t u8TimeOutSettings;
//    wifi_scan_config_t scan_config = {0};


    printf("BLEWIFI: Recv BLEWIFI_REQ_MANUAL_CONNECT_AP \r\n");

    printf("BLEWIFI: Recv Connect Request\r\n");


    // data format for connecting a hidden AP
    //--------------------------------------------------------------------------------------
    //|        1     |    1~32    |    1      |     1    |         1          |   8~63     |
    //--------------------------------------------------------------------------------------
    //| ssid length  |    ssid    | Connected |  timeout |   password_length  |   password |
    //--------------------------------------------------------------------------------------

    stConnConfig.ssid_length = data[0];

    memcpy(stConnConfig.ssid, &data[1], data[0]);
    printf("\r\n %d  Recv Connect Request SSID is %s\r\n",__LINE__, stConnConfig.ssid);

    if (len >= (1 +data[0] +1 +1 +1) ) // ssid length(1) + ssid (data(0)) + Connected(1) + timeout + password_length (1)
    {
        BLEWIFI_INFO(" \r\n Do Manually connect %d \r\n ",__LINE__);

        u8TimeOutSettings = data[data[0] + 2];

        stConnConfig.conn_timeout = u8TimeOutSettings*1000;

        u8PasswordLen = data[data[0] + 3];
        if(u8PasswordLen == 0) //password len = 0
        {
            printf("password_length = 0\r\n");
            stConnConfig.password_length = 0;
            memset((char *)stConnConfig.password, 0 , WIFI_LENGTH_PASSPHRASE);
        }
        else
        {
            if(u8PasswordLen > BLEWIFI_WIFI_MAX_REC_PASSWORD_SIZE)
            {
                BLEWIFI_INFO(" \r\n Not do Manually connect %d \r\n ",__LINE__);
                BleWifi_Ble_SendResponse(BLEWIFI_RSP_CONNECT, BLEWIFI_WIFI_PASSWORD_FAIL);
                return;
            }
            mbedtls_base64_decode(ucDecPassword , BLEWIFI_WIFI_MAX_REC_PASSWORD_SIZE + 1 , &u16DecPasswordLen , (unsigned char *)&data[data[0] + 4] , u8PasswordLen);
            memset(iv, '0' , IV_SIZE); //iv = "0000000000000000"
            BleWifi_CBC_decrypt((void *)ucDecPassword , u16DecPasswordLen , iv , g_ucSecretKey , (void *)stConnConfig.password);
            stConnConfig.password_length = strlen((char *)stConnConfig.password);
            //printf("password = %s\r\n" , wifi_config_req_connect.sta_config.password);
            //printf("password_length = %u\r\n" , wifi_config_req_connect.sta_config.password_length);
        }

        BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_WIFI_USER_CONNECTING_EXEC, false);
        BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_WAIT_UPDATE_HOST, true);
        BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_MANUALLY_CONNECT_SCAN, true);
        BleWifi_Wifi_Reset_Req();
        memset(&g_stStartConnConfig , 0 , sizeof(g_stStartConnConfig));
        memcpy(&g_stStartConnConfig , &stConnConfig , sizeof(stConnConfig));
    }
    else
    {
        BLEWIFI_INFO(" \r\n Not do Manually connect %d \r\n ",__LINE__);
        BleWifi_Wifi_Reset_Req();
        BleWifi_Ble_SendResponse(BLEWIFI_RSP_CONNECT, BLEWIFI_WIFI_CONNECTED_FAIL);
    }
}


static void BleWifi_Ble_ProtocolHandler_Disconnect(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_DISCONNECT \r\n");

    BleWifi_Wifi_Stop_Conn();
}

static void BleWifi_Ble_ProtocolHandler_Reconnect(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_RECONNECT \r\n");
}

static void BleWifi_Ble_ProtocolHandler_ReadDeviceInfo(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_READ_DEVICE_INFO \r\n");

    blewifi_device_info_t stDevInfo = {0};
    char u8aManufacturerName[33] = {0};

    BleWifi_Wifi_Query_Status(BLEWIFI_WIFI_QUERY_GET_MAC_ADDR , (void *)&stDevInfo.device_id[0]);

    BleWifi_Wifi_Query_Status(BLEWIFI_WIFI_QUERY_GET_MANUFACTURER_NAME , (void *)&u8aManufacturerName);

    stDevInfo.name_len = strlen(u8aManufacturerName);
    memcpy(stDevInfo.manufacturer_name, u8aManufacturerName, stDevInfo.name_len);
    _BleWifi_Wifi_SendDeviceInfo(&stDevInfo);
}

static void BleWifi_Ble_ProtocolHandler_WriteDeviceInfo(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_WRITE_DEVICE_INFO \r\n");

    blewifi_device_info_t stDevInfo = {0};

    memset(&stDevInfo, 0, sizeof(blewifi_device_info_t));
    memcpy(stDevInfo.device_id, &data[0], WIFI_MAC_ADDRESS_LENGTH);
    stDevInfo.name_len = data[6];
    memcpy(stDevInfo.manufacturer_name, &data[7], stDevInfo.name_len);

    BleWifi_Wifi_Query_Status(BLEWIFI_WIFI_SET_MAC_ADDR , (void *)&stDevInfo.device_id[0]);

    BleWifi_Wifi_Query_Status(BLEWIFI_WIFI_SET_MANUFACTURER_NAME , (void *)&stDevInfo.manufacturer_name[0]);

    BleWifi_Ble_SendResponse(BLEWIFI_RSP_WRITE_DEVICE_INFO, 0);

    BLEWIFI_INFO("BLEWIFI: Device ID: \""MACSTR"\"\r\n", MAC2STR(stDevInfo.device_id));
    BLEWIFI_INFO("BLEWIFI: Device Manufacturer: %s",stDevInfo.manufacturer_name);

}

void BleWifi_Wifi_SendStatusInfo(uint16_t u16Type)
{
    uint8_t *pu8Data, *pu8Pos;
    uint8_t u8Status = 0, u8StrLen = 0;
    uint16_t u16DataLen;
    uint8_t u8aIp[4], u8aNetMask[4], u8aGateway[4];
    wifi_scan_info_t stInfo;
    struct netif *iface = netif_find("st1");

    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_WIFI_STATUS \r\n");

    u8aIp[0] = (iface->ip_addr.u_addr.ip4.addr >> 0) & 0xFF;
    u8aIp[1] = (iface->ip_addr.u_addr.ip4.addr >> 8) & 0xFF;
    u8aIp[2] = (iface->ip_addr.u_addr.ip4.addr >> 16) & 0xFF;
    u8aIp[3] = (iface->ip_addr.u_addr.ip4.addr >> 24) & 0xFF;

    u8aNetMask[0] = (iface->netmask.u_addr.ip4.addr >> 0) & 0xFF;
    u8aNetMask[1] = (iface->netmask.u_addr.ip4.addr >> 8) & 0xFF;
    u8aNetMask[2] = (iface->netmask.u_addr.ip4.addr >> 16) & 0xFF;
    u8aNetMask[3] = (iface->netmask.u_addr.ip4.addr >> 24) & 0xFF;

    u8aGateway[0] = (iface->gw.u_addr.ip4.addr >> 0) & 0xFF;
    u8aGateway[1] = (iface->gw.u_addr.ip4.addr >> 8) & 0xFF;
    u8aGateway[2] = (iface->gw.u_addr.ip4.addr >> 16) & 0xFF;
    u8aGateway[3] = (iface->gw.u_addr.ip4.addr >> 24) & 0xFF;

    BleWifi_Wifi_Query_Status(BLEWIFI_WIFI_QUERY_AP_INFO , (void *)&stInfo);

    pu8Pos = pu8Data = malloc(sizeof(blewifi_wifi_status_info_t));
    if (pu8Data == NULL) {
        printf("malloc error\r\n");
        return;
    }

    u8StrLen = strlen((char *)&stInfo.ssid);

    if (u8StrLen == 0)
    {
        u8Status = 1; // Return Failure
        if (u16Type == BLEWIFI_IND_IP_STATUS_NOTIFY)     // if failure, don't notify the status
            goto release;
    }
    else
    {
        u8Status = 0; // Return success
    }

    /* Status */
    *pu8Pos++ = u8Status;

    /* ssid length */
    *pu8Pos++ = u8StrLen;

   /* SSID */
    if (u8StrLen != 0)
    {
        memcpy(pu8Pos, stInfo.ssid, u8StrLen);
        pu8Pos += u8StrLen;
    }

   /* BSSID */
    memcpy(pu8Pos, stInfo.bssid, 6);
    pu8Pos += 6;

    /* IP */
    memcpy(pu8Pos, (char *)u8aIp, 4);
    pu8Pos += 4;

    /* MASK */
    memcpy(pu8Pos,  (char *)u8aNetMask, 4);
    pu8Pos += 4;

    /* GATEWAY */
    memcpy(pu8Pos,  (char *)u8aGateway, 4);
    pu8Pos += 4;

    u16DataLen = (pu8Pos - pu8Data);

    BLEWIFI_DUMP("Wi-Fi status info data", pu8Data, u16DataLen);
    /* create Wi-Fi status info data packet */
    BleWifi_Ble_DataSendEncap(u16Type, pu8Data, u16DataLen);
    //BleWifi_Ble_DataSendEncap(BLEWIFI_RSP_WIFI_STATUS, pu8Data, u16DataLen);

release:
    free(pu8Data);
}

static void BleWifi_Ble_ProtocolHandler_WifiStatus(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_WIFI_STATUS \r\n");
    BleWifi_Wifi_SendStatusInfo(BLEWIFI_RSP_WIFI_STATUS);
}

static void BleWifi_Ble_ProtocolHandler_Reset(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_RESET \r\n");

    BleWifi_Wifi_Reset_Req();
}

#if (BLE_OTA_FUNCTION_EN == 1)
static void BleWifi_Ble_ProtocolHandler_OtaVersion(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_OTA_VERSION \r\n");
    BleWifi_HandleOtaVersionReq(data, len);
}

static void BleWifi_Ble_ProtocolHandler_OtaUpgrade(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_OTA_UPGRADE \r\n");
    BleWifi_HandleOtaUpgradeReq(data, len);
}

static void BleWifi_Ble_ProtocolHandler_OtaRaw(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_OTA_RAW \r\n");
    BleWifi_HandleOtaRawReq(data, len);
}

static void BleWifi_Ble_ProtocolHandler_OtaEnd(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_OTA_END \r\n");
    BleWifi_HandleOtaEndReq(data, len);
}
#endif

static void BleWifi_Ble_ProtocolHandler_MpCalVbat(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_MP_CAL_VBAT \r\n");
    BleWifi_MP_CalVbat(data, len);
}

static void BleWifi_Ble_ProtocolHandler_MpCalIoVoltage(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_MP_CAL_IO_VOLTAGE \r\n");
    BleWifi_MP_CalIoVoltage(data, len);
}

static void BleWifi_Ble_ProtocolHandler_MpCalTmpr(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_MP_CAL_TMPR \r\n");
    BleWifi_MP_CalTmpr(data, len);
}

static void BleWifi_Ble_ProtocolHandler_MpSysModeWrite(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_MP_SYS_MODE_WRITE \r\n");
    BleWifi_MP_SysModeWrite(data, len);
}

static void BleWifi_Ble_ProtocolHandler_MpSysModeRead(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_MP_SYS_MODE_READ \r\n");
    BleWifi_MP_SysModeRead(data, len);
}

static void BleWifi_Ble_ProtocolHandler_EngSysReset(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_ENG_SYS_RESET \r\n");
    BleWifi_Eng_SysReset(data, len);
}

static void BleWifi_Ble_ProtocolHandler_EngWifiMacWrite(uint16_t type, uint8_t *data, int len)
{
    uint8_t u8aMacAddr[6];
    blewifi_wifi_set_config_source_t stSetConfigSource;

    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_ENG_WIFI_MAC_WRITE \r\n");

    // save the mac address into flash
    memcpy(u8aMacAddr, &data[0], 6);
    BleWifi_Wifi_Set_Config(BLEWIFI_WIFI_SET_MAC_ADDR , (void *)&u8aMacAddr[0]);

    // apply the mac address from flash
    stSetConfigSource.iface = MAC_IFACE_WIFI_STA;
    stSetConfigSource.type = MAC_SOURCE_FROM_FLASH;
    BleWifi_Wifi_Set_Config(BLEWIFI_WIFI_SET_CONFIG_SOURCE , (void *)&stSetConfigSource);

    BleWifi_Ble_SendResponse(BLEWIFI_RSP_ENG_WIFI_MAC_WRITE, 0);
}

static void BleWifi_Ble_ProtocolHandler_EngWifiMacRead(uint16_t type, uint8_t *data, int len)
{
    uint8_t u8aMacAddr[6];

    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_ENG_WIFI_MAC_READ \r\n");

    // get the mac address from flash
    BleWifi_Wifi_Query_Status(BLEWIFI_WIFI_QUERY_GET_MAC_ADDR , (void *)&u8aMacAddr[0]);

    BleWifi_Ble_DataSendEncap(BLEWIFI_RSP_ENG_WIFI_MAC_READ, u8aMacAddr , 6);
}

static void BleWifi_Ble_ProtocolHandler_EngBleMacWrite(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_ENG_BLE_MAC_WRITE \r\n");
    BleWifi_Ble_MacAddrWrite(data);
    BleWifi_Ble_SendResponse(BLEWIFI_RSP_ENG_BLE_MAC_WRITE, 0);
}

static void BleWifi_Ble_ProtocolHandler_EngBleMacRead(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_ENG_BLE_MAC_READ \r\n");
    BleWifi_Ble_MacAddrRead(data);
    BleWifi_Ble_DataSendEncap(BLEWIFI_RSP_ENG_BLE_MAC_READ, data, 6);
}

static void BleWifi_Ble_ProtocolHandler_EngBleCmd(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_ENG_BLE_CMD \r\n");
    BleWifi_Eng_BleCmd(data, len);
}

static void BleWifi_Ble_ProtocolHandler_EngMacSrcWrite(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_ENG_MAC_SRC_WRITE \r\n");
    BleWifi_Eng_MacSrcWrite(data, len);
}

static void BleWifi_Ble_ProtocolHandler_EngMacSrcRead(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_ENG_MAC_SRC_READ \r\n");
    BleWifi_Eng_MacSrcRead(data, len);
}


static void BleWifi_Ble_ProtocolHandler_Auth(uint16_t type, uint8_t *data, int len)
{
    unsigned char iv[IV_SIZE] = {0};
    unsigned char ucBase64Dec[MAX_AUTH_DATA_SIZE + 1] = {0};
    size_t uBase64DecLen = 0;
    unsigned char ucUuidEncData[UUID_ENC_SIZE + 1] = {0};
    unsigned char ucUuidtoBaseData[ENC_UUID_TO_BASE64_SIZE + 1] = {0};  // to base64 max size ((4 * n / 3) + 3) & ~3
    size_t uBase64EncLen = 0;

    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_AUTH \r\n");

    if(len > MAX_AUTH_DATA_SIZE)
    {
        uBase64EncLen = 0;
        BleWifi_Ble_DataSendEncap(BLEWIFI_RSP_AUTH, ucUuidtoBaseData , uBase64EncLen);
        return ;
    }

    mbedtls_md5((unsigned char *)&g_tHttpPostContent.ubaApiKey , strlen(g_tHttpPostContent.ubaApiKey) , g_ucSecretKey);

    mbedtls_base64_decode(ucBase64Dec , MAX_AUTH_DATA_SIZE + 1 , &uBase64DecLen , (unsigned char *)data , len);
    memset(iv, '0' , IV_SIZE); //iv = "0000000000000000"
    BleWifi_CBC_decrypt((void *)ucBase64Dec , uBase64DecLen , iv , g_ucSecretKey , g_ucAppCode);

    printf("g_ucAppCode = %s\r\n",g_ucAppCode);

    //UUID generate
    memset(g_ucDeviceCode , 0 , UUID_SIZE);
    if(BleWifi_UUID_Generate(g_ucDeviceCode , (UUID_SIZE + 1)) == false)
    {
        uBase64EncLen = 0;
        BleWifi_Ble_DataSendEncap(BLEWIFI_RSP_AUTH, ucUuidtoBaseData , uBase64EncLen);
        return ;
    }

    printf("g_ucDeviceCode = %s\r\n",g_ucDeviceCode);

    memset(iv, '0' , IV_SIZE); //iv = "0000000000000000"
    BleWifi_CBC_encrypt((void *)g_ucDeviceCode , UUID_SIZE , iv , g_ucSecretKey , (void *)ucUuidEncData);
    mbedtls_base64_encode((unsigned char *)ucUuidtoBaseData , ENC_UUID_TO_BASE64_SIZE + 1  ,&uBase64EncLen ,(unsigned char *)ucUuidEncData , UUID_ENC_SIZE);

    //BLEWIFI_INFO("ucUuidtoBaseData = %s\r\n",ucUuidtoBaseData);

    BleWifi_Ble_DataSendEncap(BLEWIFI_RSP_AUTH, ucUuidtoBaseData , uBase64EncLen);
}

static void BleWifi_Ble_ProtocolHandler_AuthToken(uint16_t type, uint8_t *data, int len)
{
    unsigned char iv[IV_SIZE + 1] = {0};
    unsigned char ucBase64Dec[MAX_AUTH_TOKEN_DATA_SIZE + 1] = {0};
    size_t uBase64DecLen = 0;
    unsigned char ucCbcDecData[MAX_AUTH_TOKEN_DATA_SIZE + 1] = {0};
    uint8_t u8Ret = 0; // 0 success , 1 fail
    char * pcToken = NULL;

    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_AUTH_TOKEN \r\n");

    if(len > MAX_AUTH_TOKEN_DATA_SIZE)
    {
        BleWifi_Ble_SendResponse(BLEWIFI_RSP_AUTH_TOKEN , u8Ret);
        return;
    }

    mbedtls_base64_decode(ucBase64Dec , MAX_AUTH_TOKEN_DATA_SIZE + 1 , &uBase64DecLen , (unsigned char *)data , len);
    memset(iv, '0' , IV_SIZE); //iv = "0000000000000000"
    BleWifi_CBC_decrypt((void *)ucBase64Dec , uBase64DecLen , iv , g_ucSecretKey , ucCbcDecData);

    printf("ucCbcDecData = %s\r\n",ucCbcDecData);

    pcToken = strtok((char *)ucCbcDecData , "_");
    if(strcmp(pcToken ,(char *)g_ucAppCode) != 0)
    {
        u8Ret = 1;
    }
    pcToken = strtok(NULL , "_");
    if(strcmp(pcToken ,(char *)g_ucDeviceCode) != 0)
    {
        u8Ret = 1;
    }

    BleWifi_Ble_SendResponse(BLEWIFI_RSP_AUTH_TOKEN , u8Ret);
}

static void BleWifi_AppDeviceInfoRsp()
{
    int len = 0;
    uint8_t *data;
    uint8_t DeviceIDLength = 0;
    uint8_t ApiKeyLength = 0;
    uint8_t ChipIDLength = 0;
    uint8_t TotalSize = 0;
    unsigned char ucCbcEncData[128];
    size_t uCbcEncLen = 0;
    unsigned char ucBaseData[128];
    size_t uBaseLen = 0;
    unsigned char iv[17]={0};


    DeviceIDLength =  strlen(g_tHttpPostContent.ubaDeviceId);
    ApiKeyLength =  MAX_RSP_BASE64_API_KEY_LEN;
    ChipIDLength =  MAX_RSP_BASE64_CHIP_ID_LEN;

    TotalSize = DeviceIDLength + ApiKeyLength + ChipIDLength + 3;

    data = (uint8_t*) malloc((TotalSize + 1) * sizeof(uint8_t));

    memcpy(&data[len], &DeviceIDLength, 1);
    len = len + 1;
    memcpy(&data[len],  g_tHttpPostContent.ubaDeviceId, DeviceIDLength);
    len = len + DeviceIDLength;

    memset(iv, '0' , IV_SIZE); //iv = "0000000000000000"
    uCbcEncLen = strlen(g_tHttpPostContent.ubaApiKey);
    uCbcEncLen = (((uCbcEncLen  >> 4) + 1) << 4);
    BleWifi_CBC_encrypt((void *)g_tHttpPostContent.ubaApiKey , strlen(g_tHttpPostContent.ubaApiKey) , iv , g_ucSecretKey , (void *)ucCbcEncData);
    mbedtls_base64_encode((unsigned char *)ucBaseData , 128  ,&uBaseLen ,(unsigned char *)ucCbcEncData , uCbcEncLen);
    memcpy(&data[len], &uBaseLen, 1);
    len = len + 1;
    memcpy(&data[len],  ucBaseData, uBaseLen);
    len = len + uBaseLen;

    memset(ucCbcEncData , 0 , 128);
    memset(ucBaseData , 0 , 128);
    memset(iv, '0' , IV_SIZE); //iv = "0000000000000000"
    uCbcEncLen = strlen(g_tHttpPostContent.ubaChipId);
    uCbcEncLen = (((uCbcEncLen  >> 4) + 1) << 4);
    BleWifi_CBC_encrypt((void *)g_tHttpPostContent.ubaChipId , strlen(g_tHttpPostContent.ubaChipId) , iv , g_ucSecretKey , (void *)ucCbcEncData);
    mbedtls_base64_encode((unsigned char *)ucBaseData , 128  ,&uBaseLen ,(unsigned char *)ucCbcEncData , uCbcEncLen);
    memcpy(&data[len], &uBaseLen, 1);
    len = len + 1;
    memcpy(&data[len],  ucBaseData, uBaseLen);
    len = len + uBaseLen;


    BleWifi_Ble_DataSendEncap(BLEWIFI_RSP_APP_DEVICE_INFO, data, len);

    free(data);

}

static void BleWifi_AppWifiConnection(uint8_t *data, int len)
{
    int totallen = data[0] + (data[1] << 8);
    char *AllURL = NULL;
    char HeaderStr[12] = {0};
    const char *PosStart = (char *) &data[2];
    const char *PosResult;
    const char *NeedleStart = "/";
    uint8_t TerminalNull = 1;

    T_MwFim_GP12_HttpHostInfo HostInfo;
    memset(&HostInfo ,0, sizeof(T_MwFim_GP12_HttpHostInfo));

    if(NULL != strstr(PosStart, "http://"))
    {
        strcpy(HeaderStr, "http://");
    }
    else if(NULL != strstr(PosStart, "https://"))
    {
        strcpy(HeaderStr, "https://");
    }
    else
    {
        BleWifi_Ble_SendResponse(BLEWIFI_RSP_APP_HOST_INFO, 1);

        return;
    }

    AllURL = (char *)malloc(totallen + TerminalNull);
    if (NULL == AllURL)
    {
        BleWifi_Ble_SendResponse(BLEWIFI_RSP_APP_HOST_INFO, 1);
        return;
    }

    memset(AllURL ,0, totallen);

    if ( (PosStart=strstr(PosStart,HeaderStr)) != NULL ) {
        PosResult = PosStart;
    }

    // Copy http://testapi.coolkit.cn:8080/api/user/device/update to AllURL
    memcpy(AllURL, PosResult, totallen);
    AllURL[totallen] = '\0';

    PosResult = strstr ((AllURL + strlen(HeaderStr)), NeedleStart);

    // Copy testapi.coolkit.cn:8080 / testapi.coolkit.cn to URL.
    // Calculate URL length = PosResult - AllURL - strlen(HeaderStr)
    strncpy (HostInfo.ubaHostInfoURL, (AllURL + strlen(HeaderStr)), PosResult - AllURL - strlen(HeaderStr));

    memcpy (HostInfo.ubaHostInfoDIR, (AllURL + strlen(HeaderStr) + strlen(HostInfo.ubaHostInfoURL)), (totallen - strlen(HeaderStr) - strlen(HostInfo.ubaHostInfoURL)));

    memcpy(&g_tHostInfo,&HostInfo,sizeof(T_MwFim_GP12_HttpHostInfo));

    if (MW_FIM_OK != MwFim_FileWrite(MW_FIM_IDX_GP12_PROJECT_HOST_INFO, 0, MW_FIM_GP12_HTTP_HOST_INFO_SIZE, (uint8_t *)&HostInfo)) {
        BleWifi_Ble_SendResponse(BLEWIFI_RSP_APP_HOST_INFO, 1);
    }

    //if cloud is connected, do cloud disconnect first then execute cloud connection
    Iot_Data_TxTask_MsgSend(IOT_DATA_TX_MSG_CLOUD_DISCONNECTION, NULL, 0);

    IoT_Ring_Buffer_ResetBuffer(&g_stIotRbData);

    BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_WAIT_UPDATE_HOST, false);

    Iot_Data_TxTask_MsgSend(IOT_DATA_TX_MSG_CLOUD_CONNECTION, NULL, 0);

    BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_CHANGE_HTTPURL, true);

    BleWifi_Ble_SendResponse(BLEWIFI_RSP_APP_HOST_INFO, 0);

    osTimerStop(g_tAppCtrl_network_stop_delay_timeout_timer);
    osTimerStart(g_tAppCtrl_network_stop_delay_timeout_timer, NETWORK_STOP_DELAY);

    App_Sensor_Post_To_Cloud(SHORT_TRIG);

    if(AllURL != NULL)
    {
        free(AllURL);
    }
}


static void BleWifi_Ble_ProtocolHandler_AppDeviceInfo(uint16_t type, uint8_t *data, int len)
{
    printf("BLEWIFI: Recv BLEWIFI_REQ_APP_DEVICE_INFO \r\n");
    BleWifi_AppDeviceInfoRsp();
}

static void BleWifi_Ble_ProtocolHandler_AppWifiConnection(uint16_t type, uint8_t *data, int len)
{
    BLEWIFI_INFO("BLEWIFI: Recv BLEWIFI_REQ_APP_HOST_INFO \r\n");
    BleWifi_AppWifiConnection(data, len);
}



// it is used in the ctrl task
void BleWifi_Ble_ProtocolHandler(uint16_t type, uint8_t *data, int len)
{
    uint32_t i = 0;

    while (g_tBleProtocolHandlerTbl[i].ulEventId != 0xFFFFFFFF)
    {
        // match
        if (g_tBleProtocolHandlerTbl[i].ulEventId == type)
        {
            g_tBleProtocolHandlerTbl[i].fpFunc(type, data, len);
            break;
        }

        i++;
    }

    // not match
    if (g_tBleProtocolHandlerTbl[i].ulEventId == 0xFFFFFFFF)
    {
    }
}

// it is used in the ctrl task
void BleWifi_Ble_DataRecvHandler(uint8_t *data, int data_len)
{
    blewifi_hdr_t *hdr = NULL;
    int hdr_len = sizeof(blewifi_hdr_t);

    /* 1.aggregate fragment data packet, only first frag packet has header */
    /* 2.handle blewifi data packet, if data frame is aggregated completely */
    if (g_rx_packet.offset == 0)
    {
        hdr = (blewifi_hdr_t*)data;
        g_rx_packet.total_len = hdr->data_len + hdr_len;
        g_rx_packet.remain = g_rx_packet.total_len;
        g_rx_packet.aggr_buf = malloc(g_rx_packet.total_len);

        if (g_rx_packet.aggr_buf == NULL) {
           BLEWIFI_ERROR("%s no mem, len %d\n", __func__, g_rx_packet.total_len);
           return;
        }
    }

    // error handle
    // if the size is overflow, don't copy the whole data
    if (data_len > g_rx_packet.remain)
        data_len = g_rx_packet.remain;

    memcpy(g_rx_packet.aggr_buf + g_rx_packet.offset, data, data_len);
    g_rx_packet.offset += data_len;
    g_rx_packet.remain -= data_len;

    /* no frag or last frag packet */
    if (g_rx_packet.remain == 0)
    {
        hdr = (blewifi_hdr_t*)g_rx_packet.aggr_buf;
        BleWifi_Ble_ProtocolHandler(hdr->type, g_rx_packet.aggr_buf + hdr_len,  (g_rx_packet.total_len - hdr_len));
        g_rx_packet.offset = 0;
        g_rx_packet.remain = 0;
        free(g_rx_packet.aggr_buf);
        g_rx_packet.aggr_buf = NULL;
    }
}

void BleWifi_Ble_DataSendEncap(uint16_t type_id, uint8_t *data, int total_data_len)
{
    blewifi_hdr_t *hdr = NULL;
    int remain_len = total_data_len;

    /* 1.fragment data packet to fit MTU size */

    /* 2.Pack blewifi header */
    hdr = malloc(sizeof(blewifi_hdr_t) + remain_len);
    if (hdr == NULL)
    {
        BLEWIFI_ERROR("BLEWIFI: memory alloc fail\r\n");
        return;
    }

    hdr->type = type_id;
    hdr->data_len = remain_len;
    if (hdr->data_len)
        memcpy(hdr->data, data, hdr->data_len);

    BLEWIFI_DUMP("[BLEWIFI]:out packet", (uint8_t*)hdr, (hdr->data_len + sizeof(blewifi_hdr_t)));

    /* 3.send app data to BLE stack */
    BleWifi_Ble_SendAppMsgToBle(BW_APP_MSG_SEND_DATA, (hdr->data_len + sizeof(blewifi_hdr_t)), (uint8_t *)hdr);

    free(hdr);
}

void BleWifi_Ble_SendResponse(uint16_t type_id, uint8_t status)
{
    BleWifi_Ble_DataSendEncap(type_id, &status, 1);
}

#if 0
static int32_t _BleWifi_UtilHexToStr(void *data, UINT16 len, UINT8 **p)
{
	UINT8 t[] = "0123456789ABCDEF";
	UINT8 *num = data;
	UINT8 *buf = *p;
    UINT16 i = 0;

	while (len--)
	{
		buf[i << 1] = t[num[i] >> 4];
		buf[(i << 1) + 1] = t[num[i] & 0xf];
		i++;
    }

    *p += (i << 1);
    return 0;
}
#endif

int32_t BleWifi_Ble_InitAdvData(uint8_t *pu8Data , uint8_t *pu8Len)
{
    uint8_t ubLen;
	UINT8 bleAdvertData[] =
	{
        0x02,
        GAP_ADTYPE_FLAGS,
        GAP_ADTYPE_FLAGS_GENERAL | GAP_ADTYPE_FLAGS_BREDR_NOT_SUPPORTED,
        // connection interval range
        0x05,
        GAP_ADTYPE_SLAVE_CONN_INTERVAL_RANGE,
        UINT16_LO(DEFAULT_DESIRED_MIN_CONN_INTERVAL),
        UINT16_HI(DEFAULT_DESIRED_MIN_CONN_INTERVAL),
        UINT16_LO(DEFAULT_DESIRED_MAX_CONN_INTERVAL),
        UINT16_HI(DEFAULT_DESIRED_MAX_CONN_INTERVAL),
        0x02,
        GAP_ADTYPE_POWER_LEVEL,
        0,
        0x11,
        GAP_ADTYPE_128BIT_COMPLETE,
        0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xAA, 0xAA, 0x00, 0x00
	};

    // error handle
	ubLen = sizeof(bleAdvertData);
	if (ubLen > BLE_ADV_SCAN_BUF_SIZE)
	    ubLen = BLE_ADV_SCAN_BUF_SIZE;
	*pu8Len = ubLen;
	MemCopy(pu8Data , bleAdvertData, ubLen);

	return 0;
}

int32_t BleWifi_Ble_InitScanData(uint8_t *pu8Data , uint8_t *pu8Len)
{
    uint8_t ubLen;
    BOOL isOk = FALSE;

    if (MW_FIM_OK != MwFim_FileRead(MW_FIM_IDX_GP12_PROJECT_DEVICE_AUTH_CONTENT, 0, MW_FIM_GP12_HTTP_POST_CONTENT_SIZE, (uint8_t *)&g_tHttpPostContent))
    {
        memcpy(&g_tHttpPostContent, &g_tMwFimDefaultGp12HttpPostContent, MW_FIM_GP12_HTTP_POST_CONTENT_SIZE);
    }

    if (BLEWIFI_BLE_DEVICE_NAME_METHOD == 1)
    {
    	BD_ADDR addr;

    	if (LeGapGetBdAddr(addr) == SYS_ERR_SUCCESS)
    	{
    		UINT8 *p = pu8Data;
    		UINT16 i = BLEWIFI_BLE_DEVICE_NAME_POSTFIX_COUNT;

    		// error handle, the mac address length
    		if (i > 6)
    		    i = 6;

    		*p++ = 0x10;
    		*p++ = GAP_ADTYPE_LOCAL_NAME_COMPLETE;
            // error handle
            // !!! if i = 4, the other char are 12 bytes (i*3)
            ubLen = strlen((const char *)(BLEWIFI_BLE_DEVICE_NAME_PREFIX));
    		if (ubLen > (BLE_ADV_SCAN_BUF_SIZE - 2 - (i*3)))
    	        ubLen = BLE_ADV_SCAN_BUF_SIZE - 2 - (i*3);

    		MemCopy(p, BLEWIFI_BLE_DEVICE_NAME_PREFIX, ubLen);

    		p += ubLen;
            ubLen = strlen((const char *)(g_tHttpPostContent.ubaDeviceId));
            MemCopy(p, g_tHttpPostContent.ubaDeviceId , ubLen);
            p += ubLen;
#if 0
            if (i > 0)
            {
        		while (i--)
        		{
                    _BleWifi_UtilHexToStr(&addr[i], 1, &p);
        			*p++ = ':';
                }

                *pu8Len = p - pu8Data - 1;    // remove the last char ":"
            }
            else
            {
                *pu8Len = p - pu8Data;
            }
            pu8Data[0] = (*pu8Len - 1);     // update the total length, without buf[0]
#endif
            *pu8Len = p - pu8Data;
            pu8Data[0] = (*pu8Len - 1);     // update the total length, without buf[0]

            isOk = TRUE;
        }
    }
    else if (BLEWIFI_BLE_DEVICE_NAME_METHOD == 2)
    {
        // error handle
        ubLen = strlen((const char *)(BLEWIFI_BLE_DEVICE_NAME_FULL));
        if (ubLen > (BLE_ADV_SCAN_BUF_SIZE - 2))
            ubLen = (BLE_ADV_SCAN_BUF_SIZE - 2);
    	*pu8Len = ubLen + 2;
        pu8Data[0] = *pu8Len - 1;
        pu8Data[1] = GAP_ADTYPE_LOCAL_NAME_COMPLETE;
        MemCopy(pu8Data + 2 , BLEWIFI_BLE_DEVICE_NAME_FULL, ubLen);

        isOk = TRUE;
    }

    // error handle to give the default value
    if (isOk != TRUE)
    {
        // error handle
        ubLen = strlen("OPL_Device");
        if (ubLen > (BLE_ADV_SCAN_BUF_SIZE - 2))
            ubLen = (BLE_ADV_SCAN_BUF_SIZE - 2);
        *pu8Len = ubLen + 2;
        pu8Data[0] = *pu8Len - 1;
        pu8Data[1] = GAP_ADTYPE_LOCAL_NAME_COMPLETE;
        MemCopy(pu8Data + 2, "OPL_Device", ubLen);
    }

    return 0;
}

