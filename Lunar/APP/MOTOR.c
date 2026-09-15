#include "MOTOR.h"

/* htim3 is generated in Core/Src/main.c. */
extern TIM_HandleTypeDef htim3;

#define MOTOR_IN1_CHANNEL    TIM_CHANNEL_1
#define MOTOR_IN2_CHANNEL    TIM_CHANNEL_2

static uint32_t MOTOR_DutyToCompare(uint16_t duty)
{
    uint32_t period;

    if (duty > MOTOR_DUTY_MAX)
    {
        duty = MOTOR_DUTY_MAX;
    }

    period = __HAL_TIM_GET_AUTORELOAD(&htim3);
    return (uint32_t)(((uint64_t)period * (uint64_t)duty) /
                      (uint64_t)MOTOR_DUTY_MAX);
}

static void MOTOR_WriteInputs(uint32_t in1_compare, uint32_t in2_compare)
{
    /* Clear both inputs before changing direction. */
    __HAL_TIM_SET_COMPARE(&htim3, MOTOR_IN1_CHANNEL, 0U);
    __HAL_TIM_SET_COMPARE(&htim3, MOTOR_IN2_CHANNEL, 0U);
    __HAL_TIM_SET_COMPARE(&htim3, MOTOR_IN1_CHANNEL, in1_compare);
    __HAL_TIM_SET_COMPARE(&htim3, MOTOR_IN2_CHANNEL, in2_compare);
}

void MOTOR_Init(void)
{
    __HAL_TIM_SET_COMPARE(&htim3, MOTOR_IN1_CHANNEL, 0U);
    __HAL_TIM_SET_COMPARE(&htim3, MOTOR_IN2_CHANNEL, 0U);

    (void)HAL_TIM_PWM_Start(&htim3, MOTOR_IN1_CHANNEL);
    (void)HAL_TIM_PWM_Start(&htim3, MOTOR_IN2_CHANNEL);
}

void MOTOR_SetMode(MOTOR_Mode mode, uint16_t duty)
{
    uint32_t compare;
    uint32_t period;

    compare = MOTOR_DutyToCompare(duty);
    period = __HAL_TIM_GET_AUTORELOAD(&htim3);

    switch (mode)
    {
        case MOTOR_MODE_FORWARD:
            MOTOR_WriteInputs(compare, 0U);
            break;

        case MOTOR_MODE_REVERSE:
            MOTOR_WriteInputs(0U, compare);
            break;

        case MOTOR_MODE_BRAKE:
            /* DRV8870: IN1 = IN2 = 1 is active braking. */
            MOTOR_WriteInputs(period, period);
            break;

        case MOTOR_MODE_COAST:
        default:
            MOTOR_WriteInputs(0U, 0U);
            break;
    }
}

void MOTOR_Forward(uint16_t duty)
{
    MOTOR_SetMode(MOTOR_MODE_FORWARD, duty);
}

void MOTOR_Reverse(uint16_t duty)
{
    MOTOR_SetMode(MOTOR_MODE_REVERSE, duty);
}

void MOTOR_Stop(void)
{
    MOTOR_SetMode(MOTOR_MODE_COAST, 0U);
}

void MOTOR_Brake(void)
{
    MOTOR_SetMode(MOTOR_MODE_BRAKE, MOTOR_DUTY_MAX);
}

void MOTOR_SetSpeed(int16_t speed)
{
    if (speed > (int16_t)MOTOR_DUTY_MAX)
    {
        speed = (int16_t)MOTOR_DUTY_MAX;
    }
    else if (speed < -(int16_t)MOTOR_DUTY_MAX)
    {
        speed = -(int16_t)MOTOR_DUTY_MAX;
    }

    if (speed > 0)
    {
        MOTOR_Forward((uint16_t)speed);
    }
    else if (speed < 0)
    {
        MOTOR_Reverse((uint16_t)(-speed));
    }
    else
    {
        MOTOR_Stop();
    }
}
