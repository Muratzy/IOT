/* DS18B20 one-wire temperature driver for the STM32F401 project. */
#ifndef __TEMPRATURE_H
#define __TEMPRATURE_H

#include "stm32f4xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    DS18B20_OK = 0,
    DS18B20_BUSY,
    DS18B20_NO_DEVICE,
    DS18B20_CRC_ERROR,
    DS18B20_INVALID_PARAMETER
} DS18B20_Status;

/* Release PB6 and PB7 and leave both one-wire buses idle high. */
void DS18B20_Init(void);

/*
 * Read temperature in degrees Celsius, for example 23.5625f.
 * The first call starts conversion and returns DS18B20_BUSY; callers should
 * call again until DS18B20_OK is returned.
 */
DS18B20_Status DS18B20_ReadTemperature(GPIO_TypeDef *GPIOx,
                                       uint16_t GPIO_Pin,
                                       float *temperature_c);

/* Read temperature in 0.01 degrees Celsius, for example 2356 = 23.56 C. */
DS18B20_Status DS18B20_ReadTemperatureX100(GPIO_TypeDef *GPIOx,
                                           uint16_t GPIO_Pin,
                                           int16_t *temperature_centi);

#ifdef __cplusplus
}
#endif

#endif /* __TEMPRATURE_H */
