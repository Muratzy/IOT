import json
import os
import socket
import threading
import time
from datetime import datetime
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

TCP_HOST = "0.0.0.0"
TCP_PORT = 1000

WEB_HOST = "0.0.0.0"
WEB_PORT = 8000

BASE_DIR = Path(__file__).resolve().parent
HTML_FILE = BASE_DIR / "dashboard.html"

latest = {
    "temperature1": None,
    "temperature1_status": "wait",
    "temperature2": None,
    "temperature2_status": "wait",
    "flow": None,
    "flow_status": "wait",
    "pressure_pa": None,
    "pressure_mpa": None,
    "pressure_status": "wait",
    "pressure_adc": None,
    "pressure_delta": None,
    "raw": "",
    "updated_at": "",
    "updated_ms": 0,
    "connected": False,
    "has_data": False,
    "client_ip": "",
    "received_frames": 0,
    "frame_version": 0,
    "pump_duty": 0,
    "pump_status": "stopped",
    "pump_remaining_ms": 0,
    "pump_command_duty": 0,
    "pump_command_duration_ms": 0,
    "pump_command_status": "idle",
    "pump_command_at": "",
}
lock = threading.Lock()
client_lock = threading.Lock()
service_lock = threading.Lock()
shutdown_event = threading.Event()
tcp_ready = threading.Event()
active_client = None
tcp_listener = None
web_httpd = None
tcp_start_error = None
instance_mutex = None


def acquire_single_instance():
    """Prevent two Windows bridge processes from sharing ports 1000/8000."""
    global instance_mutex

    if os.name != "nt":
        return

    import ctypes
    from ctypes import wintypes

    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    create_mutex = kernel32.CreateMutexW
    create_mutex.argtypes = [wintypes.LPVOID, wintypes.BOOL, wintypes.LPCWSTR]
    create_mutex.restype = wintypes.HANDLE
    close_handle = kernel32.CloseHandle
    close_handle.argtypes = [wintypes.HANDLE]
    close_handle.restype = wintypes.BOOL

    mutex = create_mutex(None, False, "Local\\IOTCM_CH9121_WebBridge")
    if not mutex:
        raise OSError(ctypes.get_last_error(), "无法创建桥接服务单实例锁")
    if ctypes.get_last_error() == 183:  # ERROR_ALREADY_EXISTS
        close_handle(mutex)
        raise SystemExit(
            "[ERROR] 已有网页桥接服务正在运行。请在网页右上角点击“关闭服务”后再启动。"
        )

    instance_mutex = (kernel32, mutex)


def temperature_status(code: int) -> str:
    return {
        0: "ok",
        1: "busy",
        2: "no_device",
        3: "crc_error",
        4: "invalid_parameter",
    }.get(code, "error")


def pressure_status(code: int) -> str:
    return {
        0: "wait",
        1: "ok",
        2: "adc_error",
        3: "sensor_error",
    }.get(code, "error")


def flow_status(code: int) -> str:
    return {
        0: "wait",
        1: "ok",
        2: "timer_error",
    }.get(code, "error")


def pump_status(code: int) -> str:
    return {
        0: "stopped",
        1: "running",
        2: "done",
    }.get(code, "error")


def parse_frame(line: str):
    """
    Parse the OLED-aligned frame from the current firmware:
    T1=+21.2,S1=0,T2=+21.4,S2=0,F=00.35,SF=1,
    P=0000399,SP=1,A=0413,D=+0001

    The former TEMP/PRESS/FLOW frame remains accepted during firmware update.
    """
    values = {}
    for part in line.split(","):
        if "=" not in part:
            continue
        key, value = part.split("=", 1)
        values[key.strip().upper()] = value.strip()

    current_fields = {"T1", "S1", "T2", "S2", "F", "SF",
                      "P", "SP", "A", "D"}
    if current_fields.issubset(values):
        try:
            pressure_pa = int(values["P"])
            parsed = {
                "temperature1": float(values["T1"]),
                "temperature1_status": temperature_status(int(values["S1"])),
                "temperature2": float(values["T2"]),
                "temperature2_status": temperature_status(int(values["S2"])),
                "flow": float(values["F"]),
                "flow_status": flow_status(int(values["SF"])),
                "pressure_pa": pressure_pa,
                "pressure_mpa": pressure_pa / 1_000_000.0,
                "pressure_status": pressure_status(int(values["SP"])),
                "pressure_adc": int(values["A"]),
                "pressure_delta": int(values["D"]),
                "frame_version": 2,
            }
            pump_fields = {"M", "MS", "MT"}
            if pump_fields.issubset(values):
                parsed.update({
                    "pump_duty": int(values["M"]),
                    "pump_status": pump_status(int(values["MS"])),
                    "pump_remaining_ms": int(values["MT"]),
                    "frame_version": 3,
                })
            return parsed
        except ValueError:
            return None

    legacy_fields = {"TEMP", "PRESS", "FLOW"}
    if legacy_fields.issubset(values):
        try:
            pressure_mpa = float(values["PRESS"])
            return {
                "temperature1": float(values["TEMP"]),
                "temperature1_status": "ok",
                "temperature2": None,
                "temperature2_status": "unavailable",
                "flow": float(values["FLOW"]),
                "flow_status": "ok",
                "pressure_pa": round(pressure_mpa * 1_000_000),
                "pressure_mpa": pressure_mpa,
                "pressure_status": "ok",
                "pressure_adc": None,
                "pressure_delta": None,
                "frame_version": 1,
            }
        except ValueError:
            return None

    return None


