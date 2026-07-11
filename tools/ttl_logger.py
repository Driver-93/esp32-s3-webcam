"""
ESP32-CAM TTL 串口日志记录器
============================
持续读取 COM6 串口输出, 写入日志文件 + 实时打印。

用法:
    python ttl_logger.py              # 默认 COM6 115200
    python ttl_logger.py COM7         # 指定端口
    python ttl_logger.py COM6 9600    # 指定端口和波特率

日志文件:
    ttl_output_YYYYMMDD_HHMMSS.log   (当前目录)

其他程序可以实时读取最新的日志文件查看状态。
"""

import serial
import sys
import time
from datetime import datetime

def main():
    port = sys.argv[1] if len(sys.argv) > 1 else "COM6"
    baud = int(sys.argv[2]) if len(sys.argv) > 2 else 115200

    logfile = f"ttl_output_{datetime.now().strftime('%Y%m%d_%H%M%S')}.log"

    print(f"端口: {port} @ {baud}")
    print(f"日志: {logfile}")
    print("Ctrl+C 停止\n")

    # 先写一行头到日志
    with open(logfile, "w", encoding="utf-8") as f:
        f.write(f"=== TTL Logger started at {datetime.now().strftime('%Y-%m-%d %H:%M:%S')} ===\n")
        f.write(f"Port: {port} Baud: {baud}\n\n")

    ser = None
    try:
        ser = serial.Serial(port, baud, timeout=0.1)
        ser.setDTR(True)
        ser.setRTS(True)
        time.sleep(0.5)

        print(f"已连接 {port}，等待数据...\n")

        # 行缓冲
        partial = b""

        while True:
            try:
                data = ser.read(512)
                if data:
                    partial += data
                    # 按行分割
                    while b"\n" in partial:
                        line_bytes, partial = partial.split(b"\n", 1)
                        line = line_bytes.decode("utf-8", errors="replace").strip("\r")
                        if line:
                            timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
                            log_line = f"[{timestamp}] {line}"

                            # 打印到控制台
                            print(f"[{timestamp}] {line}")

                            # 写入日志 (实时 flush 以便其他程序读取)
                            with open(logfile, "a", encoding="utf-8") as f:
                                f.write(log_line + "\n")
                                f.flush()
                else:
                    time.sleep(0.01)
            except serial.SerialException:
                print("\n[!] 串口断开，重连中...")
                time.sleep(1)
                try:
                    if ser and ser.is_open:
                        ser.close()
                    ser = serial.Serial(port, baud, timeout=0.1)
                    print("[!] 已重连")
                except:
                    pass

    except KeyboardInterrupt:
        print(f"\n\n已停止，日志保存至: {logfile}")
    except serial.SerialException as e:
        print(f"\n[!] 无法打开 {port}: {e}")
        print(f"    请确认端口号是否正确，或使用: python {sys.argv[0]} COM端口号")
    finally:
        if ser and ser.is_open:
            ser.close()

if __name__ == "__main__":
    main()
