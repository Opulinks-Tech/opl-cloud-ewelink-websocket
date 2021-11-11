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

#include <string.h>
#include "blewifi_common.h"
#include "sys_cfg_patch.h"

#if (SNTP_FUNCTION_EN == 1)
uint32_t g_ulSntpSystemSecondInit;      // System Clock Time Initialize
uint32_t g_ulSntpTimestampInit;            // GMT Time Initialize

uint32_t BleWifi_COM_SntpCurrentSysTimeGet(void);
#endif

#ifdef BLEWIFI_SHOW_DUMP
void BleWifi_COM_HexDump(const char *title, const uint8_t *buf, uint32_t len)
{
    char u8aDumpBuf[64];
	uint32_t i;
	uint32_t u32Pos;

	if (buf == NULL)
	{
	    printf("%s - hexdump(len=%lu): [NULL]\n", title, len);
	}
	else
	{
	    printf("%s - hexdump(len=%lu):\n", title, len);

	    u32Pos = 0;
	    memset(u8aDumpBuf, 0, sizeof(u8aDumpBuf));
		for (i = 0; i < len; i++)
		{
		    u32Pos += sprintf(&(u8aDumpBuf[u32Pos]), " %02x", buf[i]);

            if (i%16 == 15)
            {
                printf("%s\n", u8aDumpBuf);
                u32Pos = 0;
                memset(u8aDumpBuf, 0, sizeof(u8aDumpBuf));
            }
		}

        printf("%s\n", u8aDumpBuf);
        u32Pos = 0;
        memset(u8aDumpBuf, 0, sizeof(u8aDumpBuf));
	}
}
#endif

#if (SNTP_FUNCTION_EN == 1)
void BleWifi_COM_SntpInit(void)
{
    BleWifi_COM_SntpTimestampSync(SNTP_SEC_INIT);
}

void BleWifi_COM_SntpTimestampSync(uint32_t u32Timestamp)
{
    g_ulSntpSystemSecondInit = BleWifi_COM_SntpCurrentSysTimeGet();
    g_ulSntpTimestampInit = u32Timestamp;
    return;
}

void BleWifi_COM_SntpDateTimeGet(struct tm *pSystemTime, float fTimeZone)
{
    time_t rawtime;
    struct tm * timeinfo;

    rawtime = BleWifi_COM_SntpTimestampGet() + 3600 * fTimeZone;
    timeinfo = localtime(&rawtime);
    memcpy(pSystemTime, timeinfo, sizeof(struct tm));
    pSystemTime->tm_year = pSystemTime->tm_year + 1900;
    pSystemTime->tm_mon = pSystemTime->tm_mon + 1;
    // printf("Current time: %d-%d-%d %d:%d:%d\n", pSystemTime->tm_year, pSystemTime->tm_mon, pSystemTime->tm_mday, pSystemTime->tm_hour, pSystemTime->tm_min, pSystemTime->tm_sec);
}

time_t BleWifi_COM_SntpTimestampGet(void)
{
    time_t rawtime;
    uint32_t ulTmpSystemSecond = 0;
    uint32_t ulDeltaSystemSecond = 0;

    ulTmpSystemSecond = BleWifi_COM_SntpCurrentSysTimeGet();
    ulDeltaSystemSecond =  (ulTmpSystemSecond - g_ulSntpSystemSecondInit);
    rawtime = g_ulSntpTimestampInit + ulDeltaSystemSecond;

    return rawtime;
}

uint32_t BleWifi_COM_SntpCurrentSysTimeGet(void)
{
    uint32_t ulTick;
    int32_t  dwOverflow;
    uint32_t ulsecond;

    uint32_t ulSecPerTickOverflow;
    uint32_t ulSecModTickOverflow;
    uint32_t ulMsModTickOverflow;

    uint32_t ulSecPerTick;
    uint32_t ulSecModTick;

    osKernelSysTickEx(&ulTick, &dwOverflow);

    // the total time of TickOverflow is 4294967295 + 1 ms
    ulSecPerTickOverflow = (0xFFFFFFFF / osKernelSysTickFrequency) * dwOverflow;
    ulSecModTickOverflow = (((0xFFFFFFFF % osKernelSysTickFrequency) + 1) * dwOverflow) / osKernelSysTickFrequency;
    ulMsModTickOverflow = (((0xFFFFFFFF % osKernelSysTickFrequency) + 1) * dwOverflow) % osKernelSysTickFrequency;

    ulSecPerTick = (ulTick / osKernelSysTickFrequency);
    ulSecModTick = (ulTick % osKernelSysTickFrequency + ulMsModTickOverflow) / osKernelSysTickFrequency;

    ulsecond = (ulSecPerTickOverflow + ulSecModTickOverflow) + (ulSecPerTick + ulSecModTick);

    return ulsecond;
}
#endif

/*
Set RF power (0x00 - 0xFF)
*/
void BleWifi_COM_RFPowerSetting(uint8_t level)
{
    T_RfCfg tCfg;
    int ret = 0;

    tCfg.u8HighPwrStatus = level;
    ret = sys_cfg_rf_init_patch(&tCfg);
    printf("RF Power Settings is = %s \n", (ret == 0 ? "successful" : "false"));
}

uint8_t BleWifi_COM_EventCreate(EventGroupHandle_t *tEventGroup)
{
    if(tEventGroup == NULL)
    {
        return false;
    }
    *tEventGroup = xEventGroupCreate();

    if(*tEventGroup == NULL)
    {
        BLEWIFI_ERROR("task create event group fail \r\n");
        return false;
    }

    return true;
}

void BleWifi_COM_EventStatusSet(EventGroupHandle_t tEventGroup , uint32_t dwEventBit , uint8_t status)
{
// ISR mode is not supported.
#if 0
    BaseType_t xHigherPriorityTaskWoken, xResult;

    // check if it is ISR mode or not
    if (0 != __get_IPSR())
    {
        if (true == status)
        {
            // xHigherPriorityTaskWoken must be initialised to pdFALSE.
    		xHigherPriorityTaskWoken = pdFALSE;

            // Set bit in xEventGroup.
            xResult = xEventGroupSetBitsFromISR(tEventGroup , dwEventBit, &xHigherPriorityTaskWoken);
            if( xResult == pdPASS )
    		{
    			// If xHigherPriorityTaskWoken is now set to pdTRUE then a context
    			// switch should be requested.  The macro used is port specific and
    			// will be either portYIELD_FROM_ISR() or portEND_SWITCHING_ISR() -
    			// refer to the documentation page for the port being used.
    			portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    		}
        }
        else
            xEventGroupClearBitsFromISR(tEventGroup , dwEventBit);
    }
    // Taske mode
    else
#endif
    {
        if (true == status)
            xEventGroupSetBits(tEventGroup, dwEventBit);
        else
            xEventGroupClearBits(tEventGroup, dwEventBit);
    }
}

uint8_t BleWifi_COM_EventStatusGet(EventGroupHandle_t tEventGroup , uint32_t dwEventBit)
{
    EventBits_t tRetBit;

    tRetBit = xEventGroupGetBits(tEventGroup);
    if (dwEventBit == (dwEventBit & tRetBit))
        return true;

    return false;
}

uint8_t BleWifi_COM_EventStatusWait(EventGroupHandle_t tEventGroup , uint32_t dwEventBit , uint32_t millisec)
{
    EventBits_t tRetBit;

    tRetBit = xEventGroupWaitBits(tEventGroup,
                                  dwEventBit,
                                  pdFALSE,
                                  pdTRUE,
                                  millisec);
    if (dwEventBit == (dwEventBit & tRetBit))
        return true;

    return false;
}

