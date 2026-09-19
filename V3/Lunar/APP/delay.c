#include "delay.h"

static uint8_t delay_dwt_ready = 0U;
static uint32_t delay_cycles_per_us = 1U;
static uint32_t delay_last_cycle = 0U;
static uint32_t delay_cycle_remainder = 0U;
static uint64_t delay_elapsed_us = 0ULL;

static void Delay_EnableDwt(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    __DSB();
    __ISB();
}

void Delay_Init(void)
{
    SystemCoreClockUpdate();

    delay_cycles_per_us = SystemCoreClock / 1000000U;
    if (delay_cycles_per_us == 0U)
    {
        delay_cycles_per_us = 1U;
    }

    Delay_EnableDwt();
    delay_last_cycle = DWT->CYCCNT;
    delay_cycle_remainder = 0U;
    delay_elapsed_us = 0ULL;
    delay_dwt_ready = 1U;
}

void Delay(uint32_t ms)
{
    HAL_Delay(ms);
}

uint32_t GetTick(void)
{
    return HAL_GetTick();
}

uint64_t GetUs(void)
{
    uint32_t now_cycle;
    uint32_t delta_cycle;
    uint64_t cycles;

    if (delay_dwt_ready == 0U)
    {
        Delay_Init();
    }

    now_cycle = DWT->CYCCNT;
    delta_cycle = now_cycle - delay_last_cycle;
    delay_last_cycle = now_cycle;

    cycles = (uint64_t)delay_cycle_remainder + (uint64_t)delta_cycle;
    delay_elapsed_us += cycles / (uint64_t)delay_cycles_per_us;
    delay_cycle_remainder = (uint32_t)(cycles % (uint64_t)delay_cycles_per_us);

    return delay_elapsed_us;
}

void DelayUs(uint32_t us)
{
    uint32_t start_cycle;
    uint32_t delay_cycles;
    uint32_t chunk_us;
    uint32_t max_chunk_us;

    if (delay_dwt_ready == 0U)
    {
        Delay_Init();
    }

    max_chunk_us = 0x7FFFFFFFU / delay_cycles_per_us;
    if (max_chunk_us == 0U)
    {
        max_chunk_us = 1U;
    }

    while (us != 0U)
    {
        chunk_us = (us > max_chunk_us) ? max_chunk_us : us;
        delay_cycles = chunk_us * delay_cycles_per_us;
        start_cycle = DWT->CYCCNT;

        while ((uint32_t)(DWT->CYCCNT - start_cycle) < delay_cycles)
        {
        }

        us -= chunk_us;
    }
}
