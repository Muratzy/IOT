#include "MOTOR.h"

extern TIM_HandleTypeDef htim3;

static MOTOR_SliceStatus motor_slice_status = MOTOR_SLICE_IDLE;
static uint8_t motor_slice_duty = 0U;
static uint32_t motor_slice_duration = 0U;
static uint32_t motor_slice_start_tick = 0U;

static uint32_t MOTOR_DutyToCompare(uint8_t duty_percent)
{
    uint32_t timer_period;

    if (duty_percent > MOTOR_DUTY_MAX)
    {
        duty_percent = MOTOR_DUTY_MAX;
    }

    timer_period = __HAL_TIM_GET_AUTORELOAD(&htim3) + 1U;
    return (uint32_t)(((uint64_t)timer_period * duty_percent) / 100U);
}

void MOTOR_Init(void)
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0U);
    (void)HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
    motor_slice_status = MOTOR_SLICE_IDLE;
    motor_slice_duty = 0U;
    motor_slice_duration = 0U;
    motor_slice_start_tick = 0U;
}

void MOTOR_SetDuty(uint8_t duty_percent)
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET);
    __HAL_TIM_SET_COMPARE(
        &htim3,
        TIM_CHANNEL_1,
        MOTOR_DutyToCompare(duty_percent));
}

void MOTOR_Forward(uint8_t duty_percent)
{
    MOTOR_SetDuty(duty_percent);
}

void MOTOR_Stop(void)
{
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0U);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET);
}

MOTOR_SliceStatus MOTOR_Slice(
    uint8_t duty_percent,
    uint32_t duration_ms)
{
    uint32_t now_tick;

    if (duty_percent > MOTOR_DUTY_MAX)
    {
        duty_percent = MOTOR_DUTY_MAX;
    }

    if ((duty_percent == 0U) || (duration_ms == 0U))
    {
        MOTOR_Stop();
        motor_slice_status = MOTOR_SLICE_DONE;
        motor_slice_duty = duty_percent;
        motor_slice_duration = duration_ms;
        return motor_slice_status;
    }

    now_tick = HAL_GetTick();

    if ((motor_slice_status == MOTOR_SLICE_IDLE) ||
        (duty_percent != motor_slice_duty) ||
        (duration_ms != motor_slice_duration))
    {
        motor_slice_duty = duty_percent;
        motor_slice_duration = duration_ms;
        motor_slice_start_tick = now_tick;
        motor_slice_status = MOTOR_SLICE_RUNNING;
        MOTOR_SetDuty(duty_percent);
    }

    if ((motor_slice_status == MOTOR_SLICE_RUNNING) &&
        ((uint32_t)(now_tick - motor_slice_start_tick) >=
         motor_slice_duration))
    {
        MOTOR_Stop();
        motor_slice_status = MOTOR_SLICE_DONE;
    }

    return motor_slice_status;
}

void MOTOR_SliceReset(void)
{
    MOTOR_Stop();
    motor_slice_status = MOTOR_SLICE_IDLE;
    motor_slice_duty = 0U;
    motor_slice_duration = 0U;
    motor_slice_start_tick = 0U;
}

void MOTOR_SetSpeed(int16_t speed_percent)
{
    if (speed_percent <= 0)
    {
        MOTOR_Stop();
        return;
    }
    if (speed_percent > (int16_t)MOTOR_DUTY_MAX)
    {
        speed_percent = (int16_t)MOTOR_DUTY_MAX;
    }
    MOTOR_SetDuty((uint8_t)speed_percent);
}
