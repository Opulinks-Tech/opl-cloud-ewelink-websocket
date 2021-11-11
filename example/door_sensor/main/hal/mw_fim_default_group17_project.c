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
#include "mw_fim_default_group17_project.h"
#include "blewifi_configuration.h"
#include "app_ctrl.h"


// Sec 2: Constant Definitions, Imported Symbols, miscellaneous


/********************************************
Declaration of data structure
********************************************/
// Sec 3: structure, uniou, enum, linked list


/********************************************
Declaration of Global Variables & Functions
********************************************/
// Sec 4: declaration of global variable
const T_MwFim_GP17_Local_Timer g_tMwFimDefaultGp17LocalTimer = {0};
const T_MwFim_GP17_Hold_Reminder g_tMwFimDefaultGp17HoldReminder =
{
    .u8Enable = false,
    .u8Switch = DOOR_WARING_FLAG_CLOSE,
    .u16Time = 0
};
const T_MwFim_GP17_System_Defense_En g_tMwFimDefaultGp17SystemDefenseEn =
{
    .u8En = DOOR_DEFENSE_ON
};

uint32_t g_ulaMwFimAddrBufferGp17LocalTimer[MW_FIM_GP17_LOCAL_TIMER_NUM];
uint32_t g_ulaMwFimAddrBufferGp17HoldReminder[MW_FIM_GP17_HOLD_REMINDER_NUM];
uint32_t g_ulaMwFimAddrBufferGp17SystemDefenseEn[MW_FIM_GP17_SYSTEM_DEFENSE_EN_NUM];

// the information table of group 17
const T_MwFimFileInfo g_taMwFimGroupTable17_project[] =
{
    {MW_FIM_IDX_GP17_PROJECT_LOCAL_TIMER,                 MW_FIM_GP17_LOCAL_TIMER_NUM,                  MW_FIM_GP17_LOCAL_TIMER_SIZE,                 (uint8_t*)&g_tMwFimDefaultGp17LocalTimer,              g_ulaMwFimAddrBufferGp17LocalTimer},
    {MW_FIM_IDX_GP17_PROJECT_HOLD_REMINDER,               MW_FIM_GP17_HOLD_REMINDER_NUM,                MW_FIM_GP17_HOLD_REMINDER_SIZE,               (uint8_t*)&g_tMwFimDefaultGp17HoldReminder,            g_ulaMwFimAddrBufferGp17HoldReminder},
    {MW_FIM_IDX_GP17_PROJECT_SYSTEM_DEFENSE_EN,           MW_FIM_GP17_SYSTEM_DEFENSE_EN_NUM,            MW_FIM_GP17_SYSTEM_DEFENSE_EN_SIZE,           (uint8_t*)&g_tMwFimDefaultGp17SystemDefenseEn,         g_ulaMwFimAddrBufferGp17SystemDefenseEn},
    // the end, don't modify and remove it
    {0xFFFFFFFF,        0x00,       0x00,       NULL,       NULL}
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
