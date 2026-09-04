#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ESP32-S3-BOX-3B 串口诊断脚本
============================
排查"烧录后串口无输出 / 输入无反应"问题

用法:
  python check_serial.py                # 自动选第一个 ESP32 设备
  python check_serial.py COM5           # 指定端口
  python check_serial.py COM5 9600      # 指定端口+波特率(默认 115200)

排查流程:
  1. 列出所有可用 COM 端口
  2. (可选) 通过 DTR 拉低尝试复位 ESP32
  3. 被动监听 N 秒,捕获启动输出(ROM 消息/setup() 打印)
  4. 主动发送 'h' / '1' / '\n' 等,看是否有回显或屏幕变化
  5. 输出诊断报告(看到了什么 / 没看到什么 / 下一步建议)
"""

import sys
import time
import threading
import argparse

# ===== 依赖检查 =====
def ensure_pyserial():
    try:
        import serial  # noqa
        return serial
    except ImportError:
        print("[依赖] pyserial 未安装,正在自动安装...")
        import subprocess
        r = subprocess.run(
            [sys.executable, "-m", "pip", "install", "pyserial", "--quiet"],
            capture_output=True, text=True
        )
        if r.returncode != 0:
            print(f"[错误] pip install 失败:\n{r.stderr}")
            sys.exit(1)
        import serial  # noqa
        return serial

def list_ports(serial):
    """列出所有可用 COM 端口(带描述)"""
    from serial.tools import list_ports
    ports = list(list_ports.comports())
    if not ports:
        print("[警告] 系统未发现任何 COM 端口")
        print("       检查: 1) USB 数据线 2) 设备管理器 3) 驱动是否装好")
        return []
    print(f"\n[端口] 发现 {len(ports)} 个串口设备:")
    for p in ports:
        desc = p.description or "(无描述)"
        hwid = p.hwid or ""
        print(f"  - {p.device:8s}  {desc:40s}  {hwid}")
    return ports

def select_port(ports, arg):
    """选端口: 优先用参数,否则让用户选,否则用第一个"""
    if arg:
        # 验证指定端口存在
        for p in ports:
            if p.device.upper() == arg.upper():
                return p.device
        print(f"[警告] 指定端口 {arg} 不在列表里,继续尝试打开")
        return arg
    if not ports:
        return None
    if len(ports) == 1:
        only = ports[0].device
        print(f"[自动] 仅 1 个端口,使用 {only}")
        return only
    print("\n[选择] 多个端口,输入编号选择 (默认 0):")
    for i, p in enumerate(ports):
        print(f"  {i}. {p.device}  {p.description or ''}")
    try:
        idx = input("编号> ").strip()
        idx = int(idx) if idx else 0
        return ports[idx].device
    except (ValueError, IndexError):
        return ports[0].device

def try_reset_via_dtr(ser):
    """尝试用 DTR/RTS 拉低复位 ESP32(对带 auto-reset 电路的板子有效)"""
    print("\n[复位] 尝试用 DTR/RTS 拉低复位 ESP32...")
    try:
        ser.dtr = False
        ser.rts = True
        time.sleep(0.1)
        ser.rts = False
        ser.dtr = True
        time.sleep(0.1)
        ser.dtr = False
        time.sleep(0.5)
        print("[复位] 已发送复位信号(对带 auto-reset 电路的板子有效;原生 USB CDC 通常无效)")
    except Exception as e:
        print(f"[复位] 失败: {e}")

def read_for(ser, seconds, label):
    """读 N 秒,实时打印,返回所有收到的字节"""
    buf = bytearray()
    end = time.time() + seconds
    print(f"\n[监听] 监听 {seconds} 秒 ({label})...")
    while time.time() < end:
        try:
            n = ser.in_waiting
            if n:
                chunk = ser.read(n)
                buf.extend(chunk)
                # 实时打印(只显示可读字符)
                try:
                    txt = chunk.decode('utf-8', errors='replace')
                    sys.stdout.write(txt)
                    sys.stdout.flush()
                except Exception:
                    sys.stdout.write(repr(chunk))
                    sys.stdout.flush()
            else:
                time.sleep(0.05)
        except KeyboardInterrupt:
            break
    print(f"\n[监听] 收到 {len(buf)} 字节")
    return bytes(buf)

def send_and_listen(ser, payload, listen_seconds, label):
    """发送 N 秒,看回显"""
    print(f"\n[发送] {label}: {payload!r}")
    try:
        ser.write(payload)
        ser.flush()
    except Exception as e:
        print(f"[发送] 失败: {e}")
        return b''
    return read_for(ser, listen_seconds, f"等回显 {listen_seconds} 秒")

def main():
    parser = argparse.ArgumentParser(description="ESP32-S3 串口诊断")
    parser.add_argument("port", nargs="?", help="COM 端口(如 COM5)")
    parser.add_argument("baud", nargs="?", type=int, default=115200, help="波特率(默认 115200)")
    parser.add_argument("--reset", action="store_true", help="打开后尝试 DTR 复位")
    parser.add_argument("--read-seconds", type=int, default=4, help="被动监听秒数(默认 4)")
    parser.add_argument("--send-seconds", type=int, default=2, help="发送后等回显秒数(默认 2)")
    args = parser.parse_args()

    serial = ensure_pyserial()
    ports = list_ports(serial)
    port = select_port(ports, args.port)
    if not port:
        print("[错误] 没有可用端口,退出")
        sys.exit(2)

    print(f"\n[打开] {port} @ {args.baud} 8N1")
    try:
        ser = serial.Serial(
            port=port,
            baudrate=args.baud,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=0.1,
            rtscts=False, xonxoff=False, dsrdtr=False,
        )
    except Exception as e:
        print(f"[错误] 打开失败: {e}")
        print("       常见原因: 1) Arduino IDE Serial Monitor 还开着(独占端口)")
        print("                 2) 端口号错 3) 驱动未装")
        sys.exit(3)

    print(f"[打开] 成功")
    if args.reset:
        try_reset_via_dtr(ser)

    # 清空残留缓冲
    ser.reset_input_buffer()
    ser.reset_output_buffer()

    # ===== 阶段 1: 被动监听启动输出 =====
    print("\n" + "=" * 60)
    print("阶段 1: 被动监听(捕获 ESP32 启动输出)")
    print("=" * 60)
    print(">>> 如果长时间没输出,在板子上按一下 RESET 按钮 <<<")
    boot_data = read_for(ser, args.read_seconds, "启动输出")

    # ===== 阶段 2: 发送测试字符 =====
    print("\n" + "=" * 60)
    print("阶段 2: 主动发送测试字符")
    print("=" * 60)
    echo_data = send_and_listen(ser, b"h\n", args.send_seconds, "发送 h + 换行 (求助)")

    # ===== 阶段 3: 再发数字 1-4 (心情切换) =====
    print("\n" + "=" * 60)
    print("阶段 3: 发送 1/2/3/4 切心情 (4=SLEEPY)")
    print("=" * 60)
    for c in [b"1", b"2", b"3", b"4"]:
        d = send_and_listen(ser, c, 1, f"发送 {c!r}")
        # 拼接统计

    # ===== 诊断报告 =====
    print("\n" + "=" * 60)
    print("诊断报告")
    print("=" * 60)

    all_data = boot_data + echo_data
    has_rom = b"ESP-ROM" in all_data
    has_boot = b"BOOT" in all_data or b"[1/4]" in all_data
    has_menu = b"\xe5\x91\xbd\xe4\xbb\xa4" in all_data or b"\xe5\xbf\x83\xe6\x83\x85" in all_data  # 命令/心情 utf8
    has_mood_echo = b"[?]" in all_data or b"\xe5\x91\xbd\xe4\xbb\xa4" in all_data

    print(f"  端口              : {port}")
    print(f"  波特率            : {args.baud}")
    print(f"  收到总字节数      : {len(all_data)}")
    print(f"  含 'ESP-ROM'      : {has_rom}     (有 = USB 链路通,ROM 启动消息到了)")
    print(f"  含 '[1/4]' 等     : {has_boot}     (有 = setup() 跑通)")
    print(f"  含中文'命令'      : {has_menu}     (有 = 菜单打印了)")

    print("\n[结论]")
    if not all_data:
        print("  ❌ 完全收不到任何字节")
        print("     1. 检查 USB 数据线(换一根)")
        print("     2. 设备管理器 → 看 COM 端口是否还在")
        print("     3. 确认 Arduino IDE Serial Monitor 已关闭(会独占端口)")
        print("     4. 板子按一下 RESET 按钮")
    elif has_rom and not has_boot:
        print("  ⚠️  看到 ESP-ROM 但没看到 setup() 输出")
        print("     说明: 1) USB 链路通 2) 应用代码没跑或卡死")
        print("     排查: Arduino IDE → Tools → USB CDC On Boot → 必须 Enabled")
        print("           Tools → Board → 必须 ESP32S3 Dev Module")
        print("     重新烧录: 按住 BOOT → 按 RESET → 松 BOOT → Upload")
    elif has_boot and not has_mood_echo:
        print("  ⚠️  setup() 跑通但串口命令无回显")
        print("     说明: 启动 OK 但 loop() 没处理输入")
        print("     排查: 检查 Serial.read() 是否被 buffer 缓冲")
    elif has_mood_echo:
        print("  ✅ 串口双向通信正常!")
        print("     如果屏幕仍无变化,问题在 display.render 或 LovyanGFX 初始化")
    else:
        print("  ⚠️  收到了一些非预期数据,可能是:")
        print("     - 波特率不对(看到的是乱码)")
        print("     - 其他设备的输出")

    ser.close()
    print("\n[完成] 串口已关闭")

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n[中断] 用户按 Ctrl+C,退出")
        sys.exit(0)
