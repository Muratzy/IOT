# 传感器与 OLED

## 温度 DS18B20

### 当前逻辑

- PB6 和 PB7 各一条独立单总线。
- `DS18B20_ReadTemperature()` 是非阻塞状态机。
- 第一次调用启动转换，返回 `DS18B20_BUSY`。
- 750 ms 后读 scratchpad、校验 CRC，返回 `DS18B20_OK`，然后立即启动下一轮转换。
- `Temperature_Task()` 把数值显示到 OLED 第 1 行。

### 应改哪里

| 目标 | 修改位置 |
|---|---|
| 改刷新周期 | `main.c -> Temperature_Task() -> PERIODIC(750U)` |
| 改显示小数位/位置 | `main.c -> OLED_ShowTemperatureField()` |
| 改传感器引脚 | `Temprature.c -> ds18b20_contexts[]`，同时改 CubeMX |
| 改转换等待时间 | `Temprature.c -> DS18B20_CONVERSION_TIME_MS` |
| 要 0.01°C 定点值 | 调用 `DS18B20_ReadTemperatureX100()` |

### 状态码

| 状态 | OLED | 含义 |
|---|---|---|
| `DS18B20_OK` | 正常温度 | 数据和 CRC 正常 |
| `DS18B20_BUSY` | `WAIT` | 转换尚未完成 |
| `DS18B20_NO_DEVICE` | `ERR` | 复位时没检测到 presence |
| `DS18B20_CRC_ERROR` | `ERR` | 单总线干扰或时序/接线问题 |

## 压力 PA1/ADC1

### 当前换算

`PRESSURE_Init()` 在零压时连续取 4 批，每批 64 次 ADC，建立 `pressure_zero_adc`。

`PRESSURE_Update()` 的核心是：

```text
delta_sum = current_sum - zero_sum
pressure = abs(delta_sum) * 1,238,000 / (4095 * 64) * 1.32
```

当前取绝对值，因此同时兼容“压力增大时电压上升”和“电压下降”的模块。`D` 保留原始方向，方便诊断。

### 关键宏

| 宏 | 当前值 | 作用 |
|---|---:|---|
| `PRESSURE_SAMPLE_COUNT` | 64 | 每次 Update 的 ADC 采样数 |
| `PRESSURE_FILTER_COUNT` | 32 | Pa 滑动平均长度 |
| `PRESSURE_SPAN_PA` | 1,238,000 | 换算量程常数 |
| `PRESSURE_GAIN_X1000` | 1320 | 1.32 增益 |
| `PRESSURE_MAX_PA` | 1,000,000 | 输出上限 |
| `PRESSURE_ZERO_BATCH_COUNT` | 4 | 上电零点批数 |

改滤波时，值越大越稳，但反应越慢。比赛现场先看 `A` 和 `D`：

- `A` 不变：优先查硬件、PA1 和 ADC。
- `A` 变而 `D` 很小：传感器输出幅度小或零点在有压时建立。
- `D` 变但 Pa 反应慢：减小 `PRESSURE_FILTER_COUNT`。
- 静态 Pa 不为 0 但只有数百/一千：这可能是 ADC 噪声经换算后的结果，先看 `D` 是否只跳几个计数。

## 流量 PA0/TIM2

### 当前公式

```text
L/min = 采样期内脉冲数 * 60000
        / (3433.8 * 实际采样毫秒数)
```

`WATER_Update()` 自己用实际 `elapsed_ms` 计算，所以即使主循环有少量抖动，流量也不会因假定固定 100 ms 而偏移。

### 应改哪里

| 目标 | 修改位置 |
|---|---|
| 更换流量计型号/标定 | `WATER.h -> WATER_PULSES_PER_LITER` |
| 改最小采样窗 | `WATER.c -> WATER_MIN_SAMPLE_MS` |
| 改平滑程度 | `WATER.c -> WATER_FILTER_SAMPLES` |
| 改 OLED 格式 | `main.c -> Water_Task()` |
| 查是否有脉冲 | 临时显示/上传 `WATER_GetLastPulseCount()` |

## OLED

OLED 是 4 行 x 16 字符的 8x16 英文字库显示。

| 函数 | 用途 |
|---|---|
| `OLED_ShowString(line, column, text)` | 显示 ASCII 字符串 |
| `OLED_ShowNum(...)` | 固定长度无符号整数 |
| `OLED_ShowSignedNum(...)` | 显示正负号+整数 |
| `OLED_ShowChar(...)` | 更新单个字符 |
| `OLED_Clear()` | 全屏清空，不建议在周期任务中频繁调用 |

更新字段时要用空格覆盖旧字符，否则新内容更短时会留下尾巴。例如：

```c
OLED_ShowString(2, 1, "Flow:WAIT       ");
```

## OLED 当前四行对应

1. T1 / T2 温度。
2. `Flow:xx.xx L/min`。
3. `P:xxxxxxx Pa`。
4. `A:xxxx D:+/-xxxx`。
