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

#ifndef __BLEWIFI_COMMON_H__
#define __BLEWIFI_COMMON_H__

#include <stdio.h>
#include <stdint.h>
#include "blewifi_configuration.h"
#include "app_configuration.h"

#include "msg.h"
#include "cmsis_os.h"
#include "event_groups.h"
#if (SNTP_FUNCTION_EN == 1)
#include <time.h>
#endif

//#define BLEWIFI_SHOW_INFO
//#define BLEWIFI_SHOW_DUMP

#ifdef BLEWIFI_SHOW_INFO
#define BLEWIFI_INFO(fmt, ...)      tracer_log(LOG_LOW_LEVEL, fmt, ##__VA_ARGS__)
#define BLEWIFI_WARN(fmt, ...)      tracer_log(LOG_MED_LEVEL, fmt, ##__VA_ARGS__)
#define BLEWIFI_ERROR(fmt, ...)     tracer_log(LOG_HIGH_LEVEL, fmt, ##__VA_ARGS__)
#else
#define BLEWIFI_INFO(fmt, ...)
#define BLEWIFI_WARN(fmt, ...)
#define BLEWIFI_ERROR(fmt, ...)
#endif  // end of BLEWIFI_SHOW_INFO

#ifdef BLEWIFI_SHOW_DUMP
#define BLEWIFI_DUMP(a, b, c)       BleWifi_COM_HexDump(a, b, c)

void BleWifi_COM_HexDump(const char *title, const uint8_t *buf, uint32_t len);
#else
#define BLEWIFI_DUMP(a, b, c)
#endif  // end of BLEWIFI_SHOW_DUMP

//#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
//#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"

#define BLEWIFI_QUEUE_FRONT    1
#define BLEWIFI_QUEUE_BACK     0

#if (SNTP_FUNCTION_EN == 1)
/*
tm Data Structure
+----------+------+---------------------------+-------+
|  Member  | Type |          Meaning          | Range |
+----------+------+---------------------------+-------+
| tm_sec   | int  | seconds after the minute  | 0-61* |
| tm_min   | int  | minutes after the hour    | 0-59  |
| tm_hour  | int  | hours since midnight      | 0-23  |
| tm_mday  | int  | day of the month          | 1-31  |
| tm_mon   | int  | months since January      | 1-12  |
| tm_year  | int  | years since 1900          |       |
| tm_wday  | int  | days since Sunday         | 0-6   |
| tm_yday  | int  | days since January 1      | 0-365 |
| tm_isdst | int  | Daylight Saving Time flag |       |
+----------+------+---------------------------+-------+
*/
void BleWifi_COM_SntpInit(void);
void BleWifi_COM_SntpTimestampSync(uint32_t u32Timestamp);
void BleWifi_COM_SntpDateTimeGet(struct tm *pSystemTime, float fTimeZone);
time_t BleWifi_COM_SntpTimestampGet(void);
#endif

void BleWifi_COM_RFPowerSetting(uint8_t level);

uint8_t BleWifi_COM_EventCreate(EventGroupHandle_t *tEventGroup);
void BleWifi_COM_EventStatusSet(EventGroupHandle_t tEventGroup , uint32_t dwEventBit , uint8_t status);
uint8_t BleWifi_COM_EventStatusGet(EventGroupHandle_t tEventGroup , uint32_t dwEventBit);
uint8_t BleWifi_COM_EventStatusWait(EventGroupHandle_t tEventGroup , uint32_t dwEventBit , uint32_t millisec);

#endif  // end of __BLEWIFI_COMMON_H__
