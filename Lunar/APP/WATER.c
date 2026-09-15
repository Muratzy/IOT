#include "WATER.h"

#define WATER_SAMPLE_PERIOD_MS    1000U
#define MILLISECONDS_PER_MINUTE  60000.0f

extern TIM_HandleTypeDef htim2;

static WATER_Status water_status = WATER_NOT_READY;
static uint32_t water_last_sample_tick = 0U;
static uint32_t water_last_pulse_count = 0U;
static float water_flow_l_min = 0.0f;
static uint16_t water_flow_centi_l_min = 0U;
static float water_total_liters = 0.0f;

void WATER_Init(void)
{
    water_last_pulse_count = 0U;
    water_flow_l_min = 0.0f;
    water_flow_centi_l_min = 0U;
    water_total_liters = 0.0f;
    water_last_sample_tick = HAL_GetTick();

    __HAL_TIM_SET_COUNTER(&htim2, 0U);
    if (HAL_TIM_Base_Start(&htim2) == HAL_OK)
    {
        water_status = WATER_OK;
    }
    else
    {
        water_status = WATER_TIMER_ERROR;
    }
}

void WATER_Proc(void)
{
    uint32_t now_tick;
    uint32_t elapsed_ms;

    if (water_status != WATER_OK)
    {
        return;
    }

    now_tick = HAL_GetTick();
    elapsed_ms = now_tick - water_last_sample_tick;
    if (elapsed_ms < WATER_SAMPLE_PERIOD_MS)
    {
        return;
    }

    water_last_pulse_count = __HAL_TIM_GET_COUNTER(&htim2);
    __HAL_TIM_SET_COUNTER(&htim2, 0U);
    water_last_sample_tick = now_tick;

    water_flow_l_min = ((float)water_last_pulse_count *
                        MILLISECONDS_PER_MINUTE) /
                       (WATER_PULSES_PER_LITER * (float)elapsed_ms);
    water_total_liters +=
        (float)water_last_pulse_count / WATER_PULSES_PER_LITER;

    water_flow_centi_l_min =
        (uint16_t)(water_flow_l_min * 100.0f + 0.5f);
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
