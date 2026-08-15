# 離線語音控制

喚醒詞「**你好小智**」+ 11 個中文命令詞，**完全離線**：不連網、不需伺服器、
不需帳號。辨識在 ESP32-S3 上跑完，延遲是毫秒級。

---

## 怎麼用

1. 說「**你好小智**」→ 序列埠印出「已喚醒，請說指令」
2. 5.8 秒內說出命令
3. 逾時或辨識完成後自動回到待機

| 說法 | 動作 | 生效條件 |
| --- | --- | --- |
| 一號普通勝 | P1 +1 | 比賽進行中 |
| 一號爆裂勝 | P1 +2 | 比賽進行中 |
| 一號極限勝 | P1 +3 | 比賽進行中 |
| 二號普通勝 / 二號爆裂勝 / 二號極限勝 | P2 同上 | 比賽進行中 |
| 取消上一分 | 撤銷 | 有可撤銷的計分 |
| 下一局 | 回到計分畫面 | 比賽進行中 |
| 開始比賽 | 進入倒數 | 比賽尚未開始 |
| 重新開始 | 比分歸零（**跳二次確認**） | 比賽已開始 |
| 結束比賽 | 提前收攤（**跳二次確認**） | 比賽進行中 |

點數依設定頁的規則，不是寫死的 —— 把「爆裂」改成 +3，語音加的也是 +3。

**「重新開始」與「結束比賽」不會直接執行**，而是彈出二次確認，要用手點。
語音誤觸發的代價在這兩個命令上最大（整場歸零），而語音不能自己確認自己。

---

## 為什麼不用 Arduino 的 ESP_SR 函式庫

Arduino core 3.2.0 內建 `ESP_SR`，但它把模型寫死成英文
（`esp32-hal-sr.c` 第 344、350 行）：

```c
afe_config.wakenet_model_name = esp_srmodel_filter(models, ESP_WN_PREFIX, "hiesp");
char *mn_name = esp_srmodel_filter(models, ESP_MN_PREFIX, ESP_MN_ENGLISH);
```

就算把中文模型燒進 `model` 分割區，這層 filter 也找不到、直接失敗。

因此 `src/voice/bey_sr.{c,h}` 是 `esp32-hal-sr.{c,h}` 的改編版，**只差三處**：

1. 喚醒詞 `"hiesp"` → `"nihaoxiaozhi"`
2. 命令詞 `ESP_MN_ENGLISH` → `ESP_MN_CHINESE`
3. 公開符號加 `bey_` 前綴，避免與 Arduino 函式庫撞名

其餘邏輯（AFE 設定、feed/detect 任務、事件群組）**保持原樣** ——
那是已驗證的實作，上游更新時只要重新套用這三處差異即可。

底層函式庫本身是支援中文的：`libmultinet.a` 裡有 `ctc_beam_search_decoder_cn`，
缺的只是模型資料。

---

## 命令詞必須用拼音，不能用漢字

這是實作過程中最花時間的一個坑。

esp-sr 文件說 MultiNet6/7 中文「only accepts **grapheme** inputs」，讀起來像是
可以直接寫漢字。**實測不行。** 註冊繁體漢字時 `mn7_cn` 對每一條都回：

```
E (1880) MN_COMMAND: invalid command, please check format, 一號爆裂勝.
```

同樣內容改成**無聲調拼音**就全部接受。esp-sr 隨附的預設命令表
`model/multinet_model/fst/commands_cn.txt` 也確實是拼音格式（`da kai kong tiao`）。

當初不確定是哪一種，所以第一版把漢字與拼音**兩種形式都註冊**到同一個
command_id，讓開機 log 告訴我們答案。答案出來後就把漢字那半移除了。

簡體漢字未測試 —— 拼音已驗證可用，沒有理由再冒險。

`src/voice/voice.cpp` 的 `CmdSpec` 因此有兩個字串欄位：
`label` 給人看（繁體中文，會顯示在畫面與 log），`phrase` 給模型（拼音）。
兩者不可混用。

---

## 模型從哪來

Arduino core 預建的 `srmodels.bin` **只有英文模型**（`wn9_hiesp` + `mn5q8_en`），
所以必須自己打包。

`assets/srmodels.bin`（2.83MB）內含：

| 模型 | 用途 |
| --- | --- |
| `wn9_nihaoxiaozhi` | 「你好小智」喚醒詞 |
| `mn7_cn` | 中文命令詞辨識（MultiNet7） |
| `fst` | MultiNet6/7 必需的字音轉換資料 |

