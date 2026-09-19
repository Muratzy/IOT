#include "NETWORK.h"
#include <stdio.h>
#include <string.h>

#define NETWORK_RX_LINE_SIZE 64U

static UART_HandleTypeDef *network_uart = NULL;
static uint8_t network_rx_byte = 0U;
static char network_rx_line[NETWORK_RX_LINE_SIZE];
static volatile uint16_t network_rx_length = 0U;
static volatile uint8_t network_rx_line_ready = 0U;
static volatile uint8_t network_rx_discard = 0U;

static void NETWORK_ArmReceive(void)
{
    if (network_uart != NULL)
    {
        (void)HAL_UART_Receive_IT(
            network_uart,
            &network_rx_byte,
            1U);
    }
}

static uint8_t NETWORK_ParseUInt32(
    const char **cursor,
    uint32_t *value)
{
    const char *text;
    uint32_t result = 0U;
    uint32_t digit;

    if ((cursor == NULL) || (*cursor == NULL) || (value == NULL))
    {
        return 0U;
    }

    text = *cursor;
    if ((*text < '0') || (*text > '9'))
    {
        return 0U;
    }

    while ((*text >= '0') && (*text <= '9'))
    {
        digit = (uint32_t)(*text - '0');
        if (result > ((UINT32_MAX - digit) / 10U))
        {
            return 0U;
        }
        result = result * 10U + digit;
        text++;
    }

    *cursor = text;
    *value = result;
    return 1U;
}

static NETWORK_CommandStatus NETWORK_ParsePumpCommand(
    const char *line,
    NETWORK_PumpCommand *command)
{
    const char *cursor;
    uint32_t duty_percent;
    uint32_t duration_ms;

    if ((line == NULL) || (command == NULL))
    {
        return NETWORK_COMMAND_INVALID;
    }
    if (strncmp(line, "PUMP=", 5U) != 0)
    {
        return NETWORK_COMMAND_INVALID;
    }

    cursor = line + 5U;
    if (NETWORK_ParseUInt32(&cursor, &duty_percent) == 0U)
    {
        return NETWORK_COMMAND_INVALID;
    }
    if (strncmp(cursor, ",TIME=", 6U) != 0)
    {
        return NETWORK_COMMAND_INVALID;
    }

    cursor += 6U;
    if (NETWORK_ParseUInt32(&cursor, &duration_ms) == 0U)
    {
        return NETWORK_COMMAND_INVALID;
    }
    if (*cursor != '\0')
    {
        return NETWORK_COMMAND_INVALID;
    }
    if ((duty_percent > 100U) ||
        (duration_ms > NETWORK_PUMP_DURATION_MAX_MS) ||
        ((duty_percent > 0U) && (duration_ms == 0U)))
    {
        return NETWORK_COMMAND_INVALID;
    }

    command->duty_percent = (uint8_t)duty_percent;
    command->duration_ms = (duty_percent == 0U) ? 0U : duration_ms;
    return NETWORK_COMMAND_VALID;
}

void NETWORK_Init(UART_HandleTypeDef *huart)
{
    network_uart = huart;
    network_rx_byte = 0U;
    network_rx_length = 0U;
    network_rx_line_ready = 0U;
    network_rx_discard = 0U;
    NETWORK_ArmReceive();
}

HAL_StatusTypeDef NETWORK_SendString(const char *str)
{
    if ((network_uart == NULL) || (str == NULL))
    {
        return HAL_ERROR;
    }

    return HAL_UART_Transmit(
        network_uart,
        (uint8_t *)str,
        (uint16_t)strlen(str),
        100U);
}

HAL_StatusTypeDef NETWORK_SendSensorData(
    const NETWORK_SensorFrame *frame)
{
    char buffer[200];
    uint32_t t1_abs;
    uint32_t t2_abs;
    uint32_t delta_abs;
    int len;

    if ((network_uart == NULL) || (frame == NULL))
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
        "P=%07lu,SP=%u,A=%04u,D=%c%04lu,"
        "M=%03u,MS=%u,MT=%010lu\r\n",
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
        (unsigned long)delta_abs,
        (unsigned int)frame->pump_duty_percent,
        (unsigned int)frame->pump_status,
        (unsigned long)frame->pump_remaining_ms);

    if ((len <= 0) || (len >= (int)sizeof(buffer)))
    {
        return HAL_ERROR;
    }

    return HAL_UART_Transmit(
        network_uart,
        (uint8_t *)buffer,
        (uint16_t)len,
        100U);
}

NETWORK_CommandStatus NETWORK_ReadPumpCommand(
    NETWORK_PumpCommand *command)
{
    char line[NETWORK_RX_LINE_SIZE];
    uint32_t primask;

    if (command == NULL)
    {
        return NETWORK_COMMAND_INVALID;
    }
    if (network_rx_line_ready == 0U)
    {
        return NETWORK_COMMAND_NONE;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    (void)memcpy(line, network_rx_line, NETWORK_RX_LINE_SIZE);
    network_rx_line_ready = 0U;
    if (primask == 0U)
    {
        __enable_irq();
    }

    return NETWORK_ParsePumpCommand(line, command);
}

HAL_StatusTypeDef NETWORK_SendPumpAck(
    uint8_t duty_percent,
    uint32_t duration_ms,
    uint8_t accepted)
{
    char buffer[64];
    int len;

    if (network_uart == NULL)
    {
        return HAL_ERROR;
    }

    len = snprintf(
        buffer,
        sizeof(buffer),
        "PUMP_ACK=%u,TIME=%lu,OK=%u\r\n",
        (unsigned int)duty_percent,
        (unsigned long)duration_ms,
        accepted != 0U ? 1U : 0U);
    if ((len <= 0) || (len >= (int)sizeof(buffer)))
    {
        return HAL_ERROR;
    }

    return HAL_UART_Transmit(
        network_uart,
        (uint8_t *)buffer,
        (uint16_t)len,
        100U);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if ((network_uart == NULL) || (huart != network_uart))
    {
        return;
    }

    if (network_rx_byte == '\n')
    {
        if ((network_rx_discard == 0U) &&
            (network_rx_length > 0U) &&
            (network_rx_line_ready == 0U))
        {
            network_rx_line[network_rx_length] = '\0';
            network_rx_line_ready = 1U;
        }
        network_rx_length = 0U;
        network_rx_discard = 0U;
    }
    else if (network_rx_byte != '\r')
    {
        if (network_rx_line_ready != 0U)
        {
            network_rx_discard = 1U;
        }
        else if (network_rx_discard == 0U)
        {
            if (network_rx_length < (NETWORK_RX_LINE_SIZE - 1U))
            {
                network_rx_line[network_rx_length] =
                    (char)network_rx_byte;
                network_rx_length++;
            }
            else
            {
                network_rx_length = 0U;
                network_rx_discard = 1U;
            }
        }
    }

    NETWORK_ArmReceive();
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if ((network_uart == NULL) || (huart != network_uart))
    {
        return;
    }

    network_rx_length = 0U;
    network_rx_discard = 0U;
    NETWORK_ArmReceive();
}
