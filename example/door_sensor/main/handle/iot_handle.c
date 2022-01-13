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

#include "iot_handle.h"
#include "iot_data.h"
#include "iot_configuration.h"
#include "cloud_http.h"
#include "httpclient.h"
#include "scrt_patch.h"
#include "mw_ota.h"
#include "wifi_api.h"
#include "driver_netlink.h"
#include "blewifi_wifi_api.h"
#include "mw_fim_default_group12_project.h"
#include "iot_ota_http.h"
#include "sensor_common.h"
#include "app_ctrl.h"
#include "sensor_battery.h"
#include "infra_cjson.h"
#include "coolkit_websocket.h"
#include "etharp.h"
//#include "blewifi_common.h"

extern EventGroupHandle_t g_tAppCtrlEventGroup;
extern IoT_Ring_buffer_t g_stIotRbData;
extern IoT_Ring_buffer_t g_stIotRbData;


char OTA_FULL_URL[256] = {0};

extern osTimerId    g_tAppPostFailLedTimeOutTimerId;
extern osTimerId    g_tmr_req_date;

extern T_MwFim_GP12_HttpPostContent g_tHttpPostContent;
extern T_MwFim_GP12_HttpHostInfo g_tHostInfo;

float g_fBatteryVoltage = 0;

extern EventGroupHandle_t g_tAppCtrlEventGroup;

#if CLOUD_HTTP_POST_ENHANCEMENT
    httpclient_t g_client = {0};
#else
    //httpclient_t client = {0};
#endif

#define PROPERTY_PAYLOAD_SIZE_300 (300)

void Iot_Handle_KeepAlive(void)
{
    ws_KeepAlive();
}

void UpdateBatteryContent(void)
{
    int i = 0;
    float fVBatPercentage = 0;

    for (i = 0 ;i < SENSOR_MOVING_AVERAGE_COUNT ;i++)
    {
        fVBatPercentage = Sensor_Auxadc_VBat_Get();
    }
    g_fBatteryVoltage = fVBatPercentage;
}

