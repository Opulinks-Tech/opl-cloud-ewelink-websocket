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

#ifndef __APP_CONFIGURATION_H__
#define __APP_CONFIGURATION_H__

#define DAMAGE_EN                    (0)  // 0 : disable , 1 enable

// The ohter parts
/* BUTTON SENSOR Config */
#define APP_CTRL_BUTTON_SENSOR_EN               (1)
#define APP_CTRL_BUTTON_IO_PORT                 GPIO_IDX_04
#define APP_CTRL_BUTTON_PRESS_LEVEL             GPIO_LEVEL_HIGH          // GPIO_LEVEL_HIGH | GPIO_LEVEL_LOW
#define APP_CTRL_BUTTON_TIMEOUT_DEBOUNCE_TIME   (30)      // 30 ms
#define APP_CTRL_BUTTON_PRESS_TIME              (3000)    // ms
#define APP_CTRL_BUTTON_RELEASE_COUNT_TIME      (800)     // ms

/* PIR Config */
#define APP_CTRL_PIR_IO_BLOCK                  GPIO_LEVEL_HIGH
#define APP_CTRL_PIR_IO_EMPTY                  GPIO_LEVEL_LOW
#ifdef __IDR_EN__  //from build config
/* IDR Config */
#define APP_CTRL_IDR_IO_LIGHT                  GPIO_LEVEL_HIGH
#define APP_CTRL_IDR_IO_DARK                   GPIO_LEVEL_LOW
#endif

/* GPIO IO Port */
#ifdef __IDR_EN__  //from build config
#define IDR_IO_PORT                        (GPIO_IDX_03)
#endif
#define BUTTON_IO_PORT                     (GPIO_IDX_04)
#define PIR_IO_PORT                        (GPIO_IDX_03)
#define BATTERY_IO_PORT                    (GPIO_IDX_10)
#define LED_IO_PORT                        (GPIO_IDX_07)
#if(DAMAGE_EN == 1)
#define DOOR_SENSOR_DAMAGE_IO_PORT         (GPIO_IDX_21)
#endif

/* LED time : unit: ms */
#define LED_TIME_BLE_ON_1           (100)
#define LED_TIME_BLE_OFF_1          (400)

#define LED_TIME_AUTOCONN_ON_1      (100)
#define LED_TIME_AUTOCONN_OFF_1     (1900)

#define LED_TIME_OTA_ON             (50)
#define LED_TIME_OTA_OFF            (1950)

#define LED_TIME_ALWAYS_OFF         (0x7FFFFFFF)

#define LED_TIME_TEST_MODE_ON_1       (200)
#define LED_TIME_TEST_MODE_OFF_1      (200)
#define LED_TIME_TEST_MODE_ON_2       (200)
#define LED_TIME_TEST_MODE_OFF_2      (200)
#define LED_TIME_TEST_MODE_ON_3       (200)
#define LED_TIME_TEST_MODE_OFF_3      (200)
#define LED_TIME_TEST_MODE_ON_4       (200)
#define LED_TIME_TEST_MODE_OFF_4      (600)

#define LED_TIME_NOT_CNT_SRV_ON_1   (100)
#define LED_TIME_NOT_CNT_SRV_OFF_1  (100)
#define LED_TIME_NOT_CNT_SRV_ON_2   (100)
#define LED_TIME_NOT_CNT_SRV_OFF_2  (700)

#if 1   // 20200528, Terence change offline led as same as NOT_CNT_SRV
#define LED_TIME_OFFLINE_ON_1   (100)
#define LED_TIME_OFFLINE_OFF_1  (100)
#define LED_TIME_OFFLINE_ON_2   (100)
#define LED_TIME_OFFLINE_OFF_2  (700)
#else
#define LED_TIME_OFFLINE_ON         (1000)
#define LED_TIME_OFFLINE_OFF        (1000)
#endif

#define LED_TIME_SHORT_PRESS_ON     (100)//Goter

#define LED_TIME_BOOT_ON_1          (100)//Goter
#define LED_TIME_BOOT_OFF_1         (100)//Goter
#define LED_TIME_BOOT_ON_2          (100)//Goter

#define LED_MP_NO_ROUTER_ON_1       (100)
#define LED_MP_NO_ROUTER_OFF_1      (1900)
#define LED_MP_NO_SERVER_ON_1       (100)
#define LED_MP_NO_SERVER_OFF_1      (100)
#define LED_MP_NO_SERVER_ON_2       (100)
#define LED_MP_NO_SERVER_OFF_2      (1700)

#define LED_PIR_ONLINE_ON_1         (500)
#define LED_PIR_ONLINE_OFF_1        (500)
#define LED_PIR_OFFLINE_ON_1        (200)
#define LED_PIR_OFFLINE_OFF_1       (200)
#define LED_PIR_OFFLINE_ON_2        (200)
#define LED_PIR_OFFLINE_OFF_2       (200)
#define LED_PIR_MP_ON_1             (1000)
#define LED_PIR_MP_OFF_1            (1000)
#if(DAMAGE_EN == 1)
#define LED_DAMAGE_ONLINE_ON_1      (500)
#define LED_DAMAGE_ONLINE_OFF_1     (500)
#define LED_DAMAGE_OFFLINE_ON_1     (200)
#define LED_DAMAGE_OFFLINE_OFF_1    (200)
#define LED_DAMAGE_OFFLINE_ON_2     (200)
#define LED_DAMAGE_OFFLINE_OFF_2    (200)
#endif

#define LED_ALWAYS_ON               (0x7FFFFFFF)

/* Door Debounce time : unit: ms */
#define PIR_IDR_DEBOUNCE_TIMEOUT        (2000)     // ms
#define NETWORK_STOP_DELAY           (5000)    // ms

#if(DAMAGE_EN == 1)
/* Damage Debounce time : unit: ms */
#define DAMAGE_DEBOUNCE_TIMEOUT      (200)     //ms
#endif

#define MP_WIFI_DEFAULT_SSID         "opulinks_mp"
#define MP_WIFI_SCAN_RETRY_TIMES     (2)

#define MP_BLINKING_NONE             (0)
#define MP_BLINKING_NO_SERVER        (1)
#define MP_BLINKING_NO_ROUTER        (2)

#define FLITER_STRONG_AP_EN     (1)   // 1: enable , 0:disable

#define SYS_CLK_RATE            (SYS_CFG_CLK_22_MHZ)

#define CUS_AUTO_CONNECT_TABLE
#define AUTO_CONNECT_INTERVAL_TABLE_SIZE  (5)
extern uint32_t g_u32AutoConnIntervalTable[];

#endif /* __APP_CONFIGURATION_H__ */

