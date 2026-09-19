#ifndef LUNAR_MOTOR_H
#define LUNAR_MOTOR_H

#include "stm32f4xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Direct percentage: 0 = stopped, 1 = 1%, 100 = 100%. */
#define MOTOR_DUTY_MAX 100U

typedef enum
{
    MOTOR_SLICE_IDLE = 0,
    MOTOR_SLICE_RUNNING,
    MOTOR_SLICE_DONE
} MOTOR_SliceStatus;

/*
 * Unidirectional pump:
 *   PA6 / TIM3_CH1 -> DRV8870 IN1 PWM
 *   PA7            -> DRV8870 IN2, always low
 */
void MOTOR_Init(void);
void MOTOR_SetDuty(uint8_t duty_percent);
void MOTOR_Forward(uint8_t duty_percent);
void MOTOR_Stop(void);
uint8_t MOTOR_GetDuty(void);

/*
 * Non-blocking timed pump slice.
 *
 * Call repeatedly from the main loop. A new duty/duration pair starts a new
 * slice. When the duration expires the pump stops and the function keeps
 * returning MOTOR_SLICE_DONE, so the same call cannot restart by accident.
 * Call MOTOR_SliceReset() before intentionally repeating the same slice.
 */
MOTOR_SliceStatus MOTOR_Slice(
    uint8_t duty_percent,
    uint32_t duration_ms);
void MOTOR_SliceReset(void);
MOTOR_SliceStatus MOTOR_GetSliceStatus(void);
uint32_t MOTOR_GetRemainingMs(void);

/* Compatibility helper: positive=forward, zero/negative=stop. */
void MOTOR_SetSpeed(int16_t speed_percent);

#ifdef __cplusplus
}
#endif

#endif /* LUNAR_MOTOR_H */
