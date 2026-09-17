/* DS18B20 one-wire driver.
 * The board uses one three-wire sensor on PB6 and one on PB7.
 */
#include "Temprature.h"
#include "delay.h"

#define DS18B20_CONVERSION_TIME_MS    750U

typedef struct
{
    GPIO_TypeDef *GPIOx;
    uint16_t pin;
    uint8_t conversion_pending;
    uint32_t conversion_tick;
} DS18B20_Context;

static DS18B20_Context ds18b20_contexts[2] =
{
    {GPIOB, GPIO_PIN_6, 0U, 0U},
    {GPIOB, GPIO_PIN_7, 0U, 0U}
};

static uint8_t DS18B20_IsSinglePin(uint16_t pin)
{
    return (uint8_t)((pin != 0U) && ((pin & (uint16_t)(pin - 1U)) == 0U));
}

static uint32_t DS18B20_PinModeShift(uint16_t pin)
{
    uint32_t index = 0U;

    while (((pin & 1U) == 0U) && (index < 16U))
    {
        pin >>= 1;
        index++;
    }

    return index * 2U;
}

static void DS18B20_Release(GPIO_TypeDef *GPIOx, uint16_t pin)
{
    uint32_t shift = DS18B20_PinModeShift(pin);

    /* Set the output latch high before changing to input mode. */
    GPIOx->BSRR = pin;
    GPIOx->OTYPER |= pin;
    GPIOx->PUPDR &= ~(3UL << shift);
    GPIOx->MODER &= ~(3UL << shift);
}

static void DS18B20_DriveLow(GPIO_TypeDef *GPIOx, uint16_t pin)
{
    uint32_t shift = DS18B20_PinModeShift(pin);

    GPIOx->OTYPER |= pin;
    GPIOx->PUPDR &= ~(3UL << shift);
    GPIOx->BSRR = (uint32_t)pin << 16U;
    GPIOx->MODER = (GPIOx->MODER & ~(3UL << shift)) | (1UL << shift);
}

static void DS18B20_DelayUs(uint32_t us)
{
    DelayUs(us);
}

static uint32_t DS18B20_EnterCritical(void)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    return primask;
}

static void DS18B20_ExitCritical(uint32_t primask)
{
    if (primask == 0U)
    {
        __enable_irq();
    }
}

static uint8_t DS18B20_Reset(GPIO_TypeDef *GPIOx, uint16_t pin)
{
    uint32_t primask;
    uint8_t presence;

    primask = DS18B20_EnterCritical();

    DS18B20_Release(GPIOx, pin);
    DS18B20_DelayUs(5U);
    DS18B20_DriveLow(GPIOx, pin);
    DS18B20_DelayUs(500U);
    DS18B20_Release(GPIOx, pin);
    DS18B20_DelayUs(70U);
    presence = (uint8_t)(HAL_GPIO_ReadPin(GPIOx, pin) == GPIO_PIN_RESET);
    DS18B20_DelayUs(410U);

    DS18B20_ExitCritical(primask);
    return presence;
}

static void DS18B20_WriteBit(GPIO_TypeDef *GPIOx, uint16_t pin, uint8_t bit)
{
    uint32_t primask;

    primask = DS18B20_EnterCritical();
    DS18B20_DriveLow(GPIOx, pin);

    if (bit != 0U)
    {
        DS18B20_DelayUs(6U);
        DS18B20_Release(GPIOx, pin);
        DS18B20_DelayUs(64U);
    }
    else
    {
        DS18B20_DelayUs(60U);
        DS18B20_Release(GPIOx, pin);
        DS18B20_DelayUs(10U);
    }

    DS18B20_ExitCritical(primask);
}

static uint8_t DS18B20_ReadBit(GPIO_TypeDef *GPIOx, uint16_t pin)
{
    uint32_t primask;
    uint8_t bit;

    primask = DS18B20_EnterCritical();
    DS18B20_DriveLow(GPIOx, pin);
    DS18B20_DelayUs(3U);
    DS18B20_Release(GPIOx, pin);
    DS18B20_DelayUs(10U);
    bit = (uint8_t)(HAL_GPIO_ReadPin(GPIOx, pin) == GPIO_PIN_SET);
    DS18B20_DelayUs(53U);
    DS18B20_ExitCritical(primask);

    return bit;
}

static void DS18B20_WriteByte(GPIO_TypeDef *GPIOx, uint16_t pin, uint8_t value)
{
    uint8_t i;

    for (i = 0U; i < 8U; i++)
    {
        DS18B20_WriteBit(GPIOx, pin, (uint8_t)(value & 0x01U));
        value >>= 1;
    }
}

static uint8_t DS18B20_ReadByte(GPIO_TypeDef *GPIOx, uint16_t pin)
{
    uint8_t i;
    uint8_t value = 0U;

    for (i = 0U; i < 8U; i++)
    {
        if (DS18B20_ReadBit(GPIOx, pin) != 0U)
        {
            value |= (uint8_t)(1U << i);
        }
    }

    return value;
}

static uint8_t DS18B20_Crc8(const uint8_t *data, uint8_t length)
{
    uint8_t crc = 0U;
    uint8_t i;
    uint8_t mix;
    uint8_t inbyte;

    while (length-- != 0U)
    {
        inbyte = *data;
        for (i = 0U; i < 8U; i++)
        {
            mix = (uint8_t)((crc ^ inbyte) & 0x01U);
            crc >>= 1;
            if (mix != 0U)
            {
                crc ^= 0x8CU; /* reflected polynomial 0x31 */
            }
            inbyte >>= 1;
        }
        data++;
    }

    return crc;
}

