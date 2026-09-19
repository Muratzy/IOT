#ifndef __NETWORK_H
#define __NETWORK_H

#include "main.h"
#include <stdint.h>

#define NETWORK_PUMP_DURATION_MAX_MS 3600000UL

typedef enum
{
    NETWORK_COMMAND_NONE = 0,
    NETWORK_COMMAND_VALID,
    NETWORK_COMMAND_INVALID
} NETWORK_CommandStatus;

typedef struct
{
    uint8_t duty_percent;
    uint32_t duration_ms;
} NETWORK_PumpCommand;

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
    uint8_t pump_duty_percent;
    uint8_t pump_status;
    uint32_t pump_remaining_ms;
} NETWORK_SensorFrame;

/**
 * @brief 初始化 NETWORK 模块并启动 USART 中断接收
 * @param huart 与 CH9121 连接的 UART
 */
void NETWORK_Init(UART_HandleTypeDef *huart);

/**
 * @brief 发送普通字符串
 */
HAL_StatusTypeDef NETWORK_SendString(const char *str);

/**
 * @brief 发送 OLED 数据以及当前水泵执行状态
 */
HAL_StatusTypeDef NETWORK_SendSensorData(
    const NETWORK_SensorFrame *frame);

/**
 * @brief 读取网页下发的 PUMP=<0~100>,TIME=<ms> 命令
 */
NETWORK_CommandStatus NETWORK_ReadPumpCommand(
    NETWORK_PumpCommand *command);

/**
 * @brief 返回水泵命令是否被 STM32 接受
 */
HAL_StatusTypeDef NETWORK_SendPumpAck(
    uint8_t duty_percent,
    uint32_t duration_ms,
    uint8_t accepted);

#endif
