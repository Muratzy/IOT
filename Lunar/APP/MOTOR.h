#ifndef LUNAR_MOTOR_H
#define LUNAR_MOTOR_H

#include "stm32f4xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Duty is expressed in per-mille: 0 = 0%, 1000 = 100%. */
#define MOTOR_DUTY_MAX    1000U

typedef enum
{
    MOTOR_MODE_COAST = 0,
    MOTOR_MODE_FORWARD,
    MOTOR_MODE_REVERSE,
    MOTOR_MODE_BRAKE
} MOTOR_Mode;

/*
 * The schematic routes TIM3_CH1 (PA6) to DRV8870 IN1 and TIM3_CH2 (PA7)
 * to DRV8870 IN2. MX_TIM3_Init() must be called before MOTOR_Init().
 */
void MOTOR_Init(void);
void MOTOR_SetMode(MOTOR_Mode mode, uint16_t duty);
void MOTOR_Forward(uint16_t duty);
void MOTOR_Reverse(uint16_t duty);
void MOTOR_Stop(void);
void MOTOR_Brake(void);

/* Positive speed is forward, negative speed is reverse, range -1000..1000. */
void MOTOR_SetSpeed(int16_t speed);

#ifdef __cplusplus
}
#endif

#endif /* LUNAR_MOTOR_H */
