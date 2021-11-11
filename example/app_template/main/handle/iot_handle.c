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

int8_t Iot_Contruct_Post_Data_And_Send(IoT_Properity_t *ptProperity)
{
    //3. contruct post data

    //4. send to Cloud

    return IOT_DATA_POST_RET_CONTINUE_DELETE;
}

int8_t Iot_Data_Parser(uint8_t *szOutBuf, uint16_t out_length)
{
    //1. handle parser

    //2. Send message to app to update status

    return 0;
}
