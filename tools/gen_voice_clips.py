#!/usr/bin/env python3
"""產生倒數語音片段，轉成可直接嵌入韌體的 C 陣列。

    python3 tools/gen_voice_clips.py                      # 預設 piper
    python3 tools/gen_voice_clips.py --slow 1.6           # 唸慢一點
    python3 tools/gen_voice_clips.py --engine say         # 改用 macOS say
    python3 -m piper.download_voices --data-dir ~/.config/piper en_US-<voice>

在 Mac 上先合成、把 PCM 燒進 flash 最可控：發音、語氣、語速都在產生階段
決定，板子只要把樣本推進 I2S。

為什麼用 piper 而不是 say：`say` 的 --rate 對這種短句幾乎沒作用
（實測 "Go Shoot!" 從 rate 170 調到 100 只有 0.68s -> 0.80s），
piper 的 --length-scale 才是真正在拉長發音。say 保留為不想裝 piper 時的退路。

輸出格式固定 22050Hz / 16bit / 單聲道，與 src/audio/audio_bus.h 的
kAudioSampleRate 一致；改一邊就要改另一邊。22050 也正好是 piper 的原生輸出
取樣率，因此不需要重新取樣。
"""

import argparse
import array
import os
import subprocess
import sys
import tempfile
import wave

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(ROOT, "src", "audio", "clips")
PIPER_DATA_DIR = os.path.expanduser("~/.config/piper")

SAMPLE_RATE = 22050

# (C 識別字, 要唸的文字)
#
# 播報用的片段刻意只有「誰」與「什麼方式」兩類，湊成兩段一句。
# 不錄點數：點數規則是可設定的，要涵蓋所有組合片段會膨脹，而且分數就顯示
# 在螢幕上，播出來只是冗餘資訊，徒然拖長播報、打斷對戰節奏。
#
# 全部用英文：Beyblade 的官方術語（Burst / Xtreme Finish）本來就是英文，
# 中譯反而拗口。
CLIPS = [
    # 倒數。數字後面刻意加句點：piper 對單獨一個短音節沒有上下文可依據，
    # 韻律預測會亂跳 ——「Two」在 length_scale 1.1 是 0.186s、調到 1.3 反而
    # 掉到 0.163s。加了句點模型才當成完整語句處理，「Two.」在兩種語速下都
    # 穩定在 0.26s 左右，比原本長三成，倒數的節奏才不會只有它一閃而過。
    ("clip_three", "Three."),
    ("clip_two", "Two."),
    ("clip_one", "One."),
    ("clip_go", "Go Shoot!"),
    # 播報 —— 誰
    ("clip_player_one", "Player One"),
    ("clip_player_two", "Player Two"),
    # 播報 —— 什麼方式
    ("clip_spin", "Spin Finish"),
    ("clip_over", "Over Finish"),
    ("clip_burst", "Burst Finish"),
    ("clip_xtreme", "Xtreme Finish"),
    # 播報 —— 比賽結束
    ("clip_wins", "Wins"),
    # 播報 —— 玩家名稱
    # 預設名單裡唸得出來的名字各錄一段，播報時就講名字而不是 Player One。
    # 只錄 ASCII：中文名字要換成中文 TTS 模型，跟其餘片段的人聲會對不起來。
    # 名單以外的自訂名字、以及中文名，播報時退回 Player One / Player Two。
    ("clip_name_weiting", "Weiting"),
    ("clip_name_yoshi", "Yoshi"),
    ("clip_name_emma", "Emma"),
    ("clip_name_wilber", "Wilber"),
    ("clip_name_opponent", "Opponent"),
    ("clip_name_red", "Red"),
    ("clip_name_blue", "Blue"),
    ("clip_name_dragoon", "Dragoon"),
    ("clip_name_dranzer", "Dranzer"),
    ("clip_name_valkyrie", "Valkyrie"),
    ("clip_name_guest", "Guest"),
]


def synth_piper(text, voice, length_scale):
    fd, raw = tempfile.mkstemp(suffix=".wav")
    os.close(fd)
    try:
        subprocess.run(
            [sys.executable, "-m", "piper", "-m", voice,
             "--data-dir", PIPER_DATA_DIR, "-f", raw,
             "--length-scale", str(length_scale)],
            input=text, text=True, check=True,
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )
        return resample(raw)
    finally:
        os.unlink(raw)


def synth_say(text, voice, length_scale):
    # say 沒有真正的長度控制，length_scale 只能粗略對應到 --rate。
    rate = int(170 / max(length_scale, 0.1))
    fd, path = tempfile.mkstemp(suffix=".wav")
    os.close(fd)
    try:
        subprocess.run(
            ["say", "-v", voice, "-r", str(rate),
             "--data-format=LEI16@%d" % SAMPLE_RATE, "-o", path, text],
            check=True,
        )
        with wave.open(path) as w:
            return w.readframes(w.getnframes())
    finally:
        os.unlink(path)


