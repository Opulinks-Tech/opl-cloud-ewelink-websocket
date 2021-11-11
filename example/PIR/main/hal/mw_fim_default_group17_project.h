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
#ifndef _MW_FIM_DEFAULT_GROUP17_PROJECT_H_
#define _MW_FIM_DEFAULT_GROUP17_PROJECT_H_

#ifdef __cplusplus
extern "C" {
#endif

// Sec 0: Comment block of the file


// Sec 1: Include File
#include "mw_fim.h"


// Sec 2: Constant Definitions, Imported Symbols, miscellaneous
// the file ID
// xxxx_xxxx_xxxx_xxxx_xxxx_xxxx_xxxx_xxxx
// ^^^^ ^^^^ Zone (0~3)
//           ^^^^ ^^^^ Group (0~8), 0 is reserved for swap
//                     ^^^^ ^^^^ ^^^^ ^^^^ File ID, start from 0
typedef enum
{
    MW_FIM_IDX_GP17_PROJECT_START = 0x01070000,             // the start IDX of group 17
    MW_FIM_IDX_GP17_PROJECT_LOCAL_TIMER,
    MW_FIM_IDX_GP17_PROJECT_HOLD_REMINDER,
    MW_FIM_IDX_GP17_PROJECT_SYSTEM_DEFENSE_EN,

    MW_FIM_IDX_GP17_PROJECT_MAX
} E_MwFimIdxGroup17_Project;


/******************************
Declaration of data structure
******************************/
// Sec 3: structure, uniou, enum, linked list

//#define MW_FIM_VER17_PROJECT 0x04    // 0x00 ~ 0xFF
typedef struct
{
    uint8_t u8Enable;
    uint8_t u8Switch;  // #define DOOR_WARING_FLAG_OPEN 0 ,  #define DOOR_WARING_FLAG_CLOSE         1
    uint16_t u16Time;  // minute

} T_MwFim_GP17_Hold_Reminder;

#define MW_FIM_GP17_HOLD_REMINDER_SIZE  sizeof(T_MwFim_GP17_Hold_Reminder)
#define MW_FIM_GP17_HOLD_REMINDER_NUM   1

typedef struct
{
    int32_t s32TimeZone; // seconds
    uint8_t u8RepeatMask;//bit0~6 sun~sat bit 7 repeat
    uint8_t u8Enable;
    uint8_t u8Hour;
    uint8_t u8Min;
    uint8_t u8DefenseState;
    uint8_t u8Reserve[3];

} T_MwFim_GP17_Local_Timer;

#define MW_FIM_GP17_LOCAL_TIMER_SIZE  sizeof(T_MwFim_GP17_Local_Timer)
#define MW_FIM_GP17_LOCAL_TIMER_NUM   8

typedef struct
{
    uint8_t u8En;
} T_MwFim_GP17_System_Defense_En;

#define MW_FIM_GP17_SYSTEM_DEFENSE_EN_SIZE  sizeof(T_MwFim_GP17_System_Defense_En)
#define MW_FIM_GP17_SYSTEM_DEFENSE_EN_NUM   1

/********************************************
Declaration of Global Variables & Functions
********************************************/
// Sec 4: declaration of global variable
extern const T_MwFimFileInfo g_taMwFimGroupTable17_project[];

extern const T_MwFim_GP17_Local_Timer g_tMwFimDefaultGp17LocalTimer;
extern const T_MwFim_GP17_Hold_Reminder g_tMwFimDefaultGp17HoldReminder;
extern const T_MwFim_GP17_System_Defense_En g_tMwFimDefaultGp17SystemDefenseEn;

// Sec 5: declaration of global function prototype


/***************************************************
Declaration of static Global Variables & Functions
***************************************************/
// Sec 6: declaration of static global variable


// Sec 7: declaration of static function prototype


#ifdef __cplusplus
}
#endif

#endif // _MW_FIM_DEFAULT_GROUP17_PROJECT_H_
