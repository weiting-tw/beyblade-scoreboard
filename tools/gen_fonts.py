#!/usr/bin/env python3
"""從原始碼的字串常值產生繁體中文子集字型。

用法：
    python3 tools/gen_fonts.py            # 產生字型
    python3 tools/gen_fonts.py --list     # 只列出會被收錄的漢字

為什麼要自動掃描：Noto Sans CJK 全字集約 1–2MB，而這個專案實際只用到不到
一百個漢字。手動維護字元清單一定會漏 —— 新增一句中文卻忘了更新清單，該字
就會在螢幕上變成空白方塊，而且編譯不會報錯。

只掃字串常值、**不掃註解**：本專案的註解全是中文，一併收進去會讓子集膨脹
數倍。下方的 tokenizer 用單一 regex 同時匹配「字串」與「註解」，只保留前者，
這樣字串裡的 `//` 也不會被誤判成註解起點。
"""

import argparse
import bisect
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# 掃描範圍：UI 與計分核心（規則名稱如「P1 爆裂勝」會顯示在局結果頁）。
SOURCES = [
    "src/ui",
    "src/app",
    "src/voice",
    "lib/match/src",
    "lib/match/include",
]

# 產生出來的字型檔本身也在 src/ui/fonts，必須排除，否則會掃到字型資料。
EXCLUDE_DIRS = {"fonts"}

FONT_DIR = os.path.join(ROOT, "src", "ui", "fonts")
NOTO_DIR = os.path.expanduser("~/Library/Fonts")

# (輸出名, 字級, 字重檔名)
# 16 用 Medium：16px 的粗體漢字在 4bpp 下筆畫會糊成一團。
# 22/30 用 Bold：按鈕與標題需要在計分板上一眼可讀。
VARIANTS = [
    ("font_tc_16", 16, "NotoSansCJKtc-Medium.otf"),
    ("font_tc_22", 22, "NotoSansCJKtc-Bold.otf"),
    ("font_tc_30", 30, "NotoSansCJKtc-Bold.otf"),
]

# 字串 或 行註解 或 區塊註解。順序重要：字串放最前面，
# 這樣 "http://..." 裡的 // 不會被當成註解。
TOKEN_RE = re.compile(r'"(?:[^"\\\n]|\\.)*"' r"|//[^\n]*" r"|/\*.*?\*/", re.S)

# 中日韓統一漢字 + 擴充A + 中文標點 + 全形符號
CJK_RE = re.compile(
    r"[　-〿㐀-䶿一-鿿豈-﫿︰-﹏＀-￯]"
)


def iter_source_files():
    for rel in SOURCES:
        base = os.path.join(ROOT, rel)
        if not os.path.isdir(base):
            continue
        for dirpath, dirnames, filenames in os.walk(base):
            dirnames[:] = [d for d in dirnames if d not in EXCLUDE_DIRS]
            for name in filenames:
                if name.endswith((".c", ".cpp", ".h", ".hpp")):
                    yield os.path.join(dirpath, name)


def collect_symbols():
    """回傳 (排序後的漢字字串, 出處對照表)。"""
    chars = {}
    for path in iter_source_files():
        with open(path, encoding="utf-8") as f:
            text = f.read()

        # 每個字元位移對應到第幾行，用來判斷該字串是不是在 Serial 呼叫上。
        line_starts = [0]
        for i, ch in enumerate(text):
            if ch == "\n":
                line_starts.append(i + 1)
        lines = text.splitlines()

        for m in TOKEN_RE.finditer(text):
            token = m.group(0)
            if not token.startswith('"'):
                continue  # 註解，跳過

            # 除錯訊息只會出現在序列埠，不需要字型。少收這些字可省下可觀的
            # flash（實測差 23 個字 / 約 100KB）。
            line_no = bisect.bisect_right(line_starts, m.start()) - 1
            if 0 <= line_no < len(lines) and "Serial." in lines[line_no]:
                continue

            for ch in CJK_RE.findall(token):
                chars.setdefault(ch, set()).add(os.path.relpath(path, ROOT))
    return "".join(sorted(chars)), chars


def build(symbols, dry_run=False):
    conv = os.path.join(ROOT, "tools", "node_modules", ".bin", "lv_font_conv")
    if not os.path.exists(conv):
        sys.exit("找不到 lv_font_conv，請先執行： cd tools && npm install lv_font_conv")

    os.makedirs(FONT_DIR, exist_ok=True)
    total = 0
    for name, size, fontfile in VARIANTS:
        src = os.path.join(NOTO_DIR, fontfile)
        if not os.path.exists(src):
            sys.exit(f"找不到字型檔：{src}\n請安裝 Noto Sans CJK TC（OFL 授權）。")

        out = os.path.join(FONT_DIR, f"{name}.c")
        cmd = [
            conv,
            "--font", src,
            "--size", str(size),
            "--bpp", "4",
            "--format", "lvgl",
            "--lv-include", "lvgl.h",
            # lv_conf.h 的 LV_USE_FONT_COMPRESSED = 0，壓縮字型會無法解碼。
            # 未壓縮也讓 CJK 的繪製成本低一些，flash 目前很寬裕。
            "--no-compress",
            "-o", out,
            "-r", "0x20-0x7F",     # ASCII：數字、英文標籤、標點
            "--symbols", symbols,  # 專案實際用到的漢字
        ]
        if dry_run:
            print(" ".join(cmd[:1] + ["..."]))
            continue
        subprocess.run(cmd, check=True)
        size_kb = os.path.getsize(out) / 1024
        total += size_kb
        print(f"  {name}.c  {size}px  {size_kb:7.1f} KB (C 原始碼)")
    if not dry_run:
        print(f"  合計 {total:.1f} KB 原始碼（編譯後的二進位會小得多）")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--list", action="store_true", help="只列出收錄的漢字與出處")
    args = ap.parse_args()

    symbols, sources = collect_symbols()
    print(f"從原始碼字串常值收集到 {len(symbols)} 個漢字／全形符號：")
    print(f"  {symbols}")

    if args.list:
        print()
        for ch in symbols:
            print(f"  {ch}  <- {', '.join(sorted(sources[ch]))}")
        return

    print()
    build(symbols)


if __name__ == "__main__":
    main()