void Post_to_Cloud_Directly(int8_t *u8pProperty_Payload, uint32_t u32Offset)
{
    IoT_Properity_t IoT_Properity;
    uint32_t ulTotalBodyLen=0;
    char szBodyFmt[BUFFER_SIZE]={0};
    uint8_t u8IsDateReq=0;
    //blewifi_wifi_set_dtim_t stSetDtim = {0};

//    printf("Post to Cloud:%s\n", u8pProperty_Payload);

    if( false == BleWifi_COM_EventStatusGet(g_tIotDataEventGroup , IOT_DATA_EVENT_BIT_CLOUD_CONNECTED))
    {
//        printf("Cloud is disconnect, no Post\n");
        return;
    }

//    if(strstr((char *)u8pProperty_Payload, "date")!=0)
//    {
//        u8IsDateReq = 1;
//    }

    memset(szBodyFmt, 0, sizeof(szBodyFmt));

    ws_encode(szBodyFmt, &ulTotalBodyLen, (char *)u8pProperty_Payload, u32Offset);  // build the websocket data packet with header and encrypt key

    osSemaphoreWait(g_tAppSemaphoreId, osWaitForever);
    if(g_tcp_hdl_ID == -1)
    {
        osSemaphoreRelease(g_tAppSemaphoreId);
        return;
    }

    if(g_tx_ID!=g_tcp_hdl_ID)
    {
        g_tx_ID = g_tcp_hdl_ID;
    }
    osSemaphoreRelease(g_tAppSemaphoreId);

#if 0
    stSetDtim.u32DtimValue = 0;
    stSetDtim.u32DtimEventBit = BW_WIFI_DTIM_EVENT_BIT_TX_USE;
    BleWifi_Wifi_Set_Config(BLEWIFI_WIFI_SET_DTIM , (void *)&stSetDtim);
#endif

    int ret = ws_write(szBodyFmt, ulTotalBodyLen);
    if(ret>0){
       if(0 == u8IsDateReq)
           printf("[ATS]WIFI Send Reply success(%llu, %d)\r\n", g_msgid, strstr((char *)u8pProperty_Payload, "date"));
    }
    else
    {
        if(0 == u8IsDateReq)
        {
            printf("[ATS]WIFI Send Reply fail(%llu)\r\n", g_msgid);
        }
        else
        {
            printf("[ATS]Req Date Failed(%llu)\r\n", g_msgid);
        }


        if(IoT_Properity.ubData!=NULL)
            free(IoT_Properity.ubData);

        osSemaphoreWait(g_tAppSemaphoreId, osWaitForever);
        osTimerStop(g_tmr_req_date);

        if (true == BleWifi_COM_EventStatusGet(g_tIotDataEventGroup, IOT_DATA_EVENT_BIT_WAITING_RX_RSP))
        {
            if (IOT_DATA_WAITING_TYPE_DATA_POST == g_u8WaitingRspType)
            {
                printf("[ATS]WIFI Send data fail(%llu)\r\n", g_msgid);
            }
        }
        osTimerStop(g_iot_tx_wait_timeout_timer);
        g_u8WaitingRspType = IOT_DATA_WAITING_TYPE_NONE;
        BleWifi_COM_EventStatusSet(g_tIotDataEventGroup, IOT_DATA_EVENT_BIT_WAITING_RX_RSP, false);

        if(((g_tcp_hdl_ID!=-1)
            &&(g_tx_ID == g_tcp_hdl_ID))
            &&(true == BleWifi_COM_EventStatusGet(g_tIotDataEventGroup , IOT_DATA_EVENT_BIT_CLOUD_CONNECTED)))
        {
            ret=ws_close();
            printf("[ATS]Cloud disconnect\r\n");
            printf("wt: ws_close(ret=%d)\n", ret);
            g_tx_ID = -1;
            g_tcp_hdl_ID = -1;
            BleWifi_COM_EventStatusSet(g_tIotDataEventGroup, IOT_DATA_EVENT_BIT_CLOUD_CONNECTED, false);
            Iot_Data_TxTaskEvtHandler_CloudConnectRetry();
            osSemaphoreRelease(g_tAppSemaphoreId);
        }
        else
        {
           osSemaphoreRelease(g_tAppSemaphoreId);
        }
    }

    // Update Battery voltage for post data
    UpdateBatteryContent();

#if 0
    stSetDtim.u32DtimValue = BleWifi_Wifi_GetDtimSetting();
    stSetDtim.u32DtimEventBit = BW_WIFI_DTIM_EVENT_BIT_TX_USE;
    BleWifi_Wifi_Set_Config(BLEWIFI_WIFI_SET_DTIM , (void *)&stSetDtim);
#endif
}

void Iot_Handle_Send_CloudRsp(void)
{
    IoT_Properity_t tProperity = {0};
    blewifi_wifi_set_dtim_t stSetDtim = {0};

    stSetDtim.u32DtimValue = 0;
    stSetDtim.u32DtimEventBit = BW_WIFI_DTIM_EVENT_BIT_TX_CLOUD_ACK_POST;
    BleWifi_Wifi_Set_Config(BLEWIFI_WIFI_SET_DTIM , (void *)&stSetDtim);

    lwip_one_shot_arp_enable();

    if (IOT_RB_DATA_OK == IoT_Ring_Buffer_Pop(&g_stCloudRspQ, &tProperity))
    {
        Post_to_Cloud_Directly((int8_t *)tProperity.ubData, tProperity.ulLen);

        osTimerStop(g_iot_tx_ack_post_timeout_timer);
        osTimerStart(g_iot_tx_ack_post_timeout_timer, CLOUD_ACK_POST_TIMEOUT);

        //remove from Q
        IoT_Ring_Buffer_ReadIdxUpdate(&g_stCloudRspQ);
        if(tProperity.ubData != NULL)
        {
            free(tProperity.ubData);
        }
    }
}

