"""把 esp-sr 語音模型包一併燒進 flash。

`pio run -t upload` 預設只燒 bootloader、分割表與韌體。語音模型是獨立的
二進位，必須寫到 partitions.csv 裡標籤為 `model` 的分割區，否則
esp_srmodel_init("model") 會找不到模型而失敗。

位移**從 partitions.csv 解析**，不寫死。這個檔案已經改過一次配置，
把數字抄兩份遲早會漂移，然後燒出一個開得起來但語音不會動的韌體。
"""

import csv
import os
import sys

Import("env")  # noqa: F821  (PlatformIO 在執行時注入)

PROJECT_DIR = env["PROJECT_DIR"]  # noqa: F821
MODEL_BIN = os.path.join(PROJECT_DIR, "assets", "srmodels.bin")
PARTITIONS = os.path.join(PROJECT_DIR, "partitions.csv")
PARTITION_LABEL = "model"


def parse_int(text):
    text = text.strip()
    if text.lower().startswith("0x"):
        return int(text, 16)
    if text.upper().endswith("K"):
        return int(text[:-1]) * 1024
    if text.upper().endswith("M"):
        return int(text[:-1]) * 1024 * 1024
    return int(text)


def find_model_partition():
    """回傳 (offset, size)；找不到就回 None。"""
    with open(PARTITIONS, encoding="utf-8") as f:
        for row in csv.reader(f):
            if not row or row[0].strip().startswith("#"):
                continue
            if row[0].strip() != PARTITION_LABEL:
                continue
            return parse_int(row[3]), parse_int(row[4])
    return None


if not os.path.exists(MODEL_BIN):
    print(f"[sr-model] 找不到 {MODEL_BIN}，跳過語音模型燒錄。")
    print("[sr-model] 重新產生：bash tools/gen_sr_model.sh")
else:
    found = find_model_partition()
    if found is None:
        sys.stderr.write(
            f"[sr-model] partitions.csv 裡沒有標籤為 '{PARTITION_LABEL}' 的分割區\n")
        env.Exit(1)  # noqa: F821

    offset, size = found
    actual = os.path.getsize(MODEL_BIN)
    if actual > size:
        sys.stderr.write(
            f"[sr-model] srmodels.bin ({actual} bytes) 大於 model 分割區 "
            f"({size} bytes)，請加大 partitions.csv 的 model 分割區\n")
        env.Exit(1)  # noqa: F821

    print(f"[sr-model] srmodels.bin {actual/1048576:.2f} MB -> "
          f"0x{offset:X}（分割區 {size/1048576:.2f} MB）")
    env.Append(FLASH_EXTRA_IMAGES=[(hex(offset), MODEL_BIN)])  # noqa: F821