def resample(path):
    """用 ffmpeg 統一成 SAMPLE_RATE/mono/s16。

    piper 原生就輸出 22050Hz，SAMPLE_RATE 也是 22050 時這一步不做重新取樣，
    只負責轉成單聲道 s16。SAMPLE_RATE 改成別的值時才會真的降／升取樣。
    """
    fd, out = tempfile.mkstemp(suffix=".wav")
    os.close(fd)
    try:
        subprocess.run(
            ["ffmpeg", "-y", "-i", path,
             "-ar", str(SAMPLE_RATE),
             "-ac", "1", "-sample_fmt", "s16", out],
            check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )
        with wave.open(out) as w:
            assert w.getframerate() == SAMPLE_RATE and w.getnchannels() == 1
            return w.readframes(w.getnframes())
    finally:
        os.unlink(out)


def trim_silence(pcm, threshold=300):
    """去掉頭尾靜音。

    TTS 的輸出前後常帶幾百毫秒空白。倒數是 1 秒一拍，這段空白會讓聲音
    聽起來慢半拍，也壓縮到真正能發聲的時間。
    """
    a = array.array("h")
    a.frombytes(pcm)
    start, end = 0, len(a)
    while start < end and abs(a[start]) < threshold:
        start += 1
    while end > start and abs(a[end - 1]) < threshold:
        end -= 1
    # 前後各留 10ms，避免切到氣音起頭
    pad = SAMPLE_RATE // 100
    start = max(0, start - pad)
    end = min(len(a), end + pad)
    return a[start:end].tobytes()


def fade_edges(pcm, ms=8):
    """頭尾各做幾毫秒淡入淡出。

    去靜音是硬切，切點的振幅不一定為零；直接播會有「喀」聲，
    在小喇叭上比語音本身還突兀。
    """
    a = array.array("h")
    a.frombytes(pcm)
    n = SAMPLE_RATE * ms // 1000
    n = min(n, len(a) // 2)
    for i in range(n):
        g = i / n
        a[i] = int(a[i] * g)
        a[len(a) - 1 - i] = int(a[len(a) - 1 - i] * g)
    return a.tobytes()


def normalize(pcm, peak_ratio=0.85):
    """把振幅拉到接近滿刻度。

    TTS 輸出通常偏小聲，直接推進小喇叭幾乎聽不到。在產生階段正規化比在
    板子上乘倍數好 —— 板上乘會把量化雜訊一起放大。
    """
    a = array.array("h")
    a.frombytes(pcm)
    peak = max((abs(v) for v in a), default=0)
    if peak == 0:
        return pcm
    gain = (32767 * peak_ratio) / peak
    for i, v in enumerate(a):
        s = int(v * gain)
        a[i] = 32767 if s > 32767 else (-32768 if s < -32768 else s)
    return a.tobytes()


def write_c(name, text, pcm, f):
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
    ap.add_argument("--engine", choices=["piper", "say"], default="piper")
    ap.add_argument("--voice", default=None,
                    help="piper 的模型名或 say 的人聲名")
    # 1.0 是模型的自然語速。拉太大會把音素整個撐開，聽起來拖沓不自然
    # （試過 1.35，使用者回報「有點怪、不夠順暢」）。1.1 只是稍微放慢。
    ap.add_argument("--slow", type=float, default=1.1,
                    help="越大唸越慢（piper 的 length-scale，1.0 = 自然語速）")
    args = ap.parse_args()

    if args.engine == "piper":
        voice = args.voice or "en_US-ryan-high"
        model = os.path.join(PIPER_DATA_DIR, voice + ".onnx")
        if not os.path.exists(model):
            sys.exit(f"找不到 piper 語音模型：{model}\n"
                     f"下載：python3 -m piper.download_voices "
                     f"--data-dir {PIPER_DATA_DIR} {voice}")
        synth = synth_piper
    else:
        voice = args.voice or "Daniel"
        synth = synth_say

    os.makedirs(OUT_DIR, exist_ok=True)
    c_path = os.path.join(OUT_DIR, "voice_clips.c")
    h_path = os.path.join(OUT_DIR, "voice_clips.h")

    total = 0
    with open(c_path, "w", encoding="utf-8") as c, \
         open(h_path, "w", encoding="utf-8") as h:
        head = (f"// 由 tools/gen_voice_clips.py 產生，請勿手動編輯。\n"
                f"// 引擎：{args.engine}　人聲：{voice}　慢速倍率：{args.slow}\n"
                f"// 格式：{SAMPLE_RATE}Hz / 16bit / mono\n")
        c.write(head + '\n#include "voice_clips.h"\n')
        h.write(head + "\n#pragma once\n\n#include <stdint.h>\n\n"
                "#ifdef __cplusplus\nextern \"C\" {\n#endif\n")

        for name, text in CLIPS:
            # 先正規化再去靜音：反過來的話門檻是相對於未放大的訊號，
            # 會把真正的尾音當成靜音切掉（實測「Two」被砍到只剩 0.16 秒）。
            pcm = fade_edges(trim_silence(normalize(
                synth(text, voice, args.slow))))
            write_c(name, text, pcm, c)
            h.write(f"extern const int16_t {name}[];\n"
                    f"extern const unsigned {name}_len;\n")
            total += len(pcm)
            print(f"  {name:12s} 「{text}」 "
                  f"{len(pcm) / 2 / SAMPLE_RATE:.2f}s  {len(pcm) / 1024:.1f} KB")

        h.write("\n#ifdef __cplusplus\n}\n#endif\n")

    print(f"  合計 {total / 1024:.1f} KB PCM -> {OUT_DIR}")


if __name__ == "__main__":
    main()