/**
  * @brief Convert date to Unix timestamp
  * @param[in] date Pointer to a structure representing the date and time
  * @return Unix timestamp
  **/

 time_t convertDateToUnixTime(const DateTime *date)
 {
    uint16_t y;
    uint16_t m;
    uint16_t d;
    uint32_t t;

    //Year
    y = date->year;
    //Month of year
    m = date->month;
    //Day of month
    d = date->day;

    //January and February are counted as months 13 and 14 of the previous year
    if(m <= 2)
    {
       m += 12;
       y -= 1;
    }

    //Convert years to days
    t = (365 * y) + (y / 4) - (y / 100) + (y / 400);
    //Convert months to days
    t += (30 * m) + (3 * (m + 1) / 5) + d;
    //Unix time starts on January 1st, 1970
    t -= 719561;
    //Convert days to seconds
    t *= 86400;
    //Add hours, minutes and seconds
    t += (3600 * date->hours) + (60 * date->minutes) + date->seconds;

    printf("tday = %u\n\n",t);

    //Return Unix time
    return t;
}


int Sensor_Sha256_Value_OTA(time_t TimeStamp, uint8_t ubaSHA256CalcStrBuf[])
{
    int len = 0;
    unsigned char uwSHA256_Buff[BUFFER_SIZE_128] = {0};

    /* Combine https Post key */
    memset(uwSHA256_Buff, 0, BUFFER_SIZE_128);
    len = sprintf((char *)uwSHA256_Buff, SHA256_FOR_OTA_FORMAT, g_tHttpPostContent.ubaDeviceId, TimeStamp, g_tHttpPostContent.ubaApiKey);

    Sensor_SHA256_Calc(ubaSHA256CalcStrBuf, len, uwSHA256_Buff);

    return COOLLINK_WS_RES_OK;

}

