#!/usr/bin/env python3
"""用 macOS 的 `say` 產生語音片段，轉成可直接嵌入韌體的 C 陣列。

    python3 tools/gen_voice_clips.py
    python3 tools/gen_voice_clips.py --voice Samantha     # 換人聲
    say -v '?'                                            # 列出所有可用人聲

為什麼不用板上的 TTS：esp-sr 的 TTS 元件（esp_tts_chinese）在 Arduino 的
預建函式庫裡**只有標頭檔、沒有 libesp_tts.a**，而且它是中文專用的，
講不出 "GO SHOOT"。在 Mac 上先合成好、把 PCM 燒進 flash，反而更可控：
發音、語氣、音量都在產生階段就決定，板子上只要把樣本推進 I2S。

格式固定 16kHz / 16bit / 單聲道 —— 那是 esp-sr 綁死的取樣率，
音訊匯流排整條都跟著它走。播放時再複製成雙聲道。
"""

import argparse
import os
import subprocess
import sys
import tempfile
import wave

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(ROOT, "src", "audio", "clips")

SAMPLE_RATE = 16000

# (C 識別字, 要唸的文字)
CLIPS = [
    ("clip_go", "Go Shoot!"),
]


def synth(text, voice, rate):
    """呼叫 say 產生 16kHz/16bit/mono 的 WAV，回傳 PCM bytes。"""
    fd, path = tempfile.mkstemp(suffix=".wav")
    os.close(fd)
    try:
        subprocess.run(
            ["say", "-v", voice, "-r", str(rate),
             "--data-format=LEI16@%d" % SAMPLE_RATE, "-o", path, text],
            check=True,
        )
        with wave.open(path) as w:
            assert w.getnchannels() == 1, "預期單聲道"
            assert w.getsampwidth() == 2, "預期 16bit"
            assert w.getframerate() == SAMPLE_RATE, "預期 16kHz"
            return w.readframes(w.getnframes())
    finally:
        os.unlink(path)


def normalize(pcm, peak_ratio=0.85):
    """把振幅拉到接近滿刻度。

    `say` 的輸出通常偏小聲，直接推進小喇叭會幾乎聽不到。這裡在產生階段
    就正規化，比在板子上乘倍數好 —— 板上乘會把量化雜訊一起放大。
    """
    import array

    a = array.array("h")
    a.frombytes(pcm)
    peak = max(abs(v) for v in a) if a else 0
    if peak == 0:
        return pcm
    gain = (32767 * peak_ratio) / peak
    for i, v in enumerate(a):
        s = int(v * gain)
        a[i] = 32767 if s > 32767 else (-32768 if s < -32768 else s)
    return a.tobytes()


def write_c(name, text, pcm, f):
    import array

    a = array.array("h")
    a.frombytes(pcm)
    f.write(f"\n// 「{text}」 {len(a)} samples, "
            f"{len(a) / SAMPLE_RATE:.2f}s, {len(pcm) / 1024:.1f} KB\n")
    f.write(f"const int16_t {name}[] = {{\n")
    for i in range(0, len(a), 12):
        f.write("    " + ", ".join(str(v) for v in a[i:i + 12]) + ",\n")
    f.write("};\n")
    f.write(f"const unsigned {name}_len = {len(a)};\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--voice", default="Daniel", help="say 的人聲名稱")
    ap.add_argument("--rate", type=int, default=170, help="語速（字/分）")
    args = ap.parse_args()

    os.makedirs(OUT_DIR, exist_ok=True)
    c_path = os.path.join(OUT_DIR, "voice_clips.c")
    h_path = os.path.join(OUT_DIR, "voice_clips.h")

    total = 0
    with open(c_path, "w", encoding="utf-8") as c, \
         open(h_path, "w", encoding="utf-8") as h:
        header = (f"// 由 tools/gen_voice_clips.py 產生，請勿手動編輯。\n"
                  f"// 人聲：{args.voice}　語速：{args.rate}　"
                  f"格式：{SAMPLE_RATE}Hz / 16bit / mono\n")
        c.write(header + '\n#include "voice_clips.h"\n')
        h.write(header + "\n#pragma once\n\n#include <stdint.h>\n\n"
                "#ifdef __cplusplus\nextern \"C\" {\n#endif\n")

        for name, text in CLIPS:
            pcm = normalize(synth(text, args.voice, args.rate))
            write_c(name, text, pcm, c)
            h.write(f"extern const int16_t {name}[];\n"
                    f"extern const unsigned {name}_len;\n")
            total += len(pcm)
            print(f"  {name:12s} 「{text}」 "
                  f"{len(pcm) / SAMPLE_RATE / 2:.2f}s  {len(pcm) / 1024:.1f} KB")

        h.write("\n#ifdef __cplusplus\n}\n#endif\n")

    print(f"  合計 {total / 1024:.1f} KB PCM -> {OUT_DIR}")


if __name__ == "__main__":
    main()