def parse_pump_ack(line: str):
    values = {}
    for part in line.split(","):
        if "=" not in part:
            continue
        key, value = part.split("=", 1)
        values[key.strip().upper()] = value.strip()

    if not {"PUMP_ACK", "TIME", "OK"}.issubset(values):
        return None

    try:
        return {
            "duty": int(values["PUMP_ACK"]),
            "duration_ms": int(values["TIME"]),
            "accepted": int(values["OK"]) == 1,
        }
    except ValueError:
        return None


def send_pump_command(duty: int, duration_ms: int):
    global active_client

    command = f"PUMP={duty},TIME={duration_ms}\r\n".encode("ascii")
    with client_lock:
        if active_client is None:
            return False, "CH9121 未连接"
        try:
            active_client.sendall(command)
        except OSError as error:
            return False, f"发送失败：{error}"

    with lock:
        latest["pump_command_duty"] = duty
        latest["pump_command_duration_ms"] = duration_ms
        latest["pump_command_status"] = "sent"
        latest["pump_command_at"] = datetime.now().strftime("%H:%M:%S")

    print("[TX]", command.decode("ascii").strip())
    return True, "命令已发送，等待 STM32 确认"


def shutdown_service():
    """Close the module connection and both listeners, then end the process."""
    global active_client

    if shutdown_event.is_set():
        return

    shutdown_event.set()
    print("[SERVICE] 正在关闭网页桥接服务")

    with client_lock:
        conn = active_client
        active_client = None

    if conn is not None:
        try:
            conn.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass
        conn.close()

    with service_lock:
        listener = tcp_listener
        httpd = web_httpd

    if listener is not None:
        listener.close()
    if httpd is not None:
        httpd.shutdown()


def delayed_shutdown():
    time.sleep(0.15)
    shutdown_service()


def tcp_server():
    global active_client, tcp_listener, tcp_start_error

    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    if os.name == "nt" and hasattr(socket, "SO_EXCLUSIVEADDRUSE"):
        server.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
    else:
        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

    try:
        server.bind((TCP_HOST, TCP_PORT))
        server.listen(1)
    except OSError as error:
        tcp_start_error = error
        server.close()
        tcp_ready.set()
        return

    with service_lock:
        tcp_listener = server
    tcp_ready.set()

    print(f"[TCP] Listening on {TCP_HOST}:{TCP_PORT}")

    while not shutdown_event.is_set():
        try:
            conn, addr = server.accept()
        except OSError:
            if shutdown_event.is_set():
                break
            raise

        if shutdown_event.is_set():
            conn.close()
            break

        conn.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        print(f"[TCP] Connected: {addr}")

        with client_lock:
            if active_client is not None:
                active_client.close()
            active_client = conn

        with lock:
            latest["connected"] = True
            latest["client_ip"] = addr[0]

        buffer = ""

        try:
            while not shutdown_event.is_set():
                data = conn.recv(1024)
                if not data:
                    break

                buffer += data.decode("utf-8", errors="ignore")

                while "\n" in buffer:
                    line, buffer = buffer.split("\n", 1)
                    line = line.rstrip("\r").strip()

                    if not line:
                        continue

                    print("[RX]", line)

                    pump_ack = parse_pump_ack(line)
                    if pump_ack is not None:
                        with lock:
                            latest["pump_command_duty"] = pump_ack["duty"]
                            latest["pump_command_duration_ms"] = pump_ack["duration_ms"]
                            latest["pump_command_status"] = (
                                "accepted" if pump_ack["accepted"] else "rejected"
                            )
                            latest["pump_command_at"] = datetime.now().strftime("%H:%M:%S")
                        continue

                    parsed = parse_frame(line)
                    if parsed is None:
                        print("[WARN] 无法解析这一帧")
                        continue

                    with lock:
                        latest.update(parsed)
                        latest["raw"] = line
                        latest["updated_at"] = datetime.now().strftime("%H:%M:%S")
                        latest["updated_ms"] = int(time.time() * 1000)
                        latest["has_data"] = True
                        latest["received_frames"] += 1

        except OSError as error:
            if not shutdown_event.is_set():
                print(f"[TCP] Connection error: {error}")
        finally:
            with client_lock:
                if active_client is conn:
                    active_client = None
                conn.close()
            with lock:
                latest["connected"] = False
                latest["client_ip"] = ""
                latest["pump_command_status"] = "disconnected"
            print("[TCP] Disconnected")

    with service_lock:
        if tcp_listener is server:
            tcp_listener = None
    server.close()
    print("[TCP] Stopped")