void Iot_Post_Data_Construct(IoT_Properity_t *ptProperity , uint8_t *pu8Data, uint32_t *pu32Len)
{
    uint32_t u32Offset = 0;
    uint8_t already_wrt_flag = 0;
    char s8aPayload[512] = {0};
    char *ps8Buf = s8aPayload;

    uint16_t uwProjectId=0;
    uint16_t uwChipId=0;
    uint16_t uwFirmwareId=0;
    float fVBatPercentage = 0;
    char FwVersion[24]={0};
    uint8_t ubaMacAddr[6]={0};
    int8_t rssi = 0;

    Sensor_Data_t *pstSensorData = NULL;

    pstSensorData = (Sensor_Data_t *)ptProperity->ubData;

    if(pstSensorData == NULL)
    {
        printf("[ERR]pstSensorData is NULL\r\n");
        return;
    }

    /* WiFi MAC */
    wifi_config_get_mac_address(WIFI_MODE_STA, ubaMacAddr);

    if(0 == pstSensorData->fVBatPercentage) //Has the battery been updated?
    {
        fVBatPercentage = g_fBatteryVoltage;

        if(fVBatPercentage > BATTERY_HIGH_VOL)
        {
            fVBatPercentage = 100;
        }
        else if(fVBatPercentage < BATTERY_LOW_VOL)
        {
            fVBatPercentage = 0;
        }
        else
        {
            fVBatPercentage = (fVBatPercentage - BATTERY_LOW_VOL) *100 / (BATTERY_HIGH_VOL - BATTERY_LOW_VOL);
        }
        pstSensorData->fVBatPercentage = fVBatPercentage;  // update battery
    }
    else
    {
        fVBatPercentage = pstSensorData->fVBatPercentage;
    }

    /* WiFi RSSI */
    if(0 == pstSensorData->rssi) //Has the rssi been updated?
    {
        printf(" Original RSSI is %d \n", wpa_driver_netlink_get_rssi());
        rssi = wpa_driver_netlink_get_rssi() + BLEWIFI_COM_RF_RSSI_ADJUST;
        pstSensorData->rssi = rssi; // update rssi
    }
    else
    {
        rssi = pstSensorData->rssi;
    }

    u32Offset = sprintf( ps8Buf, "{");

    if(g_IsInitPost)
    {

        u32Offset += sprintf( ps8Buf +u32Offset, "\"init\":1");
        already_wrt_flag = 1;

        MwOta_VersionGet(&uwProjectId, &uwChipId, &uwFirmwareId);

        memset(FwVersion, 0x00, sizeof(FwVersion));

        sprintf(FwVersion, CKS_FW_VERSION_FORMAT, uwProjectId, uwChipId, uwFirmwareId);

        if (already_wrt_flag)
            u32Offset += sprintf( ps8Buf +u32Offset, ",");
        u32Offset += sprintf( ps8Buf +u32Offset, "\"fwVersion\":\"%s\"", FwVersion);
        already_wrt_flag = 1;

        if (already_wrt_flag)
            u32Offset += sprintf( ps8Buf +u32Offset, ",");
        u32Offset += sprintf( ps8Buf +u32Offset, POST_DATA_MACADDRESS_FORMAT,
        ubaMacAddr[0],
        ubaMacAddr[1],
        ubaMacAddr[2],
        ubaMacAddr[3],
        ubaMacAddr[4],
        ubaMacAddr[5]);

        already_wrt_flag = 1;


        if (already_wrt_flag)
            u32Offset += sprintf( ps8Buf +u32Offset, ",");
        u32Offset += sprintf( ps8Buf +u32Offset, "\"chipID\":\"%d\"", uwChipId);
        already_wrt_flag = 1;


        g_IsInitPost = 0;
    }




//    if( pstSensorData->ubaType==2 || pstSensorData->ubaType==3 )
    {
        if (already_wrt_flag)
            u32Offset += sprintf( ps8Buf +u32Offset, ",");
        u32Offset += sprintf( ps8Buf +u32Offset, "\"type\":\"%d\"", pstSensorData->ubaType);
        already_wrt_flag = 1;

        if (already_wrt_flag)
            u32Offset += sprintf( ps8Buf +u32Offset, ",");
        u32Offset += sprintf( ps8Buf +u32Offset, "\"switch\":\"%s\"", pstSensorData->ubaDoorStatus?"off":"on");
        already_wrt_flag = 1;

//        struct tm *pSystemTime;
//        float fTimeZone = 0;
//        char szW3cBuf[50] = {0};
//
//        BleWifi_COM_SntpDateTimeGet(pSystemTime, fTimeZone);
//        sprintf(szW3cBuf, "%d-%d-%dT%d:%d:%dZ\n", pSystemTime->tm_year, pSystemTime->tm_mon, pSystemTime->tm_mday, pSystemTime->tm_hour, pSystemTime->tm_min, pSystemTime->tm_sec);
//
//        if (already_wrt_flag)
//            u32Offset += sprintf( ps8Buf +u32Offset, ",");
//        u32Offset += sprintf( ps8Buf +u32Offset, "\"actionTime\":\"%s\"", szW3cBuf);
//        already_wrt_flag = 1;
    }

    if (already_wrt_flag)
        u32Offset += sprintf( ps8Buf +u32Offset, ",");
    u32Offset += sprintf( ps8Buf +u32Offset, "\"rssi\":\"%d\"", rssi);
    already_wrt_flag = 1;

    if (already_wrt_flag)
        u32Offset += sprintf( ps8Buf +u32Offset, ",");
    u32Offset += sprintf( ps8Buf +u32Offset, "\"battery\":\"%d\"", (int)fVBatPercentage);
    already_wrt_flag = 1;

    u32Offset += sprintf( ps8Buf + u32Offset, "}");

    if(strlen(ps8Buf)<5) return;

    printf("Device Act Update: %s\n", ps8Buf);

    //g_msgid = Coolkit_Cloud_GetNewSeqID();
    g_msgid = pstSensorData->u64TimeStamp;

    *pu32Len = sprintf( (char *)pu8Data, COOLLINK_WS_UPDATE_BODY_WITH_PARAMS,
    g_tHttpPostContent.ubaDeviceId,
    g_Apikey,
    g_msgid,
    ps8Buf);



}

