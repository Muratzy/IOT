# IOTCM V4

V4 是当前已经完成真机联调的版本，包含 STM32 数据采集、OLED 显示、CH9121 网络透传、网页/CLI 显示，以及网页控制水泵 PWM 和运行时间的完整链路。

## 已实现功能

- STM32F401RCT6 采集 PB6、PB7 两路 DS18B20 温度。
- PA0 / TIM2 统计 YF-S401 流量脉冲。
- PA1 / ADC1 采集压力并在 OLED 显示 Pa、ADC 当前值和零点差值。
- PA6 / TIM3_CH1 输出单向水泵 PWM，`0~100` 对应 `0%~100%`。
- OLED 使用 PB10 / PB3 的 I2C2，传感器和水泵运行期间持续刷新。
- USART1 每 250 ms 向 CH9121 发送温度、流量、压力和水泵状态。
- 网页可设置水泵占空比和运行时间，`0%` 为立即关闭。
- 网页右上角可关闭当前 Python 桥接服务，完整释放 TCP `1000` 和 HTTP `8000`。
- Windows 单实例锁会拒绝重复启动，避免新旧进程抢占端口造成数据停更。
- STM32 使用 USART1 中断接收命令，水泵定时运行不阻塞传感器任务。
- STM32 返回命令 ACK、实际占空比、执行状态和剩余时间。
- CLI 显示温度 1、温度 2、流量和压力。

## 目录

```text
V4/
├─ Lunar/                    STM32CubeMX + Keil 工程
├─ Firmware/Lunar_V4.hex     已通过 Keil 编译并完成真机烧录验证的固件
├─ Host/
│  ├─ dashboard.html         实时监控与水泵控制网页
│  ├─ tcp_web_bridge_192.168.1.142.py
│  ├─ cli_monitor.py
│  └─ 启动CLI监测.cmd
├─ NETWORK_CONFIG.md         CH9121、电脑网络及控制接口参数
└─ FUNCTIONS.md              主要函数、协议和数据链说明
```

## 快速使用

1. 使用 Keil 打开 `Lunar/MDK-ARM/Lunar.uvprojx`，或直接烧录 `Firmware/Lunar_V4.hex`。
2. 按 `NETWORK_CONFIG.md` 配置 CH9121。
3. 电脑连接同一路由器 Wi-Fi，在 `Host/` 中运行：

   ```powershell
   python tcp_web_bridge_192.168.1.142.py
   ```

4. 浏览器打开 <http://127.0.0.1:8000>。
5. 在“水泵控制”区域设置 PWM 和运行秒数，点击“发送到 STM32”；点击“立即关闭”会发送 0% 命令。
6. 需要停止桥接程序时点击页面右上角“关闭服务”；重新运行脚本后刷新页面即可恢复。
7. 需要终端显示时运行 `启动CLI监测.cmd`，或者执行 `python cli_monitor.py`。

## V4 串口协议

STM32 周期状态帧：

```text
T1=+19.1,S1=0,T2=+20.3,S2=0,F=00.35,SF=1,P=0000933,SP=1,A=0408,D=+0005,M=060,MS=1,MT=0000001750
```

网页下发水泵命令：

```text
PUMP=60,TIME=2000
```

STM32 确认：

```text
PUMP_ACK=60,TIME=2000,OK=1
```

| 字段 | 含义 | 单位/取值 |
|---|---|---|
| `T1` / `T2` | 两路温度 | °C，1 位小数 |
| `F` | 瞬时流量 | L/min，2 位小数 |
| `P` | 压力原值 | Pa |
| `A` / `D` | 当前 ADC / 相对零点差值 | ADC 计数 |
| `M` | 水泵实际占空比 | `0~100` |
| `MS` | 水泵切片状态 | `0` 停止、`1` 运行、`2` 完成 |
| `MT` | 本次运行剩余时间 | ms |

## 验证结果

- Keil ARMCC 5.06 update 7：`0 Error(s), 0 Warning(s)`。
- 固件已完成 J-Link 烧录和校验。
- CH9121 `192.168.1.200` 到电脑 TCP `1000` 的连接已验证。
- 网页、Python 桥接、STM32 命令 ACK 和 v3 状态帧已完成闭环测试。
- 真机以 `1%` PWM 运行 1 秒后自动停止，并验证网页“立即关闭”命令。
