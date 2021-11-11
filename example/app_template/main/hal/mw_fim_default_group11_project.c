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

/***********************
Head Block of The File
***********************/
// Sec 0: Comment block of the file


// Sec 1: Include File
#include "mw_fim_default_group11.h"
#include "mw_fim_default_group11_project.h"
#include "blewifi_configuration.h"


// Sec 2: Constant Definitions, Imported Symbols, miscellaneous


/********************************************
Declaration of data structure
********************************************/
// Sec 3: structure, uniou, enum, linked list
// the default data of SSID roaming
const T_MwFim_GP11_Ssid_PWD g_tMwFimDefaultGp11SsidPwd = {0};
// the address buffer of SSID roaming
uint32_t g_ulaMwFimAddrBufferGP11SsidPwd[MW_FIM_GP11_SSID_PWD_NUM];

// the default data of fixed SSID roaming
const T_MwFim_GP11_Ssid_PWD g_tMwFimDefaultGp11FixedSsidPwd =
{
    .ssid        = BLEWIFI_WIFI_ROAMING_DEFAULT_SSID,
    .password    = BLEWIFI_WIFI_ROAMING_DEFAULT_PASSWORD,
    .weight      = 0,
    .seqno       = 0,
#if (BLEWIFI_WIFI_ROAMING_DEFAULT_EN == 1)
    .used        = 1,
#else
    .used        = 0,
#endif
};
// the address buffer of fixed SSID roaming
uint32_t g_ulaMwFimAddrBufferGP11FixedSsidPwd[MW_FIM_GP11_FIXED_SSID_PWD_NUM];


/********************************************
Declaration of Global Variables & Functions
********************************************/
// Sec 4: declaration of global variable

// the information table of group 11
const T_MwFimFileInfo g_taMwFimGroupTable11_project[] =
{
    {MW_FIM_IDX_GP11_PROJECT_SSID_PASSWORD, MW_FIM_GP11_SSID_PWD_NUM,  MW_FIM_GP11_SSID_PWD_SIZE, (uint8_t*)&g_tMwFimDefaultGp11SsidPwd, g_ulaMwFimAddrBufferGP11SsidPwd},
    {MW_FIM_IDX_GP11_PROJECT_FIXED_SSID_PASSWORD, MW_FIM_GP11_FIXED_SSID_PWD_NUM,  MW_FIM_GP11_FIXED_SSID_PWD_SIZE, (uint8_t*)&g_tMwFimDefaultGp11FixedSsidPwd, g_ulaMwFimAddrBufferGP11FixedSsidPwd},
    // the end, don't modify and remove it
    {0xFFFFFFFF,            0x00,              0x00,               NULL,                            NULL}
};


// Sec 5: declaration of global function prototype


/***************************************************
Declaration of static Global Variables & Functions
***************************************************/
// Sec 6: declaration of static global variable


// Sec 7: declaration of static function prototype


/***********
C Functions
***********/
// Sec 8: C Functions
