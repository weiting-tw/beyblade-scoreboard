# Beyblade X 戰鬥計分器 — 需求規格

> 來源：使用者於 2026-08-15 提供的開發任務說明。本檔為專案的需求真實來源（source of truth）。
> 實作決策與偏離規格之處記於 `docs/DECISIONS.md`。

## 1. 專案目標

使用 Waveshare **ESP32-S3-Touch-LCD-1.85B** 製作可攜式、觸控操作的 Beyblade X 戰鬥計分器。

- 支援 2 位玩家
- 顯示目前比分
- 觸控增加分數
- 支援撤銷上一筆加分
- 支援重新開始比賽
- 支援不同比賽賽制
- 顯示目前局數與勝者
- 具備倒數開始動畫
- 比賽結果可保存
- UI 適合 360 × 360 圓形螢幕
- 未來可放入 3D 列印的陀螺／競技場風格外殼

## 2. 硬體資訊

開發板：Waveshare ESP32-S3-Touch-LCD-1.85B

- 官方 Repo：https://github.com/waveshareteam/ESP32-S3-Touch-LCD-1.85B
- 官方文件：https://docs.waveshare.com/ESP32-S3-Touch-LCD-1.85B

### 核心硬體

| 項目 | 規格 |
| --- | --- |
| MCU | ESP32-S3R8 |
| CPU | 雙核心 Xtensa LX7，最高 240 MHz |
| SRAM | 512 KB |
| PSRAM | 8 MB |
| Flash | 16 MB |
| 螢幕 | 1.85 吋圓形 LCD |
| 解析度 | 360 × 360 |
| LCD 驅動 | ST77916 |
| LCD 介面 | QSPI |
| 觸控 IC | CST816S |
| 觸控介面 | I²C |
| IMU | QMI8658 |
| RTC | PCF85063 |
| 無線 | Wi-Fi 2.4 GHz、Bluetooth 5 LE |

### 重要 GPIO

```text
LCD CS    = GPIO21
LCD PCLK  = GPIO40
LCD DATA0 = GPIO46
LCD DATA1 = GPIO45
LCD DATA2 = GPIO42
LCD DATA3 = GPIO41
LCD RST   = GPIO3
LCD BL    = GPIO5

Touch SCL = GPIO10
Touch SDA = GPIO11
Touch RST = GPIO1
Touch INT = GPIO4
Touch I²C address = 0x15

IMU I²C address = 0x6B
RTC I²C address = 0x51
```

**限制**：優先使用官方 Repo 的 LCD、觸控與 BSP 初始化方式。
**不要**套用 ESP32-S3-Touch-AMOLED-1.75 的顯示器設定。

## 3. 開發環境

第一版採用：官方 LCD 初始化 + 官方 CST816S 觸控初始化 + LVGL + 設定持久化。

**第一版不要**整合 Home Assistant、音訊或網路功能。

## 4. 比賽資料模型

```cpp
struct Player {
    String name;
    int score;
};

struct MatchConfig {
    int targetScore;       // 例如 3、4、5
    int maxRounds;         // 可選，0 代表不限
    bool enableSound;
    bool saveHistory;
};

struct MatchState {
    Player player1;
    Player player2;
    int roundNumber;
    bool matchStarted;
    bool matchFinished;
    int winner;             // 0 = 尚未結束，1 = P1，2 = P2，3 = 平手
};
```

玩家名稱預設 `P1` / `P2`，但需提供設定頁允許修改（例如 `Weiting` / `Opponent`）。

## 5. 計分規則設計

計分規則**不得硬編碼成唯一模式**，必須可設定。

### 預設模式：一般點數制

- P1 獲勝：P1 +1
- P2 獲勝：P2 +1
- 達到目標分數後比賽結束
- 預設目標分數：**3 分**
- 可選：1 / 3 / 4 / 5 / 自訂

### 可擴充的結果類型

資料模型需預留：`P1 勝`、`P2 勝`、`雙方同時出界`、`無效局`、`撤銷`。

點數設計為可配置：

```cpp
struct ScoreRule {
    String resultName;
    int player1Points;
    int player2Points;
};
```

預設對應 Beyblade X 風格：普通勝利 +1、爆裂 +2、Xtreme Finish +3。
第一版不自行假設官方規則，一律走可配置路徑。

**重要限制**：第一版採「每局**手動**選擇勝利類型與點數」。
不要嘗試自動偵測爆裂、Xtreme Finish 或陀螺出界 —— 那些需要額外感測器與競技場硬體。

## 6. UI 頁面規劃

圓形螢幕通則：避免在四角放置重要按鈕。

### 頁面一：首頁／待機

標題 `BEYBLADE X` / `BATTLE SCORE`。
按鈕：`[開始比賽]` `[快速對戰]` `[設定]` `[歷史紀錄]`

### 頁面二：賽制選擇

分數制：`[1 分制]` `[3 分制]` `[4 分制]` `[5 分制]` `[自訂]`
模式：`[一般模式]` `[快速模式]` `[訓練模式]`（快速模式直接沿用上次設定）

### 頁面三：比賽準備

