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
}
lock = threading.Lock()


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
            return {
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


def tcp_server():
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind((TCP_HOST, TCP_PORT))
    server.listen(1)

    print(f"[TCP] Listening on {TCP_HOST}:{TCP_PORT}")

    while True:
        conn, addr = server.accept()
        print(f"[TCP] Connected: {addr}")

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
            conn.close()
            with lock:
                latest["connected"] = False
                latest["client_ip"] = ""
            print("[TCP] Disconnected")


class Handler(BaseHTTPRequestHandler):
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
