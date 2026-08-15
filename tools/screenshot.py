#!/usr/bin/env python3
"""把板子上的畫面抓回來存成 PNG。

板子在遠端、沒有人能看螢幕時，這是唯一能確認版面實際長相的方法。
版面的 maxR 可以用算術驗收，但「顏色對不對」「元件有沒有互相遮住」不行。

用法：
    .venv/bin/python tools/screenshot.py -o shot.png

流程：送一個 's' 給板子 -> 板子重繪一次畫面、從 flush callback 擷取 ->
RLE + base64 從序列埠吐回來 -> 這裡解碼成 PNG。

只用標準函式庫，PNG 是手寫的（zlib + struct），不需要 PIL。
"""

import argparse
import base64
import struct
import sys
import time
import zlib

DEFAULT_PORT = "/dev/cu.usbmodem21101"
BAUD = 115200


def read_capture(ser, timeout_s):
    """讀到 [snap] end 為止，回傳 (寬, 高, base64 字串)。"""
    deadline = time.time() + timeout_s
    started = False
    w = h = 0
    chunks = []

    while time.time() < deadline:
        raw = ser.readline()
        if not raw:
            continue
        line = raw.decode("utf-8", "replace").strip()

        if line.startswith("[snap] begin"):
            # 形如：[snap] begin 360x360 rgb565 rle
            parts = line.split()
            w, h = (int(v) for v in parts[2].split("x"))
            started = True
            continue
        if line.startswith("[snap] end"):
            return w, h, "".join(chunks)
        if line.startswith("[snap]"):
            print(f"板子回報：{line}", file=sys.stderr)
            continue
        if started:
            chunks.append(line)

    raise TimeoutError("等不到 [snap] end，板子可能沒收到觸發或還在傳")


def decode_rle565(payload, w, h):
    """(count, value) 對還原成 RGB888 的位元組列。"""
    data = base64.b64decode(payload)
    px = bytearray()
    total = w * h
    got = 0

    for i in range(0, len(data) - 3, 4):
        n = data[i] | (data[i + 1] << 8)
        v = data[i + 2] | (data[i + 3] << 8)
        # RGB565 -> RGB888。低位補上去，不然白色會變成 248,252,248。
        r = ((v >> 11) & 0x1F)
        g = ((v >> 5) & 0x3F)
        b = v & 0x1F
        rgb = bytes(((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)))
        if got + n > total:
            n = total - got
        px += rgb * n
        got += n
        if got >= total:
            break

    if got < total:
        print(f"警告：只收到 {got}/{total} 個像素，其餘補黑", file=sys.stderr)
        px += b"\x00\x00\x00" * (total - got)
    return bytes(px)


def write_png(path, w, h, rgb):
    """最小的 PNG 編碼器：每列前面加一個 filter type 0 的位元組。"""
    rows = b"".join(b"\x00" + rgb[y * w * 3:(y + 1) * w * 3] for y in range(h))

    def chunk(tag, body):
        c = tag + body
        return struct.pack(">I", len(body)) + c + struct.pack(">I", zlib.crc32(c))

    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(rows, 9))
           + chunk(b"IEND", b""))
    with open(path, "wb") as f:
        f.write(png)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-p", "--port", default=DEFAULT_PORT)
    ap.add_argument("-o", "--out", default="screenshot.png")
    ap.add_argument("-t", "--timeout", type=float, default=40.0)
    ap.add_argument("--settle", type=float, default=0.5,
                    help="送出觸發前先等多久，讓開埠造成的重置穩定下來")
    ap.add_argument("--screen", default=None,
                    help="先切到指定畫面再截圖。1 首頁 2 準備 3 計分 4 局結果 "
                         "5 完成 6 設定 7 賽制 8 歷史 9 勝利類型 overlay")
    args = ap.parse_args()

    try:
        import serial
    except ImportError:
        sys.exit("需要 pyserial：.venv/bin/pip install pyserial")

    with serial.Serial(args.port, BAUD, timeout=1) as ser:
        time.sleep(args.settle)
        if args.screen:
            ser.write(args.screen.encode())
            ser.flush()
            time.sleep(1.0)  # 等轉場動畫跑完，否則抓到的是動到一半的畫面
        ser.reset_input_buffer()
        ser.write(b"s")
        ser.flush()

        w, h, payload = read_capture(ser, args.timeout)

    rgb = decode_rle565(payload, w, h)
    write_png(args.out, w, h, rgb)
    print(f"已存出 {args.out}（{w}x{h}）")


if __name__ == "__main__":
    main()