重新產生（換喚醒詞或命令詞模型時才需要）：

```bash
bash tools/gen_sr_model.sh
```

腳本會 sparse-checkout esp-sr 的 `model/` 目錄（約 62MB），組裝後用官方的
`pack_model.py` 打包。要換模型就改腳本開頭的 `WAKENET` / `MULTINET` 變數。

**選模型的限制**：Arduino 預建的 `libmultinet.a` 只支援 multinet 2/4/5q8/6/7
（用 `nm` 查 `esp_sr_multinet*` 符號可確認）。選了不支援的世代會載入失敗。

### 燒錄

`tools/pio_extra_sr_model.py` 會在 `pio run -t upload` 時自動把模型一併燒進去，
位移**從 `partitions.csv` 解析**而不是寫死 —— 分割表已經改過一次，
把偏移量抄兩份遲早會漂移，然後燒出一個開得起來但語音不會動的韌體。

> 這個 extra script 必須是 `pre:` 而不是 `post:`。`post:` 執行時 esptool 的
> 參數已經組好了，`FLASH_EXTRA_IMAGES` 不會生效 —— 燒錄看起來成功，
> 但模型分割區是空的。

---

## 分割表

語音模型需要一塊自己的分割區。官方的 `esp_sr_16.csv` 只給 2.9MB，
而我們的模型包是 2.83MB，只剩 43KB 餘裕，因此自訂：

```
nvs        0x009000   20 KB    設定與歷史（位移與前一版相同，改分割表不會弄丟設定）
otadata    0x00E000    8 KB
app0       0x010000    4 MB    韌體（目前用 1.26MB）
model      0x410000    4 MB    srmodels.bin（目前用 2.83MB）
littlefs   0x810000  7.94 MB   Phase 5 音效素材
```

`esp_srmodel_init("model")` 是用**標籤**找分割區，`model` 這個名字不可改。

---

## 執行緒模型

esp-sr 跑在自己的 FreeRTOS 任務（feed 在 core 0、detect 在 core 1），
而 **LVGL 不是執行緒安全的**。

因此辨識結果不直接操作 UI，而是丟進 FreeRTOS 佇列，由 `loop()` 的
`voicePoll()` 取出後在 LVGL 執行緒上派送。佇列滿了就丟棄新命令 ——
語音命令過期沒有意義，積壓一堆舊命令一次套用比漏掉一次更糟。

派送邏輯（`src/ui/voice_commands.cpp`）一律用**比賽狀態**守門，
而不是「現在在哪一頁」。語音可能在任何時候被觸發，用狀態判斷才不會出現
「比賽還沒開始就加分」。

---

## 資源佔用

| 項目 | 啟用語音前 | 啟用語音後 |
| --- | --- | --- |
| 內部 heap | 217 KB | 121 KB |
| PSRAM | 8.38 MB | 4.55 MB |
| Flash（韌體） | 872 KB | 1.26 MB |
| Flash（模型） | — | 2.83 MB |

AFE 的緩衝區吃掉約 3.8MB PSRAM。剩下的空間仍足夠 LVGL 與未來的音訊播放。

---

## 尚未驗證

**語音辨識本身還沒有人對著板子講過話。** 已驗證的是：模型正確載入
（開機 log 顯示 `wakenet9_v1h12_你好小智` 與 `mn7_cn`）、11 個命令全部
註冊成功、麥克風有實際訊號、系統開機無 crash。

實際辨識率、誤觸發率、以及對戰現場噪音下的表現都需要實測。

測試方式：

```bash
.venv/bin/pio device monitor
```

說「你好小智」，序列埠應出現：

```
[voice] 偵測到喚醒詞
[voice] 已喚醒，請說指令
[voice] 辨識到命令 1：一號爆裂勝
[voice] 套用命令：一號爆裂勝
```

---

## 後續

1. **畫面上的語音狀態指示** —— 目前只有序列埠看得到「聆聽中／請說指令」。
   使用者不知道喚醒詞有沒有被聽到。應該在計分畫面加一個小指示燈。
2. **TTS 報比分** —— esp-sr 內含 `esp_tts_chinese`，可以在得分後用中文播報
   「一號三分」。硬體（ES8311）已確認存在。
3. **誤觸發調校** —— 若實測誤觸發太多，可調高 wakenet 的觸發門檻，
   或把「重新開始」這類高風險命令從語音移除。
4. **對戰噪音** —— 陀螺對戰本身很吵。若辨識率不佳，考慮改用推播式
   （按住畫面才聽命令）取代喚醒詞。
