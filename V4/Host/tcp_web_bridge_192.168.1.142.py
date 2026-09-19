import json
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
active_client = None


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


def tcp_server():
    global active_client

    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind((TCP_HOST, TCP_PORT))
    server.listen(1)

    print(f"[TCP] Listening on {TCP_HOST}:{TCP_PORT}")

    while True:
        conn, addr = server.accept()
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
            while True:
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

        except ConnectionError:
            pass
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
    httpd = ThreadingHTTPServer((WEB_HOST, WEB_PORT), Handler)
    print(f"[WEB] Local: http://127.0.0.1:{WEB_PORT}")
    print(f"[WEB] LAN:   http://192.168.1.142:{WEB_PORT}")
    httpd.serve_forever()


if __name__ == "__main__":
    threading.Thread(target=tcp_server, daemon=True).start()
    web_server()
