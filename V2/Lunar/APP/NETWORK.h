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

/**
 * @brief 发送传感器数据
 *
 * temperature      温度，单位℃
 * pressure_milli   压力，单位0.001 MPa
 * flow_centi       流量，单位0.01 L/min
 */
HAL_StatusTypeDef NETWORK_SendSensorData(float temperature,
                                         uint32_t pressure_milli,
                                         uint32_t flow_centi);

#endif