int8_t Iot_Contruct_Post_Data_And_Send(IoT_Properity_t *ptProperity)
{
    uint8_t u8aPayload[BUFFER_SIZE]={0};
    uint32_t u32PayloadLen =0;
    int8_t s8Ret=0;

    if(true == BleWifi_COM_EventStatusGet(g_tIotDataEventGroup, IOT_DATA_EVENT_BIT_CLOUD_CONNECTED))
    {

    //  printf("----------\nPayload for WS Send:(%d)\n----------\n%s\n", IoT_Properity.ulLen, IoT_Properity.ubData);

        Iot_Post_Data_Construct(ptProperity, &u8aPayload[0] , &u32PayloadLen);
        if(g_u8PostRetry_IotRbData_Cnt == 0)
        {
            printf("[ATS]WIFI Send data start(%llu)\r\n", g_msgid);
        }
        s8Ret = Coolkit_Cloud_Send(u8aPayload, u32PayloadLen);

        // Update Battery voltage for post data
        UpdateBatteryContent();
    }

    return s8Ret;
}

coollink_ws_result_t Coollink_ws_process_update(uint8_t *szOutBuf, uint16_t out_length, uint8_t u8IsNeedToPost)
{
    lite_cjson_t tRoot = {0};
    lite_cjson_t tSequence = {0};
    lite_cjson_t tParams= {0};
//    lite_cjson_t tLocal= {0};
    lite_cjson_t tDefense = {0};
    lite_cjson_t tEnable = {0};
    lite_cjson_t tSwitch = {0};
    lite_cjson_t tTime = {0};
    lite_cjson_t tholdReminder = {0};

    uint8_t sSeqID[32] = {0};
    uint32_t u32Offset = 0;
    int8_t s8Enable =-1;
    int8_t s8Switchon = -1;
    int16_t s16Time = -1;
    IoT_Properity_t stCloudRsp = {0};

    /* Parse Request */
    if (lite_cjson_parse((char*)szOutBuf, out_length, &tRoot)) {
        printf("JSON Parse Error\n");
        return COOLLINK_WS_RES_ERR;
    }

    if (!lite_cjson_object_item(&tRoot, "sequence", 8, &tSequence)) {

        if(tSequence.value_length < 32)
        {
            memcpy(sSeqID, tSequence.value, tSequence.value_length);
            sSeqID[tSequence.value_length] = 0;
        }
        uint64_t u64TimestampMs = 0;
        u64TimestampMs = strtoull((char*)sSeqID, NULL, 10);

        g_msgid = u64TimestampMs;
        //printf("ORG SeqID: %llu\r\n", g_msgid);
    }

    if (!lite_cjson_object_item(&tRoot, "params", 6, &tParams))
    {

       if (!lite_cjson_object_item(&tParams, "defense", 7, &tDefense)) {

            uint8_t u8Defense = 0;
            u8Defense = tDefense.value_int;

//            printf("defense: %d\r\n", u8Defense);

            App_Door_Sensor_Set_Defense(u8Defense);
        }

        if (!lite_cjson_object_item(&tParams, "holdReminder", 12, &tholdReminder)) {


            if (!lite_cjson_object_item(&tholdReminder, "enabled", 7, &tEnable)) {
                s8Enable = tEnable.value_int;
    //            printf("enable: %d\r\n", s8Enable);
             }

            if (!lite_cjson_object_item(&tholdReminder, "switch", 6, &tSwitch)) {

                if((tSwitch.value_length==3)&&strncmp(tSwitch.value,"off",3)==0)
                {
                    s8Switchon = DOOR_WARING_FLAG_CLOSE;
                }
                if((tSwitch.value_length==2)&&strncmp(tSwitch.value,"on",2)==0)
                {
                    s8Switchon = DOOR_WARING_FLAG_OPEN;
                }
        //            printf("Switchon: %d\r\n", s8Switchon);
            }

            if (!lite_cjson_object_item(&tholdReminder, "time", 4, &tTime)) {

                s16Time = tTime.value_int;
    //            printf("Time: %d\r\n", s16Time);
            }

            if((-1!=s8Enable) && (-1!=s8Switchon) && (-1!=s16Time))
            {
                T_MwFim_GP17_Hold_Reminder pstHoldReminder;
                pstHoldReminder.u8Enable = s8Enable;
                pstHoldReminder.u8Switch = s8Switchon;
                pstHoldReminder.u16Time = s16Time;

                printf("Enable=%d, Switch=%d, Time=%d\n", s8Enable, s8Switchon, s16Time);
                App_Ctrl_MsgSend(APP_CTRL_MSG_DOOR_SET_HOLD_REMIDER, (uint8_t *)&pstHoldReminder, sizeof(T_MwFim_GP17_Hold_Reminder));
            }
        }

    }

    stCloudRsp.ubData = malloc(PROPERTY_PAYLOAD_SIZE_300);
    if(stCloudRsp.ubData == NULL)
    {
        printf("ubData malloc fail\r\n");
        return COOLLINK_WS_RES_ERR;
    }

    u32Offset = sprintf((char *)stCloudRsp.ubData , COOLLINK_WS_REPLY_BODY,
    0,
    g_tHttpPostContent.ubaDeviceId,
    g_Apikey,
    sSeqID
    );

    stCloudRsp.ulLen = u32Offset;

    if(IOT_RB_DATA_OK == IoT_Ring_Buffer_Push(&g_stCloudRspQ, &stCloudRsp))
    {
        Iot_Data_TxTask_MsgSend(IOT_DATA_TX_MSG_CLOUD_POST, NULL, 0);
    }
    else
    {
        printf("rb push fail\r\n");
        free(stCloudRsp.ubData);
    }

    return COOLLINK_WS_RES_OK;
}


