#ifndef LUNAR_WATER_H
#define LUNAR_WATER_H

#include "stm32f4xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* YF-S401 calibration supplied by the manufacturer. */
#define WATER_PULSES_PER_LITER    3433.8f

typedef enum
{
    WATER_NOT_READY = 0,
    WATER_OK,
    WATER_TIMER_ERROR
} WATER_Status;

void WATER_Init(void);
void WATER_Proc(void);

WATER_Status WATER_GetStatus(void);
uint32_t WATER_GetLastPulseCount(void);
float WATER_GetFlowLMin(void);
uint16_t WATER_GetFlowCentiLMin(void);
float WATER_GetTotalLiters(void);

#ifdef __cplusplus
}
#endif

#endif /* LUNAR_WATER_H */
