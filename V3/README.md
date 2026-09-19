# IOTCM V3

V3 将 STM32 数据采集、CH9121 有线入网、电脑 Wi-Fi 接收、网页显示和 CLI 显示整理为一个可复现版本。

## 已实现功能

- STM32F401RCT6 同时采集两路 DS18B20 温度。
- PA0 / TIM2 统计 YF-S401 流量脉冲。
- PA1 / ADC1 采集压力，OLED 显示 Pa、ADC 当前值和零点差值。
- PA6 / TIM3_CH1 输出单向水泵 PWM，`1~100` 对应 `1%~100%`。
- OLED 使用 PB10 / PB3 的 I2C2，每个传感器任务独立周期刷新。
- USART1 每 250 ms 向 CH9121 发送一帧 OLED 同步数据。
- CH9121 通过网线接入路由器，电脑通过同一路由器的 Wi-Fi 接收 TCP 数据。
- 网页显示 T1、T2、流量、压力和压力 ADC 诊断。
- CLI 只显示 T1、T2、流量和压力。

## 目录

```text
V3/
├─ Lunar/                    STM32CubeMX + Keil 工程
├─ Firmware/Lunar_V3.hex     已通过 Keil 编译的固件
├─ Host/
│  ├─ dashboard.html         实时网页
│  ├─ tcp_web_bridge_192.168.1.142.py
│  ├─ cli_monitor.py
│  └─ 启动CLI监测.cmd
├─ NETWORK_CONFIG.md         CH9121 和电脑网络参数
└─ FUNCTIONS.md              主要函数与数据链说明
```

## 快速使用

1. 使用 Keil 打开 `Lunar/MDK-ARM/Lunar.uvprojx`，或直接烧录 `Firmware/Lunar_V3.hex`。
2. 按 `NETWORK_CONFIG.md` 配置 CH9121。
3. 在 `Host/` 中运行：

   ```powershell
   python tcp_web_bridge_192.168.1.142.py
   ```

4. 浏览器打开 <http://127.0.0.1:8000>。
5. 需要终端界面时运行 `启动CLI监测.cmd`，或者：

   ```powershell
   python cli_monitor.py
   ```

## V3 串口帧

```text
T1=+21.2,S1=0,T2=+21.4,S2=0,F=00.35,SF=1,P=0000399,SP=1,A=0413,D=+0001
```

| 字段 | 含义 | 单位 |
|---|---|---|
| `T1` / `T2` | 两路温度 | °C，1 位小数 |
| `S1` / `S2` | DS18B20 状态码 | 枚举值 |
| `F` | 瞬时流量 | L/min，2 位小数 |
| `SF` | 流量模块状态码 | 枚举值 |
| `P` | 压力原值 | Pa |
| `SP` | 压力模块状态码 | 枚举值 |
| `A` | PA1 当前 ADC | 0~4095 |
| `D` | 当前 ADC 相对启动零点的差值 | ADC 计数 |

V3 直接传输 Pa，修复了旧版先换算成 `0.001 MPa` 导致压力只能按 1000 Pa 跳变的问题。

## 验证结果

- Keil ARMCC 5.06 update 7：`0 Error(s), 0 Warning(s)`。
- 新版 OLED 同步帧和旧版 `TEMP/PRESS/FLOW` 帧解析测试通过。
- CH9121 与电脑 TCP `1000` 端口建立连接后，可同时使用网页和 CLI 查看数据。