coollink_ws_result_t Coollink_ws_process_upgrade(uint8_t *szOutBuf, uint16_t out_length, uint8_t u8IsNeedToPost)
{
    printf("upgrade\n");

    int length;
    char URL[128] = {0};
    const char *PosStart = (const char *)szOutBuf;
    const char *PosEnd;
    const char *NeedleStart = "downloadUrl";
    const char *NeedleEnd = "\",\"";


    if ((PosStart=strstr(PosStart,NeedleStart)) != NULL)
    {
        // Assign String to another
        PosEnd = PosStart;
        // Search the match string
        if ((PosEnd=strstr(PosEnd, NeedleEnd)) != NULL)
        {
            // Calcuate the length
            length = PosEnd - PosStart;

            memset(URL, '\0', sizeof(URL));
            // Copy string to destination string
            strncpy(URL, (PosStart + (strlen(NeedleStart) + strlen(NeedleEnd))), (length - (strlen(NeedleStart) + strlen(NeedleEnd))));


            uint8_t ubaSHA256CalcStrBuf[SCRT_SHA_256_OUTPUT_LEN];

            uint32_t u32TimeStamp = 0; // current seconds of today (based on time zone of current DevSched)

//            int32_t s32TimeZoneSec = 8*3600;


            u32TimeStamp = BleWifi_COM_SntpTimestampGet();

//            u32TimeStamp = BleWifi_SntpGetRawData(s32TimeZoneSec);

            if (1 == Sensor_Sha256_Value_OTA(u32TimeStamp, ubaSHA256CalcStrBuf))
            {
                printf("\n SENSOR_DATA_FAIL \n");
                return COOLLINK_WS_RES_ERR;
            }

            int iOffset = 0;
            char baSha256Buf[68] = {0};

            for(int i = 0; i < SCRT_SHA_256_OUTPUT_LEN; i++)
            {
                iOffset += snprintf(baSha256Buf + iOffset, sizeof(baSha256Buf), "%02x", ubaSHA256CalcStrBuf[i]);
            }

            memset(OTA_FULL_URL,0x00, sizeof(OTA_FULL_URL));
            sprintf(OTA_FULL_URL, OTA_DATA_URL, URL, g_tHttpPostContent.ubaDeviceId, u32TimeStamp, baSha256Buf);

//            BleWifi_COM_EventStatusSet(BLEWIFI_CTRL_EVENT_BIT_OTA_MODE, true);

            printf("OTA_DATA_URL: "OTA_DATA_URL"\n\n", URL, g_tHttpPostContent.ubaDeviceId, u32TimeStamp, baSha256Buf);

//            if(true == BleWifi_COM_EventStatusGet(APP_CTRL_EVENT_BIT_OTA_MODE))
            {
                printf("Will Do OTA .....\n");
                printf("\n OTA_FULL_URL = %s\n",OTA_FULL_URL);
                IoT_OTA_HTTP_TrigReq((uint8_t *)OTA_FULL_URL);
            }
       }
    }

    return COOLLINK_WS_RES_OK;
}