顯示雙方名稱與目標分數。
按鈕：`[開始倒數]` `[修改玩家名稱]` `[修改賽制]` `[返回]`

### 頁面四：倒數畫面

大型動畫 `3` → `2` → `1` → `GO!`
建議：圓形進度動畫、顏色變化、螢幕中央大型數字、預留震動／音效。

### 頁面五：主計分畫面

```text
┌────────────────────┐
│      ROUND 2       │
│                    │
│   P1        P2     │
│   2         1      │
│                    │
│ [P1勝]  [P2勝]      │
│                    │
│ [撤銷] [重設]       │
└────────────────────┘
```

操作：

- 點擊 `P1勝` / `P2勝`：開啟勝利類型選單（`[普通 +1]` `[爆裂 +2]` `[Xtreme +3]` `[取消]`）
- 長按玩家區域：直接加 1 分
- 點擊撤銷：回復上一個比分
- 點擊重設：**要求二次確認**
- 左右滑動：切換功能頁

### 頁面六：局結果

顯示本局勝者、得分、目前比分。
按鈕：`[下一局]` `[返回計分]` `[結束比賽]`
若已達目標分數則顯示 `MATCH WINNER`。

### 頁面七：比賽完成

顯示勝者、最終比分、總局數。
按鈕：`[新比賽]` `[儲存結果]` `[返回首頁]`

### 頁面八：設定頁

修改玩家名稱／目標分數／勝利類型點數、音效開關、震動開關、亮度、自動休眠時間、清除歷史、恢復預設值。

## 7. 撤銷功能

必須實作比分歷史堆疊，**至少支援撤銷最近 5 次操作**。

```cpp
struct ScoreEvent {
    int roundNumber;
    int player;
    int points;
    String resultType;
    int previousScoreP1;
    int previousScoreP2;
};
```

## 8. 儲存功能

持久化項目：玩家名稱、上次賽制、目標分數、自訂勝利點數、音效設定、螢幕亮度、最近比賽紀錄。

歷史資料限制最近 **20 場**，格式：

```json
{
  "winner": "P1",
  "player1": "Weiting",
  "player2": "Opponent",
  "score1": 3,
  "score2": 1,
  "rounds": 3,
  "timestamp": 0
}
```

時間不足時，第一版可只保存設定、不保存完整歷史。

## 9. 觸控操作要求

- 重要按鈕需足夠大（建議至少 60 × 45 px，依 360 × 360 圓形畫面調整）
- 重設需二次確認
- 比分按鈕不放在圓形邊緣
- 顯示觸控 feedback 與短暫動畫
- 避免觸控座標因螢幕方向錯位，需測試旋轉方向
- 避免 phantom touch 造成誤加分

## 10. 音效與未來硬體擴充

第一版**只預留介面**，不實作。

未來音效：`Beyblade!`、`3、2、1、Let it Rip!`、得分提示音、勝利音、Xtreme Finish 音效。

音訊硬體：

```text
Codec：ES8311
麥克風／回音消除：ES7210
喇叭控制：GPIO9
I2S MCLK：GPIO2
I2S BCLK：GPIO48
I2S LRCK：GPIO38
I2S DOUT：GPIO47
I2S DIN：GPIO39
```

不得因音訊功能影響第一版計分器穩定性。

## 11. 3D 列印外殼需求

未來移除原本 CNC 鋁殼，只保留 LCD／觸控模組 + PCB + 可選電池 + 可選喇叭。

外殼概念：Beyblade X 競技場風格、X 形外框、陀螺發射器風格、桌面計分台、可攜式手持、磁吸式底座。

需預留開孔：LCD 可視區、USB-C、PWR 按鈕、BOOT 按鈕、麥克風孔、喇叭出音孔、電池空間、I²C／UART 接頭。

**第一版不設計完整外殼**，只輸出：PCB 與螢幕量測需求、外殼結構建議、固定方式選擇、需要實測的尺寸清單。

## 12. 開發階段

| Phase | 內容 |
| --- | --- |
| 1 | 確認硬體：建立專案、燒錄官方 LCD 範例、確認 LCD 顯示、CST816S 觸控、旋轉方向、360×360 座標 |
| 2 | 基本計分器：P1/P2 顯示、點擊 +1、重設、目標分數判定、勝者畫面 |
| 3 | Beyblade X 勝利類型：普通／爆裂／Xtreme、可設定點數、局數紀錄 |
| 4 | 完整 UX：倒數動畫、撤銷、玩家名稱、賽制設定、設定儲存、歷史紀錄 |
| 5 | 音效與硬體：喇叭、震動馬達、IMU 搖晃操作、電池狀態、自動休眠 |
| 6 | 3D 列印外殼：實測尺寸、設計內框與外殼、輸出 STL／STEP、列印修正 |

## 13. 最終交付內容

1. 專案目錄結構
2. 可編譯原始碼
3. 平台設定檔
4. 使用的函式庫與版本
5. 安裝與燒錄步驟
6. 觸控座標校正方式
7. UI 操作說明
8. 賽制與點數設定方式
9. 儲存資料格式
10. 已知限制
11. 後續音效整合方式
12. 3D 外殼所需實測尺寸
