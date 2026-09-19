import argparse
import json
import os
import sys
import time
import urllib.error
import urllib.request


DEFAULT_URL = "http://127.0.0.1:8000/api/data"


STATUS_TEXT = {
    "ok": "正常",
    "wait": "等待",
    "busy": "转换中",
    "no_device": "未连接",
    "crc_error": "CRC错误",
    "invalid_parameter": "参数错误",
    "adc_error": "ADC错误",
    "sensor_error": "传感器错误",
    "timer_error": "定时器错误",
    "unavailable": "无数据",
    "error": "错误",
}


def fetch_data(url: str) -> dict:
    request = urllib.request.Request(
        url,
        headers={"Cache-Control": "no-cache"},
    )
    with urllib.request.urlopen(request, timeout=2.0) as response:
        return json.loads(response.read().decode("utf-8"))


def format_value(value, status: str, decimals: int) -> str:
    if status != "ok" or value is None:
        return "--"
    return f"{float(value):.{decimals}f}"


def render(data: dict) -> str:
    t1_status = data.get("temperature1_status", "wait")
    t2_status = data.get("temperature2_status", "wait")
    flow_status = data.get("flow_status", "wait")
    pressure_status = data.get("pressure_status", "wait")

    connected = bool(data.get("connected"))
    has_data = bool(data.get("has_data"))
    connection = "在线" if connected and has_data else (
        "已连接，等待数据" if connected else "离线"
    )

    t1 = format_value(data.get("temperature1"), t1_status, 1)
    t2 = format_value(data.get("temperature2"), t2_status, 1)
    flow = format_value(data.get("flow"), flow_status, 2)

    pressure_pa = data.get("pressure_pa")
    pressure = (
        str(int(round(float(pressure_pa))))
        if pressure_status == "ok" and pressure_pa is not None
        else "--"
    )

    return "\n".join(
        [
            "+--------------------------------------+",
            "|          IOTCM CLI Monitor           |",
            "+--------------------------------------+",
            f"| 连接状态 : {connection:<24}|",
            f"| 温度 T1  : {t1:>8} °C  {STATUS_TEXT.get(t1_status, t1_status):<10}|",
            f"| 温度 T2  : {t2:>8} °C  {STATUS_TEXT.get(t2_status, t2_status):<10}|",
            f"| 流量     : {flow:>8} L/min {STATUS_TEXT.get(flow_status, flow_status):<8}|",
            f"| 压力     : {pressure:>8} Pa  {STATUS_TEXT.get(pressure_status, pressure_status):<10}|",
            "+--------------------------------------+",
            f" 最后更新: {data.get('updated_at') or '--'}",
            " Ctrl+C 退出",
        ]
    )


def clear_screen() -> None:
    if os.name == "nt":
        os.system("cls")
    else:
        sys.stdout.write("\033[2J\033[H")
        sys.stdout.flush()


def main() -> int:
    parser = argparse.ArgumentParser(
        description="显示 IOTCM 的 T1、T2、流量和压力。"
    )
    parser.add_argument("--url", default=DEFAULT_URL)
    parser.add_argument("--interval", type=float, default=0.25)
    parser.add_argument(
        "--once",
        action="store_true",
        help="读取一次后退出，便于脚本调用和测试。",
    )
    args = parser.parse_args()

    try:
        while True:
            try:
                output = render(fetch_data(args.url))
            except (urllib.error.URLError, TimeoutError, json.JSONDecodeError) as error:
                output = (
                    "IOTCM CLI Monitor\n\n"
                    "本地数据服务不可用。\n"
                    f"地址: {args.url}\n"
                    f"错误: {error}\n"
                )

            if not args.once:
                clear_screen()
            print(output, flush=True)

            if args.once:
                return 0
            time.sleep(max(args.interval, 0.1))
    except KeyboardInterrupt:
        print("\nCLI 监测已停止。")
        return 0


if __name__ == "__main__":
    raise SystemExit(main())