coollink_ws_result_t Coollink_ws_process_error(uint8_t *szOutBuf, uint16_t out_length, uint8_t u8IsNeedToPost)
{
    lite_cjson_t tRoot = {0};
    lite_cjson_t tError = {0};
    lite_cjson_t tDate = {0};
    uint64_t result = 0;
    uint8_t IsPrntPostRlt = 1;
    blewifi_wifi_set_dtim_t stSetDtim = {0};
    IoT_Properity_t ptProperity  = {0};

    /* Parse Request */
    if (lite_cjson_parse((char*)szOutBuf, out_length, &tRoot)) {
        printf("JSON Parse Error\n");
        return COOLLINK_WS_RES_ERR;
    }
//    printf("Cloud Resp Data:%s\n", szOutBuf);
    if (!lite_cjson_object_item(&tRoot, "error", 5, &tError))
    {
        printf("Cloud Resp errorcode: %d\r\n", tError.value_int);
    }

    if (!lite_cjson_object_item(&tRoot, "date", 4, &tDate))
    {
//        printf("Cloud Resp Date:%s\n", tDate.value);
        int mon=0,d=0,y=0,h,min,s;
        sscanf(tDate.value,"%d-%d-%dT%d:%d:%d*",&y,&mon,&d,&h,&min,&s);
        printf("Cloud Rsp Date: %d %d %d %d %d %d\r\n",y,mon,d,h,min,s);
        IsPrntPostRlt = 0;

        DateTime date;

        date.year=y;
        date.month=mon;
        date.day=d;
        date.hours=h;
        date.minutes=min;
        date.seconds=s;

        uint32_t u32Timestamp = 0;
        u32Timestamp = convertDateToUnixTime(&date);

        BleWifi_COM_SntpTimestampSync(u32Timestamp);

//        BleWifi_Ctrl_MsgSend(BLEWIFI_CTRL_MSG_DEV_SCHED_START, NULL, 0);

    }

    lite_cjson_t tSequence = {0};
    char sBuf[32] = {0};
    if (!lite_cjson_object_item(&tRoot, "d_seq", 5, &tSequence)) {

        if(tSequence.value_length < 32)
        {
            memcpy(sBuf, tSequence.value, tSequence.value_length);
            sBuf[tSequence.value_length] = 0;
        }
        result = strtoull((char*)sBuf, NULL, 10);

        if( tError.value_int == 0 && g_msgid == result)
        {
            g_msgid = 0; //reset
#ifdef __SONOFF__
            osTimerStop(g_tAppPostFailLedTimeOutTimerId);
            BleWifi_COM_EventStatusSet(g_tAppCtrlEventGroup, APP_CTRL_EVENT_BIT_NOT_CNT_SRV, false);
            App_Ctrl_LedStatusChange();
#endif

            osTimerStop(g_iot_tx_wait_timeout_timer);
            BleWifi_COM_EventStatusSet(g_tIotDataEventGroup, IOT_DATA_EVENT_BIT_WAITING_RX_RSP, false);

            if(IOT_DATA_WAITING_TYPE_KEEPALIVE == g_u8WaitingRspType)
            {
                if (IOT_RB_DATA_OK != IoT_Ring_Buffer_CheckEmpty(&g_stKeepAliveQ))
                {
                    IoT_Ring_Buffer_Pop(&g_stKeepAliveQ , &ptProperity);
                    IoT_Ring_Buffer_ReadIdxUpdate(&g_stKeepAliveQ);

                    if(ptProperity.ubData!=NULL)
                        free(ptProperity.ubData);
                }
                osSemaphoreWait(g_tAppSemaphoreId, osWaitForever);
                g_u8PostRetry_KeepAlive_Cnt = 0;
                g_u8PostRetry_KeepAlive_Fail_Round = 0;
                osSemaphoreRelease(g_tAppSemaphoreId);
            }
            else if(IOT_DATA_WAITING_TYPE_DATA_POST == g_u8WaitingRspType)
            {
                // if clear the flag for last post retry after keep alive
                BleWifi_COM_EventStatusSet(g_tIotDataEventGroup, IOT_DATA_EVENT_BIT_LAST_POST_RETRY, false);

                if (IOT_RB_DATA_OK != IoT_Ring_Buffer_CheckEmpty(&g_stIotRbData))
                {
                    IoT_Ring_Buffer_Pop(&g_stIotRbData , &ptProperity);
                    IoT_Ring_Buffer_ReadIdxUpdate(&g_stIotRbData);

                    if(ptProperity.ubData!=NULL)
                        free(ptProperity.ubData);
                }
                printf("post cnt = %u success\r\n" , (g_u8PostRetry_IotRbData_Cnt + 1));

                osSemaphoreWait(g_tAppSemaphoreId, osWaitForever);
                g_u8PostRetry_IotRbData_Cnt = 0;
                osSemaphoreRelease(g_tAppSemaphoreId);
            }

            osSemaphoreWait(g_tAppSemaphoreId, osWaitForever);
            g_u8WaitingRspType = IOT_DATA_WAITING_TYPE_NONE;
            osSemaphoreRelease(g_tAppSemaphoreId);

            if(1 == IsPrntPostRlt)
            {
                printf("[ATS]WIFI Send Data success(%llu)\r\n", result);
            }

            Iot_Data_TxTask_MsgSend(IOT_DATA_TX_MSG_CLOUD_POST, NULL, 0);

            stSetDtim.u32DtimValue = BleWifi_Wifi_GetDtimSetting();
            stSetDtim.u32DtimEventBit = BW_WIFI_DTIM_EVENT_BIT_TX_USE;
            BleWifi_Wifi_Set_Config(BLEWIFI_WIFI_SET_DTIM , (void *)&stSetDtim);
        }
        else if(g_msgid != result)
        {
            printf("[ATS]WIFI post fail(%llu)\r\n", result);
        }
    }

    if(0!=tError.value_int)
    {
        return COOLLINK_WS_RES_ERR;
    }

    return COOLLINK_WS_RES_OK;
}

