# Python 后端修改指南

## 主文件

- 无线方案：`04_上位机与调试/tcp_web_bridge_192.168.1.142.py`
- 有线直连：`有线接收/Host/tcp_web_bridge_192.168.1.142.py`
- CLI：`04_上位机与调试/cli_monitor.py`

两个桥接版本使用同一数据协议和 API。有线版只在网卡绑定和启动恢复上有区别。

## 函数映射

| 想改的功能 | 函数/位置 |
|---|---|
| 改 TCP 监听地址/端口 | `TCP_HOST` / `TCP_PORT` |
| 改网页地址/端口 | `WEB_HOST` / `WEB_PORT` |
| 改页面文件 | `HTML_FILE` |
| 增加 API 数据字段 | `latest` |
| 改状态码文字 | `temperature_status()` / `pressure_status()` / `flow_status()` / `pump_status()` |
| 改 STM32 帧解析 | `parse_frame()` |
| 改 ACK 解析 | `parse_pump_ack()` |
| 改向 STM32 发的命令 | `send_pump_command()` |
| 改 TCP 接收和拆行 | `tcp_server()` |
| 增加 GET API | `Handler.do_GET()` |
| 增加/改 POST API | `Handler.do_POST()` |
| 改关闭服务 | `shutdown_service()` |
| 改网页启动 | `web_server()` |
| 阻止重复进程 | `acquire_single_instance()` |

## 数据是怎么流转的

1. CH9121 作为 TCP Client 连到 Python `TCP_PORT=1000`。
2. `tcp_server()` 的 `recv(1024)` 可能一次收半行、一行或多行，所以先累积到 `buffer`，再按 `\n` 拆完整行。
3. ACK 行交给 `parse_pump_ack()`，状态帧交给 `parse_frame()`。
4. 解析成功后在 `lock` 保护下更新 `latest`。
5. `GET /api/data` 把 `latest` 序列化为 JSON。
6. `POST /api/pump` 验证 duty/time，再调 `send_pump_command()` 对当前 `active_client` 执行 `sendall()`。

## 当前 HTTP API

| 方法 | 路径 | 作用 |
|---|---|---|
| GET | `/` | 返回 `dashboard.html` |
| GET | `/dashboard.html` | 返回页面 |
| GET | `/api/data` | 最新数据和连接状态 |
| POST | `/api/pump` | 下发 duty/duration |
| POST | `/api/shutdown` | 关闭 TCP、HTTP 和模块连接 |

## 增加一个 API 字段

假设 STM32 帧已增加 `L=12.3`：

```python
latest = {
    # ...
    "level": None,
    "level_status": "wait",
}
```

在 `parse_frame()` 中：

```python
parsed.update({
    "level": float(values["L"]),
    "level_status": "ok",
})
```

只要放入 `latest`，`/api/data` 会自动返回该字段。

## 修改水泵接口

`Handler.do_POST()` 当前限制：

- `duty` 和 `duration_ms` 必须是整数，布尔值不接受。
- duty 为 0~100。
- duty=0 时强制 duration=0。
- 非零 duty 时 duration 为 1~3600000 ms。
- CH9121 未连接时返回 HTTP 409。

如果扩展命令，要同时修改前端请求、Python 校验/格式化和 STM32 解析。

## 离线调试命令

```powershell
# 语法检查
python -m py_compile .\04_上位机与调试\tcp_web_bridge_192.168.1.142.py

# 启动
python .\04_上位机与调试\tcp_web_bridge_192.168.1.142.py

# 另开终端查 API
Invoke-RestMethod http://127.0.0.1:8000/api/data

# 只读一次 CLI
python .\04_上位机与调试\cli_monitor.py --once
```

## 常见坑

- 不能让两个 Python 桥接进程同时占用 1000/8000。页面右上角“关闭服务”会完整释放它们。
- 不要把每次 `recv()` 当成完整一帧，TCP 没有消息边界。
- 不要让网页直接连 CH9121；Python 是 TCP/协议与 HTTP 之间的桥。
- 修改 `latest` 键名后，必须全局搜索前端和 CLI 中的同名键。
