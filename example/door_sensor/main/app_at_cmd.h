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

#ifndef __APP_AT_CMD_H__
#define __APP_AT_CMD_H__

#include "cmsis_os.h"
#include "mw_fim_default_group16_project.h"
#include "mw_fim_default_group12_project.h"
#include "wifi_types.h"


typedef struct
{
    char ubaDeviceId[DEVICE_ID_LEN];
    char ubaApiKey[API_KEY_LEN];
    char ubaWifiMac[CHIP_ID_LEN];
    char ubaBleMac[CHIP_ID_LEN];
    char ubaDeviceModel[MODEL_ID_LEN];
} T_SendJSONParam;

int Write_data_into_fim(T_SendJSONParam *DeviceData);
void app_at_cmd_add(void);

/* For Check strength of wifi */
#define MAX_WIFI_PASSWORD_LEN                           (64)




#endif /* __APP_AT_CMD_H__ */

