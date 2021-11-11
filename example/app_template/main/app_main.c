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
 * @file app_main.c
 * @author Terence Yang
 * @date 6 Oct 2020
 * @brief File creates the app task architecture.
 *
 */

#include "blewifi_configuration.h"
#include "blewifi_common.h"
#include "blewifi_wifi_api.h"
#include "blewifi_ble_api.h"
#include "app_main.h"
#include "app_ctrl.h"
#include "iot_data.h"
#include "sys_common_api.h"
#include "sys_common_api_patch.h"
#include "mw_fim_default_group03.h"
#include "mw_fim_default_group03_patch.h"
#include "app_at_cmd.h"
#include "iot_configuration.h"
#include "iot_ota_http.h"
#include "at_cmd_task_patch.h"
#ifdef __BLEWIFI_TRANSPARENT__
#include "hal_pin_config_project_transparent.h"
#else
#include "hal_pin_config_project.h"
#endif
#include "hal_dbg_uart.h"
#include "hal_uart.h"

void AppInit(void)
{
    T_MwFim_SysMode tSysMode;

    // get the settings of system mode
	if (MW_FIM_OK != MwFim_FileRead(MW_FIM_IDX_GP03_PATCH_SYS_MODE, 0, MW_FIM_SYS_MODE_SIZE, (uint8_t*)&tSysMode))
    {
        // if fail, get the default value
        memcpy(&tSysMode, &g_tMwFimDefaultSysMode, MW_FIM_SYS_MODE_SIZE);
    }

    if ((tSysMode.ubSysMode == MW_FIM_SYS_MODE_MP) && (HAL_PIN_0_1_UART_MODE == IO01_UART_MODE_DBG))
    {
        at_cmd_switch_uart1_dbguart();

        /* Make uart host buffer clean */
        Hal_Uart_DataSend(UART_IDX_1, 0);
        Hal_DbgUart_DataSend(0);

        msg_print_uart1("\r\nSwitch: AT UART\r\n>");
    }

    // only for the init and user mode
    if ((tSysMode.ubSysMode == MW_FIM_SYS_MODE_INIT) || (tSysMode.ubSysMode == MW_FIM_SYS_MODE_USER))
    {
        /* APP "control" task Initialization */
        App_Ctrl_Init();

        /* Wi-Fi Initialization */
        BleWifi_Wifi_Init();

        /* BLE Stack Initialization */
        BleWifi_Ble_Init();

        /* blewifi HTTP OTA */
        #if (IOT_WIFI_OTA_FUNCTION_EN == 1)
        IoT_OTA_HTTP_Init();
        #endif

        /* IoT device Initialization */
        #if (IOT_DEVICE_DATA_TX_EN == 1) || (IOT_DEVICE_DATA_RX_EN == 1)
        Iot_Data_Init();
        #endif

        /* RF Power settings */
        BleWifi_COM_RFPowerSetting(BLEWIFI_COM_RF_POWER_SETTINGS);

        #if (SNTP_FUNCTION_EN == 1)
        BleWifi_COM_SntpInit();
        #endif

        // set TCA mode
        #if (APP_RF_SET_TCA_MODE == 1)
        sys_set_rf_temp_cal_mode(true);
        #endif
    }

    // update the system mode
    App_Ctrl_SysModeSet(tSysMode.ubSysMode);

    // add app cmd
    app_at_cmd_add();

    // Enable CR+LF as default
    at_cmd_crlf_term_set(CRLF_ENABLE);
}
