#ifndef LUNAR_PRESSURE_H
#define LUNAR_PRESSURE_H

#include "stm32f4xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * PA1 / ADC1_IN1 pressure sensor.
 *
 * The conversion follows the implementation verified in the senior project:
 * capture the zero-pressure ADC value at startup, average 64 conversions per
 * update, convert the magnitude of the zero-point difference to pressure, and
 * apply the 1.32 span gain. Both rising-output and falling-output modules are
 * accepted without waiting for a direction threshold.
 * The hydraulic line must be at zero pressure while PRESSURE_Init() runs.
 */
typedef enum
{
    PRESSURE_NOT_READY = 0,
    PRESSURE_OK,
    PRESSURE_ADC_ERROR,
    PRESSURE_SENSOR_ERROR /* retained for API compatibility; no voltage guess */
} PRESSURE_Status;

void PRESSURE_Init(void);
PRESSURE_Status PRESSURE_Update(void);

PRESSURE_Status PRESSURE_GetStatus(void);
uint16_t PRESSURE_GetRawADC(void);
uint16_t PRESSURE_GetZeroADC(void);
int16_t PRESSURE_GetDeltaADC(void);
uint32_t PRESSURE_GetPa(void);
float PRESSURE_GetMPa(void);
uint16_t PRESSURE_GetCentiMPa(void);
uint16_t PRESSURE_GetMilliMPa(void);

#ifdef __cplusplus
}
#endif

#endif /* LUNAR_PRESSURE_H */
