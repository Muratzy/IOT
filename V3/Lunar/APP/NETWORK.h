#ifndef __NETWORK_H
#define __NETWORK_H

#include "main.h"
#include <stdint.h>

/**
 * @brief 初始化NETWORK模块
 * @param huart 与CH9121连接的UART
 */
void NETWORK_Init(UART_HandleTypeDef *huart);

/**
 * @brief 发送普通字符串
 */
HAL_StatusTypeDef NETWORK_SendString(const char *str);

typedef struct
{
    int16_t temperature_1_x10;
    uint8_t temperature_1_status;
    int16_t temperature_2_x10;
    uint8_t temperature_2_status;
    uint16_t flow_centi_l_min;
    uint8_t flow_status;
    uint32_t pressure_pa;
    uint8_t pressure_status;
    uint16_t pressure_adc;
    int16_t pressure_delta_adc;
} NETWORK_SensorFrame;

/*
 * Send the same values shown on the four OLED rows:
 *   T1/T2, flow, pressure in Pa, current ADC and signed ADC delta.
 * Status values are the corresponding sensor-library enum values.
 */
HAL_StatusTypeDef NETWORK_SendSensorData(
    const NETWORK_SensorFrame *frame);

#endif