int8_t Iot_Data_Parser(uint8_t *szOutBuf, uint16_t out_length)
{

    //1. handle parser
    char szDecodeBuf[BUFFER_SIZE] = {0};
    int Decode_len=0;

    ws_decode((unsigned char*)szOutBuf, out_length, (unsigned char*)szDecodeBuf, BUFFER_SIZE , &Decode_len);

    if(0!=Decode_len)
        printf("[ATS]WIFI Rcv data success(len=%d)\npayload:%s\n", Decode_len, szDecodeBuf);

    if(strstr(szDecodeBuf, "action")!=0){
        lite_cjson_t tRoot = {0};
        lite_cjson_t tUpdate = {0};
        char sBuf[32] = {0};
        const coollink_ws_command_t *cmd;

        /* Parse Request */
        if (!lite_cjson_parse(szDecodeBuf, out_length, &tRoot)) {

            if (!lite_cjson_object_item(&tRoot, "action", 6, &tUpdate)) {

                if(tUpdate.value_length < 32)
                {
                    memcpy(sBuf, tUpdate.value, tUpdate.value_length);
                    sBuf[tUpdate.value_length] = 0;
                }
            }

            for (int i = 0; coollink_ws_action_commands[i].pattern!= NULL; i++) {
                cmd = &coollink_ws_action_commands[i];
                if (strcmp(cmd->pattern, sBuf)==0) {
                    coollink_ws_result_t Reslt = cmd->callback((uint8_t*)szDecodeBuf, Decode_len, false);

                }
            }
        }
        else
            printf("JSON Parse Error\n");

    }
    else if(strstr(szDecodeBuf, "error")!=0)
    {
        coollink_ws_result_t Reslt = Coollink_ws_process_error((uint8_t*)szDecodeBuf, Decode_len, false);
    }

    return 0;
}

void Coolkit_Req_Date(void const *argu)
{
    Iot_Data_TxTask_MsgSend(IOT_DATA_TX_MSG_CLOUD_KEEP_ALIVE, NULL, 0);
}
