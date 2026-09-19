# V4 主要函数说明

## 主循环任务

| 函数 | 周期 | 作用 |
|---|---:|---|
| `Temperature_Task()` | 750 ms | 非阻塞读取 PB6/PB7 两路 DS18B20，并刷新 OLED 第一行。 |
| `Pressure_Task()` | 100 ms | 更新 PA1 压力值，显示 Pa、当前 ADC 和零点差值。 |
| `Water_Task()` | 100 ms | 读取 TIM2 硬件脉冲计数并刷新瞬时流量。 |
| `Pump_Task()` | 每次主循环 | 读取水泵命令、启动或停止非阻塞切片并发送 ACK。 |
| `NETWORK_SendSensorData()` | 250 ms | 发送温度、流量、压力和水泵状态帧。 |

`PERIODIC(T)` 在未到周期时直接从当前任务返回，不阻塞其他任务；`PERIODIC_START/END` 用于主循环内的周期代码块。`Pump_Task()` 每圈执行，因此定时水泵运行期间 OLED 和传感器仍会继续刷新。

## 温度模块

| 函数 | 作用 |
|---|---|
| `DS18B20_Init()` | 初始化 PB6、PB7 两条单总线。 |
| `DS18B20_ReadTemperature()` | 启动或读取异步温度转换，返回 `OK/BUSY/NO_DEVICE/CRC_ERROR` 等状态。 |
| `Temperature_ToX10()` | 将温度转换为 OLED 和网络共用的 0.1 °C 定点值。 |
| `OLED_ShowTemperatureField()` | 显示温度、等待或错误状态。 |

## 压力和流量模块

| 函数 | 作用 |
|---|---|
| `PRESSURE_Init()` | 在水路零压时采样建立启动零点。 |
| `PRESSURE_Update()` | 平均 ADC、计算相对零点差值并换算为 Pa。 |
| `PRESSURE_GetPa()` | 返回 OLED 和网络共用的压力 Pa 原值。 |
| `PRESSURE_GetRawADC()` | 返回 PA1 最新平均 ADC。 |
| `PRESSURE_GetDeltaADC()` | 返回当前 ADC 相对启动零点的带符号差值。 |
| `WATER_Init()` | 清零并启动 TIM2 外部时钟计数。 |
| `WATER_Update()` | 根据 100 ms 内的脉冲差计算瞬时流量。 |
| `WATER_GetFlowCentiLMin()` | 返回 0.01 L/min 定点流量。 |

压力换算沿用参考工程的 `1238000 Pa` 满量程和 `1.32` 增益；YF-S401 标定常数为 `3433.8 pulse/L`。

## 水泵模块

| 函数 | 作用 |
|---|---|
| `MOTOR_Init()` | 启动 TIM3_CH1 PWM，PA7 保持低电平。 |
| `MOTOR_SetDuty(percent)` | 将 `0~100` 映射为 `0%~100%` PWM。 |
| `MOTOR_Stop()` | 清零 PWM 比较值。 |
| `MOTOR_Slice(duty, duration_ms)` | 非阻塞定时运行；到期停止并保持 `DONE`。 |
| `MOTOR_SliceReset()` | 停泵并清除旧切片状态，允许执行新命令。 |
| `MOTOR_GetDuty()` | 返回当前实际设置的 PWM 百分比。 |
| `MOTOR_GetSliceStatus()` | 返回停止、运行或完成状态。 |
| `MOTOR_GetRemainingMs()` | 返回运行切片的剩余毫秒数。 |

## STM32 网络模块

| 函数/结构 | 作用 |
|---|---|
| `NETWORK_Init()` | 保存 USART1 句柄并启动单字节中断接收。 |
| `HAL_UART_RxCpltCallback()` | 在中断中按换行符组装命令，避免阻塞主循环。 |
| `NETWORK_ReadPumpCommand()` | 读取并校验 `PUMP=<0~100>,TIME=<ms>`。 |
| `NETWORK_SendPumpAck()` | 返回命令占空比、时间和接受状态。 |
| `NETWORK_SensorFrame` | 保存全部传感器值、状态及水泵执行状态。 |
| `NETWORK_SendSensorData()` | 生成 v3 紧凑文本帧并经 USART1 发送。 |

接收缓存只保存一条完整命令；超长、格式错误或范围错误的命令会被拒绝。水泵最长单次运行时间为 `3600000 ms`。

## 上位机

| 函数 | 作用 |
|---|---|
| `parse_frame()` | 解析带 `M/MS/MT` 的 v3 状态帧，同时兼容旧状态帧。 |
| `parse_pump_ack()` | 解析 STM32 的 `PUMP_ACK` 确认。 |
| `send_pump_command()` | 通过当前 CH9121 TCP 连接向 STM32 下发命令。 |
| `tcp_server()` | 在 TCP 1000 接受 CH9121 连接并拆分串口文本行。 |
| `acquire_single_instance()` | 使用 Windows 命名互斥量阻止第二个桥接进程启动。 |
| `shutdown_service()` | 关闭 CH9121 Socket、TCP 监听和 HTTP 服务。 |
| `Handler.do_POST()` | 校验 `/api/pump` 请求并调用命令发送函数。 |
| `web_server()` | 在 HTTP 8000 提供网页、`/api/data`、`/api/pump` 和 `/api/shutdown`。 |
| `cli_monitor.fetch_data()` | 从 `/api/data` 获取最新数据，不占用 CH9121 TCP 连接。 |

网页显示 STM32 实际 PWM、运行状态、剩余时间和最后一次 ACK，而不是只显示本地设定值。
