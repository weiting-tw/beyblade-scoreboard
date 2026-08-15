#!/usr/bin/env bash
# 重新產生 assets/srmodels.bin（esp-sr 語音模型包）。
#
# 一般開發不需要跑這個 —— assets/srmodels.bin 已經在版控裡。
# 只有在要換喚醒詞或換命令詞模型時才需要。
#
# 為什麼要自建：Arduino core 內建的 srmodels.bin 只打包了英文模型
# （wn9_hiesp + mn5q8_en），沒有任何中文模型。
#
# 注意：Arduino 的 ESP_SR 包裝層把喚醒詞寫死成 "hiesp"、命令詞寫死成
# ESP_MN_ENGLISH，所以本專案不使用它，改為直接呼叫 esp-sr 的 C API。
# 詳見 src/voice/。
set -euo pipefail

cd "$(dirname "$0")/.."
PROJECT_DIR="$(pwd)"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

# 要打包哪些模型。改這裡就能換喚醒詞或命令詞模型。
WAKENET="wn9_nihaoxiaozhi"   # 「你好小智」
MULTINET="mn7_cn"            # 中文命令詞辨識

echo "==> 取得 esp-sr 模型（sparse checkout，約 62MB）"
git clone --depth 1 --filter=blob:none --sparse \
    https://github.com/espressif/esp-sr.git "$WORK/esp-sr"
git -C "$WORK/esp-sr" sparse-checkout set model

MODEL_SRC="$WORK/esp-sr/model"
STAGE="$WORK/stage"
mkdir -p "$STAGE"

echo "==> 組裝：$WAKENET + $MULTINET"
cp -r "$MODEL_SRC/wakenet_model/$WAKENET" "$STAGE/"
cp -r "$MODEL_SRC/multinet_model/$MULTINET" "$STAGE/"

# MULTINET6 與 MULTINET7 額外需要 fst 模型（見 esp-sr 的 movemodel.py）。
case "$MULTINET" in
    mn6*|mn7*)
        echo "    $MULTINET 需要 fst，一併打包"
        cp -r "$MODEL_SRC/multinet_model/fst" "$STAGE/"
        ;;
esac

echo "==> 打包"
python3 "$MODEL_SRC/pack_model.py" -m "$STAGE" -o srmodels.bin
mkdir -p "$PROJECT_DIR/assets"
mv "$STAGE/srmodels.bin" "$PROJECT_DIR/assets/srmodels.bin"

SIZE=$(wc -c < "$PROJECT_DIR/assets/srmodels.bin")
echo "==> 完成：assets/srmodels.bin  $((SIZE / 1024)) KB"
echo "    partitions.csv 的 model 分割區必須大於這個大小。"
echo "    燒錄：.venv/bin/pio run -t upload（extra script 會自動一併燒）"
