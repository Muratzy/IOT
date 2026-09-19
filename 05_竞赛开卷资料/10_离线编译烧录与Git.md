# 离线编译、烧录与 Git

## Keil 工程

打开：

```text
Lunar/MDK-ARM/Lunar.uvprojx
```

当前使用 ARMCC 5 工具链。断网前要确认 Keil、ARM Compiler 5、J-Link 驱动和设备包已安装，比赛现场无法临时下载。

## 编译产物

```text
Lunar/MDK-ARM/Lunar/Lunar.axf
Lunar/MDK-ARM/Lunar/Lunar.hex
```

`.uvprojx` 已开启 Create HEX File。编译完成后检查 Build Output 的 error/warning，不要只看 HEX 文件时间。

## 每次修改后的最小验证

1. Keil Rebuild：`0 Error(s)`。
2. 烧录并复位 STM32。
3. OLED 检查四行。
4. Python 看一条完整 `[RX]`。
5. `/api/data` 看 JSON。
6. 网页检查与 OLED 一致。
7. 如果改了控制，先用低 PWM/短时间测试，再验证立即停止。

## CubeMX 重新生成前

1. 备份或 Git commit。
2. 确认 `main.c` 手写代码都在 `USER CODE` 标记中。
3. 确认 `Lunar/APP/*.c/.h` 已被 Keil 工程组引用。
4. 生成后先用 Git diff 检查是否出现大量无关改动。
5. 重新编译。

## 比赛时的 Git 安全用法

### 查当前改了什么

```powershell
git status --short
git diff -- Lunar 04_上位机与调试
```

### 只暂存确定的文件

```powershell
git add -- Lunar/APP/MOTOR.c Lunar/Core/Src/main.c
git diff --cached
```

不要在工作区有大量无关变更时盲目使用 `git add .`。

### 做一个现场保险提交

```powershell
git commit -m "Checkpoint before pressure tuning"
```

断网时 commit 依然可以在本地完成；push 等恢复网络再做。

## 只恢复一个还没暂存的文件

先用 `git diff -- <file>` 确认，再执行：

```powershell
git restore -- Lunar/APP/MOTOR.c
```

这会丢失该文件未提交的修改，所以必须先看 diff。

## 用已知正常的 HEX 快速恢复

如果源码临时调坏但需先恢复演示：

1. 保存当前代码和 diff，不要直接覆盖。
2. 从已验证版本取 `.hex`。
3. 用 Keil/J-Link 只烧录 HEX。
4. 先恢复现场演示，再在副本中定位源码问题。

## Python/前端离线验证

```powershell
python -m py_compile .\04_上位机与调试\tcp_web_bridge_192.168.1.142.py
python -m py_compile .\04_上位机与调试\cli_monitor.py
```

`dashboard.html` 没有编译步骤，需启动 Python 后在浏览器 F12 Console/Network 中验证。

## 比赛前必须本地保存的东西

- 完整项目目录。
- 已验证 HEX。
- Keil 安装包/工具链。
- J-Link/ST-Link 驱动和烧录软件。
- Python 安装程序（本项目桥接只使用标准库）。
- CH9121 配置工具和已验证配置截图/文档。
- 本 `05_竞赛开卷资料/` 目录的离线副本。
