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


HAL_StatusTypeDef NETWORK_SendSensorData(
    const NETWORK_SensorFrame *frame)
{
    char buffer[160];
    uint32_t t1_abs;
    uint32_t t2_abs;
    uint32_t delta_abs;
    int len;

    if (network_uart == NULL || frame == NULL)
    {
        return HAL_ERROR;
    }

    t1_abs = (frame->temperature_1_x10 < 0)
        ? (uint32_t)(-(int32_t)frame->temperature_1_x10)
        : (uint32_t)frame->temperature_1_x10;
    t2_abs = (frame->temperature_2_x10 < 0)
        ? (uint32_t)(-(int32_t)frame->temperature_2_x10)
        : (uint32_t)frame->temperature_2_x10;
    delta_abs = (frame->pressure_delta_adc < 0)
        ? (uint32_t)(-(int32_t)frame->pressure_delta_adc)
        : (uint32_t)frame->pressure_delta_adc;

    len = snprintf(
        buffer,
        sizeof(buffer),
        "T1=%c%lu.%01lu,S1=%u,"
        "T2=%c%lu.%01lu,S2=%u,"
        "F=%02lu.%02lu,SF=%u,"
        "P=%07lu,SP=%u,A=%04u,D=%c%04lu\r\n",
        frame->temperature_1_x10 < 0 ? '-' : '+',
        (unsigned long)(t1_abs / 10U),
        (unsigned long)(t1_abs % 10U),
        (unsigned int)frame->temperature_1_status,
        frame->temperature_2_x10 < 0 ? '-' : '+',
        (unsigned long)(t2_abs / 10U),
        (unsigned long)(t2_abs % 10U),
        (unsigned int)frame->temperature_2_status,
        (unsigned long)(frame->flow_centi_l_min / 100U),
        (unsigned long)(frame->flow_centi_l_min % 100U),
        (unsigned int)frame->flow_status,
        (unsigned long)frame->pressure_pa,
        (unsigned int)frame->pressure_status,
        (unsigned int)frame->pressure_adc,
        frame->pressure_delta_adc < 0 ? '-' : '+',
        (unsigned long)delta_abs
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