class BridgeHTTPServer(ThreadingHTTPServer):
    allow_reuse_address = os.name != "nt"

    def server_bind(self):
        if os.name == "nt" and hasattr(socket, "SO_EXCLUSIVEADDRUSE"):
            self.socket.setsockopt(
                socket.SOL_SOCKET,
                socket.SO_EXCLUSIVEADDRUSE,
                1,
            )
        super().server_bind()


class Handler(BaseHTTPRequestHandler):
    def send_json(self, status, data):
        payload = json.dumps(data, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)

    def do_GET(self):
        if self.path == "/" or self.path == "/dashboard.html":
            try:
                content = HTML_FILE.read_bytes()
                self.send_response(200)
                self.send_header("Content-Type", "text/html; charset=utf-8")
                self.send_header("Content-Length", str(len(content)))
                self.end_headers()
                self.wfile.write(content)
            except FileNotFoundError:
                self.send_error(404, "dashboard.html not found")
            return

        if self.path.startswith("/api/data"):
            with lock:
                payload = json.dumps(latest, ensure_ascii=False).encode("utf-8")

            self.send_response(200)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Cache-Control", "no-store")
            self.send_header("Content-Length", str(len(payload)))
            self.end_headers()
            self.wfile.write(payload)
            return

        self.send_error(404)

    def do_POST(self):
        if self.path == "/api/shutdown":
            if self.headers.get("X-IOTCM-Action") != "shutdown":
                self.send_json(403, {
                    "ok": False,
                    "message": "关闭请求缺少本地页面标识",
                })
                return
            self.send_json(200, {
                "ok": True,
                "message": "桥接服务正在关闭",
            })
            threading.Thread(target=delayed_shutdown, daemon=True).start()
            return

        if self.path != "/api/pump":
            self.send_error(404)
            return

        try:
            content_length = int(self.headers.get("Content-Length", "0"))
        except ValueError:
            self.send_json(400, {"ok": False, "message": "Content-Length 无效"})
            return

        if content_length <= 0 or content_length > 4096:
            self.send_json(400, {"ok": False, "message": "请求正文长度无效"})
            return

        try:
            body = json.loads(self.rfile.read(content_length).decode("utf-8"))
            duty_value = body["duty"]
            duration_value = body["duration_ms"]
            if isinstance(duty_value, bool) or isinstance(duration_value, bool):
                raise ValueError
            duty = int(duty_value)
            duration_ms = int(duration_value)
            if duty != duty_value or duration_ms != duration_value:
                raise ValueError
        except (KeyError, TypeError, ValueError, json.JSONDecodeError, UnicodeDecodeError):
            self.send_json(400, {
                "ok": False,
                "message": "需要整数 duty 和 duration_ms",
            })
            return

        if duty < 0 or duty > 100:
            self.send_json(400, {"ok": False, "message": "PWM 必须为 0~100"})
            return
        if duty == 0:
            duration_ms = 0
        elif duration_ms <= 0 or duration_ms > 3600000:
            self.send_json(400, {
                "ok": False,
                "message": "运行时间必须为 1~3600000 ms",
            })
            return

        sent, message = send_pump_command(duty, duration_ms)
        self.send_json(200 if sent else 409, {
            "ok": sent,
            "message": message,
            "duty": duty,
            "duration_ms": duration_ms,
        })

    def log_message(self, format, *args):
        return


def web_server():
    global web_httpd

    httpd = BridgeHTTPServer((WEB_HOST, WEB_PORT), Handler)
    with service_lock:
        web_httpd = httpd

    print(f"[WEB] Local: http://127.0.0.1:{WEB_PORT}")
    print(f"[WEB] LAN:   http://192.168.1.142:{WEB_PORT}")
    try:
        httpd.serve_forever()
    finally:
        httpd.server_close()
        with service_lock:
            if web_httpd is httpd:
                web_httpd = None
        print("[WEB] Stopped")


if __name__ == "__main__":
    acquire_single_instance()
    threading.Thread(target=tcp_server, daemon=True).start()
    if not tcp_ready.wait(3.0):
        raise SystemExit("[ERROR] TCP 服务启动超时")
    if tcp_start_error is not None:
        raise SystemExit(f"[ERROR] 无法监听 TCP {TCP_PORT}：{tcp_start_error}")

    try:
        web_server()
    except KeyboardInterrupt:
        print("\n[SERVICE] 收到终止请求")
    finally:
        shutdown_service()
