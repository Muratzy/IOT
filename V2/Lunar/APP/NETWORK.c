#include "NETWORK.h"
#include <stdio.h>
#include <string.h>

static UART_HandleTypeDef *network_uart = NULL;


void NETWORK_Init(UART_HandleTypeDef *huart)
{
    network_uart = huart;
}


HAL_StatusTypeDef NETWORK_SendString(const char *str)
{
    if (network_uart == NULL || str == NULL)
    {
        return HAL_ERROR;
    }

    return HAL_UART_Transmit(network_uart,
                             (uint8_t *)str,
                             strlen(str),
                             100);
}


HAL_StatusTypeDef NETWORK_SendSensorData(float temperature,
                                         uint32_t pressure_milli,
                                         uint32_t flow_centi)
{
    char buffer[128];

    /*
     * 不直接使用 %.2f
     * 避免Keil没有开启浮点printf时出现问题
     */

    int32_t temp_x100;

    if (temperature >= 0.0f)
    {
        temp_x100 = (int32_t)(temperature * 100.0f + 0.5f);
    }
    else
    {
        temp_x100 = (int32_t)(temperature * 100.0f - 0.5f);
    }

    int32_t temp_int = temp_x100 / 100;
    uint32_t temp_frac;

    if (temp_x100 < 0)
    {
        temp_frac = (uint32_t)(-(temp_x100 % 100));
    }
    else
    {
        temp_frac = (uint32_t)(temp_x100 % 100);
    }

    int len = snprintf(
        buffer,
        sizeof(buffer),

        "TEMP=%ld.%02lu,"
        "PRESS=%lu.%03lu,"
        "FLOW=%lu.%02lu\r\n",

        (long)temp_int,
        (unsigned long)temp_frac,

        (unsigned long)(pressure_milli / 1000U),
        (unsigned long)(pressure_milli % 1000U),

        (unsigned long)(flow_centi / 100U),
        (unsigned long)(flow_centi % 100U)
    );

    if (len <= 0 || len >= (int)sizeof(buffer))
    {
        return HAL_ERROR;
    }

    return HAL_UART_Transmit(network_uart,
                             (uint8_t *)buffer,
                             (uint16_t)len,
                             100);
}
