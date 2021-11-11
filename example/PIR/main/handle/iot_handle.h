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

#ifndef __IOT_HANDLE_H__
#define __IOT_HANDLE_H__

#include <stdint.h>
#include <stdbool.h>
#include "iot_rb_data.h"
#include "iot_configuration.h"
#include <time.h> // This is for time_t

#define BUFFER_SIZE                 768

#define POST_DATA_MACADDRESS_FORMAT "\"mac\":\"%02x%02x%02x%02x%02x%02x\""
#define OTA_DATA_URL "%s?deviceid=%s&ts=%u&sign=%s"
#define SHA256_FOR_OTA_FORMAT "%s%u%s"
#define CKS_FW_VERSION_FORMAT "%d.%d.%03d"


#define BUFFER_SIZE_128 128

#define CLOUD_ACK_POST_TIMEOUT   (1500)  //ms

enum _coollink_ws_result_t {
    COOLLINK_WS_RES_OK = 0,
    COOLLINK_WS_RES_ERR = -1
};
typedef enum _coollink_ws_result_t coollink_ws_result_t;

typedef coollink_ws_result_t(*coollink_ws_command_callback_t)(uint8_t *szOutBuf, uint16_t out_length, uint8_t u8IsNeedToPost);

struct _coollink_ws_command_t {
    const char *pattern;
    coollink_ws_command_callback_t callback;
};

typedef struct _coollink_ws_command_t coollink_ws_command_t;

coollink_ws_result_t Coollink_ws_process_update(uint8_t *szOutBuf, uint16_t out_length, uint8_t u8IsNeedToPost);
coollink_ws_result_t Coollink_ws_process_upgrade(uint8_t *szOutBuf, uint16_t out_length, uint8_t u8IsNeedToPost);
coollink_ws_result_t Coollink_ws_process_error(uint8_t *szOutBuf, uint16_t out_length, uint8_t u8IsNeedToPost);

static const coollink_ws_command_t coollink_ws_action_commands[] = {

    { .pattern = "update",      .callback = Coollink_ws_process_update},
    { .pattern = "upgrade",     .callback = Coollink_ws_process_upgrade},
};

typedef struct
{
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t dayOfWeek;
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
    uint16_t milliseconds;
} DateTime;

typedef struct
{
    uint8_t u8aUrl[128];
    uint8_t u8aSendBuf[1024];
    uint16_t u16SendBufLen;
} xIotDataPostInfo_t;

typedef struct {
  time_t TimeStamp;
  uint8_t DoorStatus;
  uint8_t ContentType;
  uint8_t ubaMacAddr[6];
  char Battery[8];
  int rssi;
  char FwVersion[24];
} HttpPostData_t;

void Iot_Handle_KeepAlive(void);
void Iot_Handle_Send_CloudRsp(void);
int8_t Iot_Contruct_Post_Data_And_Send(IoT_Properity_t *ptProperity);
int8_t Iot_Data_Parser(uint8_t *szOutBuf, uint16_t out_length);
void Coolkit_Req_Date(void const *argu);
void UpdateBatteryContent(void);

#endif // __IOT_HANDLE_H__
