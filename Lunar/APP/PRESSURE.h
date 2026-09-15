#ifndef LUNAR_PRESSURE_H
#define LUNAR_PRESSURE_H

#include "stm32f4xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * XGZP6161 parameters.
 * The installed sensor is powered from 5 V and its 0.5 V to 4.5 V output is
 * divided by 10 kOhm / 20 kOhm on the board before reaching ADC1_IN1 (PA1).
 * Change only PRESSURE_FULL_SCALE_MPA when a different range is fitted.
 */
#define PRESSURE_FULL_SCALE_MPA          1.0f
#define PRESSURE_SENSOR_ZERO_VOLTAGE     0.5f
#define PRESSURE_SENSOR_FULL_VOLTAGE     4.5f
#define PRESSURE_ADC_REFERENCE_VOLTAGE   3.3f
#define PRESSURE_DIVIDER_GAIN            1.5f

typedef enum
{
    PRESSURE_NOT_READY = 0,
    PRESSURE_OK,
    PRESSURE_ADC_ERROR
} PRESSURE_Status;

void PRESSURE_Init(void);
void PRESSURE_Proc(void);

PRESSURE_Status PRESSURE_GetStatus(void);
uint16_t PRESSURE_GetRawADC(void);
float PRESSURE_GetMPa(void);
uint16_t PRESSURE_GetCentiMPa(void);

#ifdef __cplusplus
}
#endif

#endif /* LUNAR_PRESSURE_H */
