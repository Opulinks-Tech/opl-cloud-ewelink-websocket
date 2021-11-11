/******************************************************************************
*  Copyright 2019 - 2019, Opulinks Technology Ltd.
*  ----------------------------------------------------------------------------
*  Statement:
*  ----------
*  This software is protected by Copyright and the information contained
*  herein is confidential. The software may not be copied and the information
*  contained herein may not be used or disclosed except with the written
*  permission of Opulinks Technology Ltd. (C) 2018
******************************************************************************/
#ifndef __SENSOR_DOOR_H__
#define __SENSOR_DOOR_H__

#include "cmsis_os.h"
#include "hal_vic.h"
#include "app_configuration.h"

extern osTimerId g_tAppPIRTimerId;
#ifdef __IDR_EN__  //from build config
extern osTimerId g_tAppIDRTimerId;
#endif
#if(DAMAGE_EN == 1)
extern osTimerId g_tAppDoorDamageTimerId;
#endif

void Sensor_Init(void);

#endif // end of __SENSOR_DOOR_H__

