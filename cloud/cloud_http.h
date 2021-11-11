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

#ifndef __CLOUD_HTTP_H__
#define __CLOUD_HTTP_H__

#include <stdint.h>
#include <stdbool.h>
#include "iot_configuration.h"
#include "httpclient.h"

HTTPCLIENT_RESULT Cloud_Http_Post(httpclient_t *client, char *url, httpclient_data_t *client_data);
HTTPCLIENT_RESULT Cloud_Http_Get(httpclient_t *client, char *url, httpclient_data_t *client_data);
#if CLOUD_HTTP_POST_ENHANCEMENT
HTTPCLIENT_RESULT Cloud_Http_Close(httpclient_t *client);
#endif

#endif // __CLOUD_HTTP_H__