static DS18B20_Context *DS18B20_FindContext(GPIO_TypeDef *GPIOx,
                                            uint16_t pin)
{
    uint32_t i;

    for (i = 0U; i < (sizeof(ds18b20_contexts) / sizeof(ds18b20_contexts[0])); i++)
    {
        if ((ds18b20_contexts[i].GPIOx == GPIOx) &&
            (ds18b20_contexts[i].pin == pin))
        {
            return &ds18b20_contexts[i];
        }
    }

    return 0;
}

static DS18B20_Status DS18B20_StartConversion(DS18B20_Context *context)
{
    if ((context == 0) || (context->GPIOx == 0) ||
        !DS18B20_IsSinglePin(context->pin))
    {
        return DS18B20_INVALID_PARAMETER;
    }

    if (DS18B20_Reset(context->GPIOx, context->pin) == 0U)
    {
        context->conversion_pending = 0U;
        return DS18B20_NO_DEVICE;
    }

    /* Each one-wire bus has one sensor, so Skip ROM is appropriate. */
    DS18B20_WriteByte(context->GPIOx, context->pin, 0xCCU);
    DS18B20_WriteByte(context->GPIOx, context->pin, 0x44U);
    context->conversion_tick = HAL_GetTick();
    context->conversion_pending = 1U;

    return DS18B20_BUSY;
}

static DS18B20_Status DS18B20_ReadScratchpad(GPIO_TypeDef *GPIOx,
                                             uint16_t pin,
                                             int16_t *raw_temperature)
{
    uint8_t scratchpad[9];
    uint8_t i;
    uint16_t raw;

    if ((GPIOx == 0) || !DS18B20_IsSinglePin(pin) || (raw_temperature == 0))
    {
        return DS18B20_INVALID_PARAMETER;
    }

    if (DS18B20_Reset(GPIOx, pin) == 0U)
    {
        return DS18B20_NO_DEVICE;
    }

    DS18B20_WriteByte(GPIOx, pin, 0xCCU);
    DS18B20_WriteByte(GPIOx, pin, 0xBEU);

    for (i = 0U; i < 9U; i++)
    {
        scratchpad[i] = DS18B20_ReadByte(GPIOx, pin);
    }

    if (DS18B20_Crc8(scratchpad, 8U) != scratchpad[8])
    {
        return DS18B20_CRC_ERROR;
    }

    raw = (uint16_t)scratchpad[0] |
          ((uint16_t)scratchpad[1] << 8U);
    *raw_temperature = (int16_t)raw;
    return DS18B20_OK;
}

/*
 * Start a conversion on the first call, then return BUSY while the DS18B20
 * converts. Once the conversion is ready, read the scratchpad and start the
 * next conversion immediately. No 750 ms blocking delay is used here.
 */
static DS18B20_Status DS18B20_ReadRaw(GPIO_TypeDef *GPIOx,
                                      uint16_t pin,
                                      int16_t *raw_temperature)
{
    DS18B20_Context *context;
    DS18B20_Status status;

    if ((GPIOx == 0) || !DS18B20_IsSinglePin(pin) || (raw_temperature == 0))
    {
        return DS18B20_INVALID_PARAMETER;
    }

    context = DS18B20_FindContext(GPIOx, pin);
    if (context == 0)
    {
        return DS18B20_INVALID_PARAMETER;
    }

    if (context->conversion_pending == 0U)
    {
        return DS18B20_StartConversion(context);
    }

    if ((uint32_t)(HAL_GetTick() - context->conversion_tick) <
        DS18B20_CONVERSION_TIME_MS)
    {
        return DS18B20_BUSY;
    }

    context->conversion_pending = 0U;
    status = DS18B20_ReadScratchpad(GPIOx, pin, raw_temperature);

    /* Keep the sensor running continuously for the next periodic read. */
    if (status != DS18B20_NO_DEVICE)
    {
        (void)DS18B20_StartConversion(context);
    }

    return status;
}

void DS18B20_Init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();
    DS18B20_Release(GPIOB, GPIO_PIN_6);
    DS18B20_Release(GPIOB, GPIO_PIN_7);
    ds18b20_contexts[0].conversion_pending = 0U;
    ds18b20_contexts[1].conversion_pending = 0U;
}

DS18B20_Status DS18B20_ReadTemperature(GPIO_TypeDef *GPIOx,
                                       uint16_t GPIO_Pin,
                                       float *temperature_c)
{
    DS18B20_Status status;
    int16_t raw_temperature;

    if (temperature_c == 0)
    {
        return DS18B20_INVALID_PARAMETER;
    }

    status = DS18B20_ReadRaw(GPIOx, GPIO_Pin, &raw_temperature);
    if (status == DS18B20_OK)
    {
        *temperature_c = (float)raw_temperature / 16.0f;
    }

    return status;
}

DS18B20_Status DS18B20_ReadTemperatureX100(GPIO_TypeDef *GPIOx,
                                           uint16_t GPIO_Pin,
                                           int16_t *temperature_centi)
{
    DS18B20_Status status;
    int16_t raw_temperature;

    if (temperature_centi == 0)
    {
        return DS18B20_INVALID_PARAMETER;
    }

    status = DS18B20_ReadRaw(GPIOx, GPIO_Pin, &raw_temperature);
    if (status == DS18B20_OK)
    {
        *temperature_centi = (int16_t)(((int32_t)raw_temperature * 100L) / 16L);
    }

    return status;
}
