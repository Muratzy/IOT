#ifndef LUNAR_DELAY_H
#define LUNAR_DELAY_H

#include "stm32f4xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

void Delay_Init(void);
void Delay(uint32_t ms);
uint32_t GetTick(void);
uint64_t GetUs(void);
void DelayUs(uint32_t us);

/*
 * Rate-limit a void task. The first call runs immediately. This macro must
 * be used inside a void function because it returns from that function.
 */
#define PERIODIC(T)                                                   \
    do                                                                \
    {                                                                 \
        static uint32_t periodic_next_tick = 0U;                     \
        uint32_t periodic_now_tick = HAL_GetTick();                  \
        if ((int32_t)(periodic_now_tick - periodic_next_tick) < 0)   \
        {                                                             \
            return;                                                   \
        }                                                             \
        periodic_next_tick = periodic_now_tick + (uint32_t)(T);      \
    } while (0)

/* Execute a block at most once every T milliseconds. */
#define PERIODIC_START(NAME, T)                                      \
    do                                                                \
    {                                                                 \
        static uint32_t NAME##_next_tick = 0U;                       \
        uint32_t NAME##_now_tick = HAL_GetTick();                    \
        if ((int32_t)(NAME##_now_tick - NAME##_next_tick) >= 0)       \
        {                                                             \
            NAME##_next_tick = NAME##_now_tick + (uint32_t)(T);       \

#define PERIODIC_END                                                  \
        }                                                             \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif /* LUNAR_DELAY_H */
