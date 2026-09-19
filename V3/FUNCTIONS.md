# V3 主要函数说明

## 主循环任务

| 函数 | 周期 | 作用 |
|---|---:|---|
| `Temperature_Task()` | 750 ms | 非阻塞读取 PB6/PB7 两路 DS18B20，并刷新 OLED 第一行。 |
| `Pressure_Task()` | 100 ms | 更新 PA1 压力值，显示 Pa、当前 ADC 和零点差值。 |
| `Water_Task()` | 100 ms | 读取 TIM2 硬件脉冲计数并刷新瞬时流量。 |
| `MOTOR_Slice(60, 8000)` | 主循环反复调用 | 以 60% 占空比运行 8 秒后停止，不阻塞 OLED 和传感器任务。 |
| `NETWORK_SendSensorData()` | 250 ms | 将 OLED 对应数据格式化并经 USART1 发给 CH9121。 |

`PERIODIC(T)` 用于 void 任务的周期控制；未到执行时间时直接从当前任务返回。`PERIODIC_START/END` 用于在主循环中包围周期代码块。

## 温度模块

| 函数 | 作用 |
|---|---|
| `DS18B20_Init()` | 初始化 PB6、PB7 两条单总线。 |
| `DS18B20_ReadTemperature()` | 启动或读取异步温度转换，返回 `OK/BUSY/NO_DEVICE/CRC_ERROR` 等状态。 |
| `Temperature_ToX10()` | 将浮点温度转换为 OLED 与网络共用的 0.1 °C 定点值。 |
| `OLED_ShowTemperatureField()` | 按 `T1/T2` 格式显示温度、等待或错误状态。 |

## 压力模块

| 函数 | 作用 |
|---|---|
| `PRESSURE_Init()` | 在水路零压时采集 4 组、每组 64 次 ADC，建立启动零点。 |
| `PRESSURE_Update()` | 每次平均 64 次 ADC；取相对零点差值的绝对值，换算为 Pa并进行滑动平均。 |
| `PRESSURE_GetPa()` | 返回 OLED 和 V3 串口帧使用的 Pa 原值。 |
| `PRESSURE_GetRawADC()` | 返回 PA1 最新平均 ADC。 |
| `PRESSURE_GetDeltaADC()` | 返回当前 ADC 相对启动零点的带符号差值。 |
| `PRESSURE_GetStatus()` | 返回等待、正常、ADC 错误或传感器错误状态。 |

压力换算沿用参考工程的标定：

```text
Pa = |sample_sum - zero_sum| × 1238000 ÷ (4095 × 64) × 1.32
```

## 流量模块

| 函数 | 作用 |
|---|---|
| `WATER_Init()` | 清零并启动 TIM2 外部时钟计数。 |
| `WATER_Update()` | 根据 100 ms 内的脉冲差计算瞬时流量。 |
| `WATER_GetFlowCentiLMin()` | 返回 0.01 L/min 定点流量，供 OLED 和网络共用。 |
| `WATER_GetLastPulseCount()` | 返回最近周期的脉冲数量。 |

YF-S401 标定常数为 `3433.8 pulse/L`。

## 水泵模块

| 函数 | 作用 |
|---|---|
| `MOTOR_Init()` | 启动 TIM3_CH1 PWM，PA7 保持低电平。 |
| `MOTOR_SetDuty(percent)` | 将 `0~100` 直接映射为 `0%~100%` PWM。 |
| `MOTOR_Stop()` | 将 PWM 比较值清零并保持 PA7 低电平。 |
| `MOTOR_Slice(duty, duration_ms)` | 非阻塞定时运行；完成后保持 `DONE`，避免主循环重复启动。 |
| `MOTOR_SliceReset()` | 清除完成状态，允许重新执行相同切片。 |

## 网络模块

| 函数/结构 | 作用 |
|---|---|
| `NETWORK_SensorFrame` | 保存两路温度、流量、压力、ADC 差值及各模块状态。 |
| `NETWORK_Init()` | 保存 CH9121 所连接的 UART 句柄。 |
| `NETWORK_SendString()` | 发送普通字符串。 |
| `NETWORK_SendSensorData()` | 生成紧凑 V3 文本帧并通过 USART1 阻塞发送。 |

网络帧保持整数和定点格式，不依赖 Keil 浮点 `printf`。压力直接使用 Pa，避免旧版 `0.001 MPa` 造成的 1000 Pa 量化。

## 上位机

| 函数 | 作用 |
|---|---|
| `parse_frame()` | 解析 V3 OLED 同步帧，同时兼容旧版 `TEMP/PRESS/FLOW` 帧。 |
| `tcp_server()` | 在 TCP 1000 端口接受 CH9121 连接，按换行符拆分串口帧。 |
| `web_server()` | 在 HTTP 8000 提供网页和 `/api/data` JSON。 |
| `cli_monitor.fetch_data()` | 从本地 `/api/data` 获取最近数据，不与 CH9121 抢占 TCP 端口。 |
| `cli_monitor.render()` | 仅显示 T1、T2、流量和压力。 |
