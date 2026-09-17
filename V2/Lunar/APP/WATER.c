#include "WATER.h"

#define WATER_MIN_SAMPLE_MS        100U
#define MILLISECONDS_PER_MINUTE  60000.0f
#define WATER_FILTER_SAMPLES         4U

extern TIM_HandleTypeDef htim2;

static WATER_Status water_status = WATER_NOT_READY;
static uint32_t water_last_sample_tick = 0U;
static uint32_t water_last_pulse_count = 0U;
static uint32_t water_counter_snapshot = 0U;
static float water_flow_l_min = 0.0f;
static uint16_t water_flow_centi_l_min = 0U;
static float water_total_liters = 0.0f;
static float water_flow_history[WATER_FILTER_SAMPLES] = {0.0f};
static float water_flow_sum = 0.0f;
static uint8_t water_flow_history_index = 0U;
static uint8_t water_flow_history_count = 0U;
static uint8_t water_timer_running = 0U;

void WATER_Init(void)
{
    uint32_t i;

    water_last_pulse_count = 0U;
    water_counter_snapshot = 0U;
    water_flow_l_min = 0.0f;
    water_flow_centi_l_min = 0U;
    water_total_liters = 0.0f;
    water_flow_sum = 0.0f;
    water_flow_history_index = 0U;
    water_flow_history_count = 0U;
    water_timer_running = 0U;
    for (i = 0U; i < WATER_FILTER_SAMPLES; i++)
    {
        water_flow_history[i] = 0.0f;
    }
    water_last_sample_tick = HAL_GetTick();

    __HAL_TIM_SET_COUNTER(&htim2, 0U);
    if (HAL_TIM_Base_Start(&htim2) == HAL_OK)
    {
        water_timer_running = 1U;
        water_status = WATER_NOT_READY;
    }
    else
    {
        water_status = WATER_TIMER_ERROR;
    }
}

WATER_Status WATER_Update(void)
{
    uint32_t now_tick;
    uint32_t elapsed_ms;
    uint32_t current_counter;
    float instant_flow_l_min;

    if (water_timer_running == 0U)
    {
        water_status = WATER_TIMER_ERROR;
        return water_status;
    }

    now_tick = HAL_GetTick();
    elapsed_ms = now_tick - water_last_sample_tick;
    if (elapsed_ms < WATER_MIN_SAMPLE_MS)
    {
        water_status = WATER_NOT_READY;
        return water_status;
    }

    current_counter = __HAL_TIM_GET_COUNTER(&htim2);
    water_last_pulse_count = current_counter - water_counter_snapshot;
    water_counter_snapshot = current_counter;
    water_last_sample_tick = now_tick;

    instant_flow_l_min = ((float)water_last_pulse_count *
                          MILLISECONDS_PER_MINUTE) /
                         (WATER_PULSES_PER_LITER * (float)elapsed_ms);
    water_total_liters +=
        (float)water_last_pulse_count / WATER_PULSES_PER_LITER;

    water_flow_sum -= water_flow_history[water_flow_history_index];
    water_flow_history[water_flow_history_index] = instant_flow_l_min;
    water_flow_sum += instant_flow_l_min;

    water_flow_history_index++;
    if (water_flow_history_index >= WATER_FILTER_SAMPLES)
    {
        water_flow_history_index = 0U;
    }
    if (water_flow_history_count < WATER_FILTER_SAMPLES)
    {
        water_flow_history_count++;
    }

    water_flow_l_min =
        water_flow_sum / (float)water_flow_history_count;

    water_flow_centi_l_min =
        (uint16_t)(water_flow_l_min * 100.0f + 0.5f);
    water_status = WATER_OK;
    return water_status;
}

WATER_Status WATER_GetStatus(void)
{
    return water_status;
}

uint32_t WATER_GetLastPulseCount(void)
{
    return water_last_pulse_count;
}

float WATER_GetFlowLMin(void)
{
    return water_flow_l_min;
}

uint16_t WATER_GetFlowCentiLMin(void)
{
    return water_flow_centi_l_min;
}

float WATER_GetTotalLiters(void)
{
    return water_total_liters;
}
