#include "PRESSURE.h"

#define PRESSURE_SAMPLE_PERIOD_MS    100U
#define PRESSURE_AVERAGE_SAMPLES      16U
#define PRESSURE_ADC_FULL_SCALE     4095.0f

extern ADC_HandleTypeDef hadc1;

static PRESSURE_Status pressure_status = PRESSURE_NOT_READY;
static uint16_t pressure_raw_adc = 0U;
static float pressure_mpa = 0.0f;
static uint16_t pressure_centi_mpa = 0U;
static uint32_t pressure_last_sample_tick = 0U;

void PRESSURE_Init(void)
{
    pressure_status = PRESSURE_NOT_READY;
    pressure_raw_adc = 0U;
    pressure_mpa = 0.0f;
    pressure_centi_mpa = 0U;
    pressure_last_sample_tick = HAL_GetTick() - PRESSURE_SAMPLE_PERIOD_MS;
}

void PRESSURE_Proc(void)
{
    uint32_t now_tick;
    uint32_t adc_sum;
    uint32_t sample_index;
    float sensor_voltage;

    now_tick = HAL_GetTick();
    if ((uint32_t)(now_tick - pressure_last_sample_tick) <
        PRESSURE_SAMPLE_PERIOD_MS)
    {
        return;
    }
    pressure_last_sample_tick = now_tick;

    adc_sum = 0U;
    for (sample_index = 0U;
         sample_index < PRESSURE_AVERAGE_SAMPLES;
         sample_index++)
    {
        if (HAL_ADC_Start(&hadc1) != HAL_OK)
        {
            pressure_status = PRESSURE_ADC_ERROR;
            return;
        }

        if (HAL_ADC_PollForConversion(&hadc1, 10U) != HAL_OK)
        {
            (void)HAL_ADC_Stop(&hadc1);
            pressure_status = PRESSURE_ADC_ERROR;
            return;
        }

        adc_sum += HAL_ADC_GetValue(&hadc1);
        (void)HAL_ADC_Stop(&hadc1);
    }

    pressure_raw_adc = (uint16_t)(adc_sum / PRESSURE_AVERAGE_SAMPLES);
    sensor_voltage = ((float)pressure_raw_adc *
                      PRESSURE_ADC_REFERENCE_VOLTAGE /
                      PRESSURE_ADC_FULL_SCALE) * PRESSURE_DIVIDER_GAIN;

    pressure_mpa = (sensor_voltage - PRESSURE_SENSOR_ZERO_VOLTAGE) *
                   PRESSURE_FULL_SCALE_MPA /
                   (PRESSURE_SENSOR_FULL_VOLTAGE -
                    PRESSURE_SENSOR_ZERO_VOLTAGE);

    if (pressure_mpa < 0.0f)
    {
        pressure_mpa = 0.0f;
    }
    else if (pressure_mpa > PRESSURE_FULL_SCALE_MPA)
    {
        pressure_mpa = PRESSURE_FULL_SCALE_MPA;
    }

    pressure_centi_mpa = (uint16_t)(pressure_mpa * 100.0f + 0.5f);
    pressure_status = PRESSURE_OK;
}

PRESSURE_Status PRESSURE_GetStatus(void)
{
    return pressure_status;
}

uint16_t PRESSURE_GetRawADC(void)
{
    return pressure_raw_adc;
}

float PRESSURE_GetMPa(void)
{
    return pressure_mpa;
}

uint16_t PRESSURE_GetCentiMPa(void)
{
    return pressure_centi_mpa;
}
