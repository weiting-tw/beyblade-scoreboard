# Beyblade X 戰鬥計分器

Waveshare **ESP32-S3-Touch-LCD-1.85B**（1.85吋 360×360 圓形觸控螢幕）上的陀螺對戰計分器。

兩位玩家、可設定賽制、三種勝利類型（普通／爆裂／Xtreme Finish）、倒數動畫、撤銷、設定與歷史紀錄持久化。

需求規格見 [`docs/SPEC.md`](docs/SPEC.md)，實作偏離與已知風險見 [`docs/DECISIONS.md`](docs/DECISIONS.md)，
轉場與獲勝動畫的設計見 [`docs/MOTION.md`](docs/MOTION.md)。

---

## 1. 專案目錄結構

```
beyblade-scoreboard/
├── platformio.ini            平台設定（pioarduino + Arduino core 3.2.0）
├── partitions.csv            16MB flash 分割表
├── include/
│   └── lv_conf.h             LVGL 設定（官方版本 + 大字體、關掉 demo）
├── lib/
│   └── match/                計分核心：純 C++、零 Arduino 依賴、可主機端測試
│       ├── include/match_core.h
│       └── src/match_core.cpp
├── src/
│   ├── main.cpp              初始化與主迴圈
│   ├── bsp/                  官方驅動，原樣複製自 Waveshare repo
│   │   ├── Display_ST77916.*  esp_lcd_st77916.*
│   │   ├── Touch_CST816.*     I2C_Driver.*
│   │   └── LVGL_Driver.*
│   ├── drivers/              ES8311 / ES7210 codec 驅動（官方）
│   ├── audio/                I2S 匯流排與語音片段（22.05kHz PCM）
│   ├── probe/                音訊硬體診斷韌體（獨立 env）
│   ├── app/
│   │   ├── app.*             全域比賽狀態、歷史寫入
│   │   ├── settings_store.*  NVS 持久化
│   │   └── feedback.*        音效與語音播報（獨立 FreeRTOS 任務）
│   └── ui/
│       ├── ui.h  ui_theme.*  圓形螢幕版面基礎、轉場與動畫工具
│       ├── fonts.h           中文子集字型宣告
│       ├── fonts/            font_tc_16/22/30.c（由 gen_fonts.py 產生）
│       └── screen_*.cpp      九個畫面
├── tools/
│   ├── serial_watch.py       擷取序列埠輸出（畫面異常時第一個跑這個）
│   ├── gen_fonts.py          從原始碼字串常值產生中文子集字型
│   └── gen_voice_clips.py    產生倒數與播報語音片段（piper TTS）
├── test/
│   └── test_match_core/      23 個計分核心單元測試
├── docs/
│   ├── SPEC.md               需求規格
│   ├── DECISIONS.md          實作決策與規格偏離
│   └── MOTION.md             轉場與獲勝動畫設計
└── reference/                官方 repo（gitignored，389MB）
```

**分層原則**：`lib/match` 不知道 LVGL 或 Arduino 的存在，`src/ui` 不直接碰 NVS。
計分規則的正確性完全由主機端測試保證，不需要板子。

---

## 2. 函式庫與版本

| 項目 | 版本 | 來源 |
| --- | --- | --- |
| PlatformIO 平台 | `pioarduino 54.03.20` | ESP-IDF 5.4 + Arduino core 3.2.0 |
| Arduino ESP32 core | 3.2.0 | 與官方 `Examples/Arduino-V3.2.0` 一致 |
| LVGL | 8.4.0 | PlatformIO registry（與官方隨附版本相同） |
| ST77916 / CST816 驅動 | — | 官方 repo `Examples/Arduino-V3.2.0/examples/01_lvgl_demo`，原樣複製 |
| Noto Sans CJK TC | — | 中文字型來源（OFL 授權，可嵌入）。只嵌入實際用到的漢字子集（目前 99 字） |
| lv_font_conv | latest | 字型子集產生器（Node，僅開發時需要） |
| piper TTS | latest | 語音片段合成（Python，僅開發時需要） |
| Unity（測試） | 2.6.1 | 僅 native 環境 |

