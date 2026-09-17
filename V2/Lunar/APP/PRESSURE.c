#include "PRESSURE.h"

#define PRESSURE_SAMPLE_COUNT       64U
#define PRESSURE_FILTER_COUNT       32U
#define PRESSURE_SPAN_PA       1238000LL
#define PRESSURE_GAIN_X1000       1320LL
#define PRESSURE_MAX_PA         1000000LL
#define PRESSURE_ZERO_BATCH_COUNT      4U

extern ADC_HandleTypeDef hadc1;

static PRESSURE_Status pressure_status = PRESSURE_NOT_READY;
static int32_t pressure_zero_sum = 0;
static uint16_t pressure_zero_adc = 0U;
static uint16_t pressure_raw_adc = 0U;
static int16_t pressure_delta_adc = 0;
static uint32_t pressure_pa = 0U;
static float pressure_mpa = 0.0f;
static uint16_t pressure_centi_mpa = 0U;
static uint16_t pressure_milli_mpa = 0U;

static uint32_t pressure_history[PRESSURE_FILTER_COUNT];
static uint64_t pressure_history_sum = 0ULL;
static uint8_t pressure_history_index = 0U;
static uint8_t pressure_history_count = 0U;

static HAL_StatusTypeDef PRESSURE_ReadSum(int32_t *sum)
{
    uint32_t index;
    uint32_t valid_count = 0U;
    int32_t value_sum = 0;

    if (sum == NULL)
    {
        return HAL_ERROR;
    }

    for (index = 0U; index < PRESSURE_SAMPLE_COUNT; index++)
    {
        if (HAL_ADC_Start(&hadc1) != HAL_OK)
        {
            continue;
        }

        if (HAL_ADC_PollForConversion(&hadc1, 10U) != HAL_OK)
        {
            (void)HAL_ADC_Stop(&hadc1);
            continue;
        }

        value_sum += (int32_t)HAL_ADC_GetValue(&hadc1);
        valid_count++;
        (void)HAL_ADC_Stop(&hadc1);
    }

    if (valid_count == 0U)
    {
        return HAL_ERROR;
    }
    if (valid_count < PRESSURE_SAMPLE_COUNT)
    {
        value_sum = (int32_t)(
            ((int64_t)value_sum * PRESSURE_SAMPLE_COUNT) / valid_count);
    }

    *sum = value_sum;
    return HAL_OK;
}

static void PRESSURE_SetValue(uint32_t value_pa)
{
    pressure_pa = value_pa;
    pressure_mpa = (float)value_pa / 1000000.0f;
    pressure_centi_mpa =
        (uint16_t)((value_pa + 5000U) / 10000U);
    pressure_milli_mpa =
        (uint16_t)((value_pa + 500U) / 1000U);
}

void PRESSURE_Init(void)
{
    uint32_t index;
    uint32_t zero_batch;
    int32_t zero_sample_sum;
    int64_t zero_total = 0LL;

    pressure_status = PRESSURE_NOT_READY;
    pressure_zero_sum = 0;
    pressure_zero_adc = 0U;
    pressure_raw_adc = 0U;
    pressure_delta_adc = 0;
    pressure_history_sum = 0ULL;
    pressure_history_index = 0U;
    pressure_history_count = 0U;
    PRESSURE_SetValue(0U);

    for (index = 0U; index < PRESSURE_FILTER_COUNT; index++)
    {
        pressure_history[index] = 0U;
    }

    /* Average several complete batches so one noisy startup read cannot
       become the permanent zero point. The pump is still stopped here. */
    for (zero_batch = 0U;
         zero_batch < PRESSURE_ZERO_BATCH_COUNT;
         zero_batch++)
    {
        if (PRESSURE_ReadSum(&zero_sample_sum) != HAL_OK)
        {
            pressure_status = PRESSURE_ADC_ERROR;
            return;
        }
        zero_total += zero_sample_sum;
    }
    pressure_zero_sum = (int32_t)(zero_total / PRESSURE_ZERO_BATCH_COUNT);

    pressure_zero_adc =
        (uint16_t)(pressure_zero_sum / (int32_t)PRESSURE_SAMPLE_COUNT);
    pressure_raw_adc = pressure_zero_adc;
    pressure_status = PRESSURE_NOT_READY;
}

PRESSURE_Status PRESSURE_Update(void)
{
    int32_t adc_sum;
    int32_t adc_delta_sum;
    int64_t adc_delta_magnitude;
    int64_t sampled_pa;
    uint32_t filtered_pa;

    if (PRESSURE_ReadSum(&adc_sum) != HAL_OK)
    {
        pressure_status = PRESSURE_ADC_ERROR;
        return pressure_status;
    }

    pressure_raw_adc =
        (uint16_t)(adc_sum / (int32_t)PRESSURE_SAMPLE_COUNT);

    /*
     * Keep the exact verified conversion:
     *   pressure = (sample_sum - zero_sum) * 1,238,000
     *              / (4095 * 64) * 1.32
     */
    adc_delta_sum = adc_sum - pressure_zero_sum;
    pressure_delta_adc =
        (int16_t)(adc_delta_sum / (int32_t)PRESSURE_SAMPLE_COUNT);

    /* The replacement module can have either transfer direction. Pressure is
       the distance from the captured zero point, so preserve the sign only in
       the diagnostic value and convert the magnitude immediately. */
    adc_delta_magnitude = adc_delta_sum;
    if (adc_delta_magnitude < 0LL)
    {
        adc_delta_magnitude = -adc_delta_magnitude;
    }

    sampled_pa = adc_delta_magnitude * PRESSURE_SPAN_PA;
    sampled_pa /= (4095LL * (int64_t)PRESSURE_SAMPLE_COUNT);
    sampled_pa = sampled_pa * PRESSURE_GAIN_X1000 / 1000LL;

    if (sampled_pa < 0LL)
    {
        sampled_pa = 0LL;
    }
    else if (sampled_pa > PRESSURE_MAX_PA)
    {
        sampled_pa = PRESSURE_MAX_PA;
    }

    pressure_history_sum -=
        pressure_history[pressure_history_index];
    pressure_history[pressure_history_index] = (uint32_t)sampled_pa;
    pressure_history_sum += (uint32_t)sampled_pa;
    pressure_history_index++;
    if (pressure_history_index >= PRESSURE_FILTER_COUNT)
    {
        pressure_history_index = 0U;
    }
    if (pressure_history_count < PRESSURE_FILTER_COUNT)
    {
        pressure_history_count++;
    }

    filtered_pa =
        (uint32_t)(pressure_history_sum / pressure_history_count);
    PRESSURE_SetValue(filtered_pa);
    pressure_status = PRESSURE_OK;
    return pressure_status;
}

PRESSURE_Status PRESSURE_GetStatus(void)
{
    return pressure_status;
}

uint16_t PRESSURE_GetRawADC(void)
{
    return pressure_raw_adc;
}

uint16_t PRESSURE_GetZeroADC(void)
{
    return pressure_zero_adc;
}

int16_t PRESSURE_GetDeltaADC(void)
{
    return pressure_delta_adc;
}

uint32_t PRESSURE_GetPa(void)
{
    return pressure_pa;
}

float PRESSURE_GetMPa(void)
{
    return pressure_mpa;
}

uint16_t PRESSURE_GetCentiMPa(void)
{
    return pressure_centi_mpa;
}

uint16_t PRESSURE_GetMilliMPa(void)
{
    return pressure_milli_mpa;
}
