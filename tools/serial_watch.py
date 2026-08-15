#!/usr/bin/env python3
"""擷取板子的序列埠輸出，用來抓當機 backtrace。

    .venv/bin/python tools/serial_watch.py            # 重置板子並錄 20 秒
    .venv/bin/python tools/serial_watch.py -s 60      # 錄 60 秒（操作時用）
    .venv/bin/python tools/serial_watch.py --no-reset # 不重置，接續觀察

為什麼不直接用 `pio device monitor`：它是互動式的，在自動化流程裡不會自己
結束；macOS 也沒有 `timeout` 指令可以包它。這個腳本錄固定秒數就返回，
輸出可以直接 grep。

**畫面異常時第一件事就是跑這個。** 有一次症狀是「按按鈕畫面出現紅綠藍彩條」，
看起來像顯示問題，實際上是板子當機重開、彩條是 LCD 驅動的開機測試圖樣。
序列埠一跑就看到穩定重現的 LoadProhibited，backtrace 直接指到肇因。
猜顯示層浪費了好幾輪，抓 log 一次就中。

抓到 Guru Meditation 後解 backtrace：

    ADDR2LINE=$(find ~/.platformio/packages/toolchain-xtensa-esp-elf -name "*-addr2line" | head -1)
    $ADDR2LINE -pfiaC -e .pio/build/waveshare_s3_lcd185b/firmware.elf <位址們>
"""

import argparse
import sys
import time

try:
    import serial
except ImportError:
    sys.exit("需要 pyserial：.venv/bin/pip install pyserial\n"
             "（安裝 esptool 時會一併帶進來）")

DEFAULT_PORT = "/dev/cu.usbmodem21101"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-p", "--port", default=DEFAULT_PORT,
                    help=f"序列埠（預設 {DEFAULT_PORT}，與 platformio.ini 一致）")
    ap.add_argument("-s", "--seconds", type=float, default=20.0)
    ap.add_argument("--no-reset", action="store_true",
                    help="不重置板子，接續觀察目前狀態")
    args = ap.parse_args()

    try:
        s = serial.Serial(args.port, 115200, timeout=0.2)
    except Exception as e:  # noqa: BLE001
        sys.exit(f"開啟 {args.port} 失敗：{e}\n"
                 f"確認板子已接上，且沒有其他 monitor 佔用序列埠。")

    if not args.no_reset:
        # ESP32-S3 的 USB-JTAG：用 RTS 脈衝硬重置，才抓得到開機訊息。
        s.setDTR(False)
        s.setRTS(True)
        time.sleep(0.15)
        s.setRTS(False)

    end = time.time() + args.seconds
    while time.time() < end:
        data = s.read(8192)
        if data:
            sys.stdout.write(data.decode("utf-8", "replace"))
            sys.stdout.flush()
    s.close()


if __name__ == "__main__":
    main()
