# 水泵 PWM 与定时控制

## 硬件模式

当前水泵只支持单向 PWM：

- PA6 / TIM3_CH1 -> 驱动板 IN1，输出 PWM。
- PA7 -> IN2，程序始终写低。
- `0` 表示关闭，`1~100` 直接表示 `1%~100%` 占空比。

## 函数选择

| 需求 | 函数 |
|---|---|
| 立即设一个占空比，不自动停 | `MOTOR_SetDuty(percent)` |
| 兼容旧调用，正数运行、非正数停 | `MOTOR_SetSpeed(speed)` |
| 立即停泵 | `MOTOR_Stop()` |
| 占空比+持续时间，不阻塞 | `MOTOR_Slice(duty, duration_ms)` |
| 重复执行同一组切片参数 | 先 `MOTOR_SliceReset()`，再 `MOTOR_Slice()` |
| 查实际 PWM | `MOTOR_GetDuty()` |
| 查运行/完成状态 | `MOTOR_GetSliceStatus()` |
| 查剩余时间 | `MOTOR_GetRemainingMs()` |

## `MOTOR_Slice()` 的状态机

1. 状态是 `IDLE`，或新的 duty/duration 与旧值不同：记录起始 tick，启动 PWM，进入 `RUNNING`。
2. 在 `RUNNING` 期间，主循环必须反复调用同样的 `MOTOR_Slice(duty, duration)`。
3. `HAL_GetTick() - start >= duration`时自动 `MOTOR_Stop()`，进入 `DONE`。
4. `DONE` 后继续传同样参数不会重启，避免主循环把水泵不断重新计时。

## 本地测试示例

如果要在不经过网页的情况下测试“30% 运行 2 秒”，不要用 `HAL_Delay(2000)`。在 `main.c` 的 `USER CODE BEGIN 0` 中可临时写：

```c
static void Pump_TestTask(void)
{
    static uint8_t started = 0U;

    if (started == 0U)
    {
        MOTOR_SliceReset();
        started = 1U;
    }

    if (MOTOR_GetSliceStatus() == MOTOR_SLICE_IDLE ||
        MOTOR_GetSliceStatus() == MOTOR_SLICE_RUNNING)
    {
        (void)MOTOR_Slice(30U, 2000U);
    }
}
```

然后在 while 中调用 `Pump_TestTask()`。OLED 和传感器仍可刷新。测试完成后应删除这个临时调用，避免与网页命令同时控制水泵。

## 网页命令执行逻辑

`Pump_Task()` 每次 while 执行：

1. `NETWORK_ReadPumpCommand()` 检查是否有完整命令。
2. 合法命令：先 `MOTOR_SliceReset()`，再保存 duty/time。
3. duty > 0 时调用 `MOTOR_Slice()`；duty=0 时 Reset 已经把水泵停掉。
4. 发 `PUMP_ACK=...,TIME=...,OK=1`。
5. 之后每圈主循环在 `RUNNING` 状态内继续调用 `MOTOR_Slice()`，直到到期停泵。

## 修改 PWM 频率

应在 `Lunar.ioc` 中改 TIM3 Prescaler/Period，不要只手改生成的 `MX_TIM3_Init()`。

```text
PWM Hz = TIM3 timer clock / (Prescaler + 1) / (Period + 1)
```

当前 TIM3 timer clock 84 MHz，PSC=5，ARR=57343，约 244 Hz。`MOTOR_DutyToCompare()` 会自动读 ARR，所以修改频率后 `1~100%` 的百分比映射仍然有效。

## 保护建议

- 网页后端已限制 duty `0~100`，duration 最大 `3600000 ms`。
- 比赛调试先用低占空比、短时间。
- 修改水泵逻辑后，必须检查“立即停止”按钮还能发 `PUMP=0,TIME=0`。
- 如果水泵上电就动，先检查 `MOTOR_Init()` 是否在启动 PWM 前把 CCR 清 0，PA7 是否为低。