> PlatformIO **官方**的 `espressif32` 平台仍停在 Arduino core 2.0.x，無法建置官方範例，
> 因此改用 [pioarduino](https://github.com/pioarduino/platform-espressif32) fork。

---

## 3. 安裝與燒錄步驟

### 3.1 安裝 PlatformIO CLI

專案內已建好隔離環境（不影響系統 Python）：

```bash
cd beyblade-scoreboard
python3 -m venv .venv
.venv/bin/pip install platformio
```

### 3.2 跑單元測試（不需要板子）

```bash
.venv/bin/pio test -e native
```

### 3.3 編譯韌體

```bash
.venv/bin/pio run -e waveshare_s3_lcd185b
```

首次執行會下載約 1–2GB 的 ESP32 工具鏈。

### 3.4 燒錄

用 USB-C 接上板子，然後：

```bash
.venv/bin/pio run -e waveshare_s3_lcd185b -t upload
.venv/bin/pio device monitor
```

**進不了燒錄模式時**：按住 `BOOT` → 按一下 `RESET` → 放開 `BOOT`，再重跑 upload。

**燒錄埠要自己設**。埠名每台機器都不一樣，所以不放在版控裡：

```bash
cp platformio_local.ini.example platformio_local.ini
# 改成 pio device list 查到的埠
```

檔案不存在時 PlatformIO 會略過，編譯照樣能過，只有燒錄需要它。

**為什麼不讓它自動偵測**：開發機上若同時接著別片 ESP32，PlatformIO 可能燒到錯的
板子 —— 這在本專案實際發生過。1.85B 的特徵是 16MB 外部 flash + 8MB PSRAM。
1.85B 的辨識特徵是 **16MB 外部 flash**，用以下指令確認：

```bash
.venv/bin/pio device list                        # 列出候選埠
.venv/bin/esptool.py --port <埠> flash_id        # 看 flash 大小，應為 16MB
```

### 3.5 關鍵板子設定

這幾項來自官方 ESP-IDF 範例的 `sdkconfig.defaults`，**設錯會開不了機**：

| 設定 | 值 | 來源 |
| --- | --- | --- |
| Flash | 16MB, QIO | `CONFIG_ESPTOOLPY_FLASHMODE_QIO` / `FLASHSIZE_16MB` |
| PSRAM | **Octal (OPI)**, 80MHz | `CONFIG_SPIRAM_MODE_OCT` / `SPIRAM_SPEED_80M` |
| CPU | 240MHz | `CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240` |

在 `platformio.ini` 對應為 `board_build.arduino.memory_type = qio_opi`。
用 Arduino IDE 的話對應 **PSRAM: "OPI PSRAM"**、**Flash Mode: "QIO 80MHz"**、**Flash Size: "16MB"**。

---

## 4. 觸控座標校正方式

官方 `Lvgl_Touchpad_Read()`（`src/bsp/LVGL_Driver.cpp`）直接把 CST816S 回報的座標餵給 LVGL，
沒有任何旋轉或鏡射。第一次燒錄請先驗證方向。

**已實測，方向正確，不需要任何修正。** 四個邊緣的實際回報值：

| 點的位置 | x | y |
| --- | --- | --- |
| 最上緣 | 188 | **6** |
| 最下緣 | 192 | **355** |
| 最左緣 | **12** | 180 |
| 最右緣 | **350** | 205 |

上小下大、左小右大，沒有鏡射也沒有旋轉，中心落在 180 附近。下表的三種修正
都不需要套用；列在這裡是給換板子或改面板時重測用的。

**重測方法**：燒 `debug_serial` 版（`Lvgl_Touchpad_Read()` 裡有 `BEY_DEBUG_SERIAL`
包住的座標回報，只在按下那一瞬間印一行），然後 `tools/serial_watch.py -s 60`
邊錄邊點四個邊。不必再手動改程式碼取消註解。

**症狀與修法**（改在 `Lvgl_Touchpad_Read()` 內）：

| 症狀 | 修正 |
| --- | --- |
| 左右相反 | `data->point.x = 359 - touch_data.x;` |
| 上下相反 | `data->point.y = 359 - touch_data.y;` |
| 旋轉 90°（點上方反應在右方） | `data->point.x = touch_data.y; data->point.y = 359 - touch_data.x;` |

**幽靈觸控**（規格第 9 節）：若靜置時 `points` 偶發非 0 造成誤加分，
在 `Lvgl_Touchpad_Read()` 加上座標合理性檢查即可 —— 丟棄 `x` 或 `y` 超過 359 的回報。
本專案在應用層另有一道防線：加分一律要經過勝利類型選單或長按，單純點一下不會改變比分。

---

## 5. UI 操作說明

```
首頁 ─ 開始比賽 ─→ 賽制 ─ 下一步 ─→ 準備 ─ 開始 ─→ 倒數 3-2-1-GO ─→ 主計分
  │                                  │                                  │
  ├─ 快速對戰 ──沿用上次賽制─────────┘              ┌── 未達標 ──→ 局結果 ┤
  ├─ 設定 ─────→ 設定頁                            └── 已達標 ──→ 比賽結束
  └─ 歷史紀錄 ─→ 歷史頁
```

### 主計分畫面

| 操作 | 效果 |
| --- | --- |
| 點 `P1 勝` / `P2 勝` | 開啟勝利類型選單（普通 +1 / 爆裂 +2 / Xtreme +3 / 取消） |
| **長按**玩家分數區域 | 直接套用該玩家的「普通勝」，跳過選單 |
| 點 `撤銷` | 撤銷上一筆計分（無可撤銷時按鈕呈灰色停用） |
| 點 `重設` | 彈出「重設比賽？」二次確認後歸零 |

頂端顯示 `第 2 局    3 分制`。

分數變動時只有變動的那一邊會彈跳，一眼看得出加給誰（見 `docs/MOTION.md`）。

### 局結果畫面

`撤銷`（剛才按錯就用這顆）／ `下一局` ／ `結束`（提前結束，二次確認）。

### 撤銷

保存最近 **8** 筆計分（規格要求至少 5）。撤銷會一併還原分數、局數與比賽結束狀態 ——
Xtreme Finish 誤按導致比賽提前結束時，一次撤銷就能回到比賽中。

---

## 6. 賽制與點數設定方式

### 賽制（目標分數）

**賽制選擇頁**：`1` / `3` / `4` / `5` 捷徑按鈕，或用 `−` `+` 微調（範圍 1–20）。
按 `下一步` 時寫入 NVS，「快速對戰」下次就會沿用。

### 勝利點數

**設定頁**的 `普通勝` / `爆裂勝` / `Xtreme` 三列，各自 0–9 分。
預設 1 / 2 / 3。設定頁的調整會同步套用到 P1 與 P2（見 `docs/DECISIONS.md` D6）。

點數不是寫死的 —— 資料結構是 `ScoreRule { name, p1Points, p2Points, countsAsRound }`，
`RuleSet` 另外還定義了規格第 5 節要求預留的兩種結果：

| 結果 | P1 | P2 | 計入局數 |
| --- | --- | --- | --- |
| Double Out（雙方同時出界） | 0 | 0 | 是 |
| No Contest（無效局） | 0 | 0 | **否** |

這兩種目前沒有對應的 UI 按鈕（規格第 5 節只要求「預留資料模型」），
核心已完整支援並有測試涵蓋。要加按鈕只需在勝利類型選單多兩個項目。

---

## 7. 儲存資料格式

全部存在 NVS（Arduino `Preferences`），namespace `bey`。

| Key | 型別 | 內容 |
| --- | --- | --- |
| `cfg` | blob | `AppSettings`：magic + MatchConfig + RuleSet + 亮度 + 休眠秒數 + 震動開關 |
| `hist` | blob | `MatchRecord[n]`，index 0 為最新 |
| `histN` | int | 歷史筆數（0–20） |

```cpp
struct MatchRecord {          // 52 bytes
    char     p1Name[20];
    char     p2Name[20];
    uint8_t  score1, score2;
    uint8_t  rounds;
    uint8_t  winner;          // 0=未定 1=P1 2=P2 3=平手
    uint32_t timestamp;       // RTC epoch 秒；未接 RTC 時為 0
};
```

`AppSettings::magic`（目前 `0x42455902`）用於版本識別。改動 struct 版面時務必把 magic 加一 ——
不改的話會把舊版位元組當成新版解讀，讀到亂數設定。magic 不符時自動回退預設值。
（v1 → v2 就是因為規則名稱從英文改成中文而升版。）

寫入時機：設定頁按 `返回` 時一次寫入（避免每按一下 ± 就磨損 flash）；
比賽結束時寫入歷史（受設定頁 `保存紀錄` 開關控制）。

---

## 8. 已知限制

1. **新增中文字串後必須重跑 `tools/gen_fonts.py`** —— 嵌入的是實際用到的漢字子集（目前 99 字，以 `tools/gen_fonts.py --list` 為準），
   沒收錄的字會在螢幕上變成空白方塊，而且**編譯不會報錯**。見下方第 13 節。
2. **玩家名稱只能從 12 組預設中選** —— 圓形螢幕放不下可用的鍵盤。清單在 `src/ui/screen_names.cpp`。
3. **歷史紀錄沒有時間戳** —— `timestamp` 恆為 0。RTC **硬體確實存在**（實測 0x51 有回應），
   純粹是還沒實作 `nowEpoch()`。
4. **沒有自動休眠** —— 設定欄位已保留但屬 Phase 5。
5. **沒有震動** —— 板上沒有震動馬達（I2C 上也沒有觸覺驅動器），要外接才能實作。
   設定頁刻意不顯示震動開關：撥了沒反應的開關比缺一個功能更讓人困惑。
   呼叫點（得分、撤銷）與 `AppSettings::enableVibration` 欄位都留著，
   外接馬達後只要填 `feedbackHaptic()` 就會動。音效與語音播報已實作（見第 9 節）。
6. **沒有電池電量顯示** —— BQ27220 屬 Phase 5，實測 0x55 有回應，官方有 `02_lvgl_BQ27220` 範例。
7. **麥克風沒有任何用途** —— ES7210 焊在板上且實測有訊號，但離線語音辨識已移除，
   正式韌體只開 I2S 的 TX 通道。要驗證麥克風請用 `audio_probe`。
8. **勝利類型必須手動選擇** —— 不自動偵測爆裂／出界／Xtreme Finish，那需要競技場端的額外感測器。
9. **顯示與觸控尚未經人眼確認** —— 見下節。

### 已驗證 vs. 未驗證

**已在實機驗證**（見第 12 節）：PSRAM 設定、LCD 驅動初始化、觸控 IC 通訊、
應用層開機無 crash、七個 I²C 裝置全部在線、麥克風有訊號。

**仍需人眼／人耳確認**：

- 畫面是否正常顯示（`docs/DECISIONS.md` R1：官方的 `full_refresh = 1` 搭配
  1/10 螢幕大小緩衝區，若有撕裂或只更新局部，先試著改成 0）
- 中文字是否正常，有無空白方塊（漏字代表 `gen_fonts.py` 需重跑）
- 觸控座標方向是否正確（校正方式見第 4 節）
- 縮放動畫是否流暢、有無記憶體不足
- 喇叭是否實際發聲（用 `audio_probe` 的嗶聲測）

---

## 9. 音效與語音播報

硬體（規格第 10 節）：

```
Codec ES8311 / 喇叭致能 GPIO9
I2S: MCLK 2, BCLK 48, LRCK 38, DOUT 47
```

`src/audio/audio_bus.*` 擁有 I2S 與 codec，`src/app/feedback.*` 是唯一的使用者：

```cpp
void feedbackPlay(Sfx sfx);                              // 短提示音
void feedbackAnnounce(Voice first, Voice second = ...);  // 拼句播報
void feedbackHaptic(uint16_t ms);                        // 板上無馬達，目前 no-op
```

播放跑在自己的 FreeRTOS 任務（優先權 6，釘在 core 0）。I2S 的 DMA 只有約 65ms
且 `auto_clear = true`，餵不及會直接輸出靜音 —— 這是優先權訂得高的原因，
放在 LVGL 執行緒則會造成畫面卡頓。呼叫端一律非阻塞：佇列滿了就丟棄，
音效過期沒有意義。

**素材不放檔案系統。** 倒數（Three / Two / One / Go Shoot!）與播報
（Player One / Burst Finish / Wins ...）是 `tools/gen_voice_clips.py` 在 Mac 上
用 piper TTS 合成、直接嵌成 C 陣列的 PCM（22.05kHz / 16bit / mono，合計約 264KB）；
其餘提示音在板上即時合成正弦波。發音、語氣、語速都在產生階段決定，
板子只要把樣本推進 I2S，不需要檔案系統也沒有讀檔延遲。

取樣率 22.05kHz 是刻意選的：齒擦音（Xtreme / Finish 的 sh、th）落在 4–8kHz，
16kHz 只有 8kHz 頻寬會把它們削掉；22.05k 給到 11kHz 頻寬就夠，
而且是 piper 的原生輸出取樣率，產生片段時不必重新取樣。44.1k 只會讓體積翻倍。

改動語音內容後要重跑：

```bash
python3 tools/gen_voice_clips.py    # 注意是系統 python3，.venv 裡沒有 piper
```

---

## 10. 3D 外殼所需實測尺寸

第一版不設計完整外殼（規格第 11 節）。以下是設計前**必須拿游標卡尺量**的項目 ——
官方 repo 只提供 `hardware/ESP32-S3-Touch-LCD-1.85B Rev1.1.pdf` 原理圖，**沒有機構圖**，
所有尺寸都得自己量。

### 必量清單

**PCB 本體**
- [ ] PCB 外形長 × 寬 × 厚
- [ ] 四個角的圓角半徑
- [ ] 安裝孔：孔徑、孔心到板邊距離、孔距
- [ ] 板子背面最高元件的高度（決定內框深度）

**螢幕**
- [ ] 圓形玻璃外徑
- [ ] 可視區直徑（規格是 1.85吋 / 360×360，實際可視區要量）
- [ ] 玻璃面到 PCB 正面的高度
- [ ] 玻璃中心相對 PCB 中心的偏移量（通常不完全置中）
- [ ] FPC 排線的出線位置、寬度與最小彎曲半徑

**接口與按鍵開孔位置**（皆量測「孔心到 PCB 邊緣」與「孔心到板底」）
- [ ] USB-C：外框長 × 寬 × 高
- [ ] PWR 按鈕
- [ ] BOOT 按鈕
- [ ] RESET 按鈕
- [ ] 麥克風孔
- [ ] 喇叭出音孔
- [ ] I²C / UART 排針位置與間距

**選配件**
- [ ] 電池：長 × 寬 × 厚、接頭型號與線長
- [ ] 喇叭：直徑 × 厚度、線長
- [ ] 電池接座在 PCB 上的位置

### 結構建議

- **固定方式**：優先用 PCB 原有安裝孔配 M2 銅柱／自攻螺絲。若孔位不便，改用四個 0.4mm 壓片壓住 PCB 邊緣（避開元件），配合上下殼夾持。
- **分件**：上蓋（螢幕環）+ 下殼（電池與接口）兩件式，用 M2 螺絲或卡扣接合。磁吸底座做成第三件獨立零件。
- **螢幕壓框**：圓形螢幕的可視區與玻璃外徑不同，上蓋開孔要對可視區留 0.5mm 餘量，避免遮到邊緣像素。
- **公差**：FDM 列印建議孔位單邊留 0.2mm、USB-C 開孔單邊留 0.3mm。第一版先印一個「只有開孔與孔位的驗證片」再印完整外殼。

### 建模工具

外殼是機構件，需要精確尺寸與公差，**不適合用生成式 3D（MeshyAI 那類）** ——
那產出的是有機造型 mesh，孔位對不上。建議用參數化 CAD 原始碼：

- **OpenSCAD**（`brew install openscad`）—— 純文字，可 CLI 匯出 STL
- **CadQuery / build123d**（Python）—— 支援圓角與去料，可匯出 STEP 方便後續修改

量完尺寸後把數據交給我，我可以直接產出參數化原始碼；你在本機 render 後回報哪裡不合再迭代。

---

## 11. 開發階段進度

| Phase | 內容 | 狀態 |
| --- | --- | --- |
| 1 | 確認硬體：LCD、觸控、旋轉方向、360×360 座標 | **待實機驗證** |
| 2 | 基本計分器：P1/P2、+1、重設、目標分數判定、勝者畫面 | 完成（23 項單元測試） |
| 3 | 勝利類型：普通／爆裂／Xtreme、可設定點數、局數紀錄 | 完成（含測試） |
| 4 | 完整 UX：倒數、撤銷、玩家名稱、賽制設定、儲存、歷史 | 完成（待實機驗證） |
| 5 | 音效與語音播報（已實作）／震動、IMU、電池、自動休眠（未實作） | 部分完成 |
| 6 | 3D 列印外殼 | 待實測尺寸（見第 10 節） |
| — | 中文 UI、轉場與獲勝動畫（規格外追加） | 完成（待實機驗證） |

---

## 12. 硬體實測結果

以下是實際燒錄後從板子上量到的，不是規格書抄來的。

### 開機自檢（正式韌體）

```
The SPI initialization succeeded.
Install LCD driver of st77916
Vendor-specific initialization for case 2.
TouchPad_Version:0x00  ChipID:0xb5  ProjID:0x71  FwVersion:0x04
[bey] ready, heap=217348 psram=8384444
```

- **PSRAM 8,384,444 bytes 可用** —— 證實 `board_build.arduino.memory_type = qio_opi` 設定正確
- ST77916 LCD 驅動安裝成功
- CST816 觸控在 I²C 上回應（ChipID `0xB5`）
- 應用層 setup 跑完，無 crash

### I²C 裝置清單（`pio run -e audio_probe`）

| 位址 | 裝置 | 用途 |
| --- | --- | --- |
| `0x15` | CST816S | 觸控 |
| `0x18` | **ES8311** | 喇叭 codec |
| `0x40` | **ES7210** | 麥克風 ADC |
| `0x51` | PCF85063 | RTC（歷史紀錄時間戳） |
| `0x55` | BQ27220 | 電量計 |
| `0x6B` | QMI8658 | IMU |
| `0x7E` | 未知 | 尚未查明 |

七個裝置全部在線。這代表：

- **音效與語音播報可行** —— ES8311 在線，喇叭實測會發聲；
  ES7210 也在（環境噪音 peak 150–400，拍手會跳到數千），只是目前沒有用途
- **歷史紀錄時間戳可做** —— RTC 存在，只差 `nowEpoch()` 的實作
- **電池電量、IMU 搖晃操作可做** —— 都屬 Phase 5

### 診斷韌體

```bash
.venv/bin/pio run -e audio_probe -t upload
.venv/bin/pio device monitor
```

掃 I²C、印麥克風即時音量條、每 4 秒播一次 1kHz 嗶聲。
測完記得燒回正式韌體：`.venv/bin/pio run -t upload`。

---

## 13. 中文字型：新增字串後必須重新產生

UI 是中文的，但嵌入的**不是全字集** —— Noto Sans CJK 全字集要 1–2MB，
這個專案目前用到 99 個漢字／全形符號（實際數量跑 `tools/gen_fonts.py --list` 會印出來）。子集由 `tools/gen_fonts.py` 從原始碼自動掃描產生。

### 何時要重跑

**任何時候你在程式裡新增或修改了中文字串。** 沒收錄的字會在螢幕上顯示成
空白方塊，而且編譯階段完全不會報錯 —— 只有燒進板子才看得出來。

```bash
# 一次性設定（只需做一次）
cd tools && npm install lv_font_conv && cd ..

# 每次改動中文字串後
python3 tools/gen_fonts.py

# 只想看會收錄哪些字、各自來自哪個檔案
python3 tools/gen_fonts.py --list
```

### 腳本做了什麼

從 `src/ui`、`src/app`、`lib/match` 掃描 **字串常值裡** 的中日韓字元，
產生三個字級的字型到 `src/ui/fonts/`。

兩個關鍵細節：

- **不掃註解**。本專案註解全是中文，一併收進去會讓子集膨脹好幾倍。
- **要掃 `lib/match`**。規則名稱（如「P1 爆裂勝」）定義在計分核心而非 UI 層。

### 需求

- Node.js（`lv_font_conv` 是 Node 工具）
- `~/Library/Fonts/NotoSansCJKtc-{Medium,Bold}.otf`
  （[Noto CJK](https://github.com/notofonts/noto-cjk/releases)，OFL 授權可嵌入）

產生出來的 `.c` 檔**有進版控**，所以一般編譯不需要 Node 或字型檔 ——
只有要改中文字串時才需要。

字級選用與其他細節見 `docs/DECISIONS.md` D1。
