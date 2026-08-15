# 實作決策與規格偏離

`docs/SPEC.md` 是需求真實來源。本檔記錄實作時偏離規格的地方、原因，以及改回去的做法。

---

## D1. UI 文字使用英文，非規格中的中文按鈕

**規格**：第 6 節按鈕寫作 `[開始比賽]`、`[撤銷]`、`[重設]` 等中文。

**實作**：`START`、`UNDO`、`RESET` 等英文。

**原因**：LVGL 內建的 Montserrat 字體只含 Latin-1，沒有任何中日韓字元。直接餵中文字串會渲染成空白或豆腐方塊。規格自己的標題（`BEYBLADE X`、`BATTLE SCORE`、`ROUND 2`、`MATCH WINNER`）本來就是英文，全英文介面在視覺上也一致。

**要改成中文的做法**：

1. 準備一個含所需字元的 TTF（例如思源黑體 Noto Sans TC）
2. 用 [lv_font_conv](https://github.com/lvgl/lv_font_conv) 產生**子集**字型，只包含實際用到的數十個漢字：
   ```
   lv_font_conv --font NotoSansTC-Bold.ttf --size 24 --bpp 4 \
     --format lvgl -o src/ui/font_tc_24.c \
     --symbols "開始比賽快速對戰設定歷史紀錄撤銷重局勝者返回下一結束..."
   ```
3. 把產出的 `.c` 放進 `src/ui/`，在 `ui_theme.h` 宣告 `LV_FONT_DECLARE(font_tc_24)`
4. 把各畫面的 `&lv_font_montserrat_20` 換成 `&font_tc_24`

**成本**：全字集約 1–2MB flash，子集約 30–80KB。app0 分割區有 4MB，子集完全放得下。

---

## D2. 玩家名稱用「選擇」而非「輸入」

**規格**：第 4 節「也可以讓使用者輸入或選擇」。

**實作**：`showNames()` 提供 12 組預設名稱的雙 roller 選擇，沒有鍵盤。

**原因**：360×360 圓形螢幕塞不下可用的 QWERTY 鍵盤——四個角會被圓周切掉，且每個鍵會遠小於規格第 9 節的 60×45 觸控目標下限。規格明確允許「選擇」。

**要改成自由輸入的做法**：`lv_keyboard` + `lv_textarea`，建議改用 3×4 的 T9 式輸入或字元轉盤，而非完整鍵盤。預設清單在 `src/ui/screen_names.cpp` 的 `kPresetOptions`。

---

## D3. 局結果頁的中間按鈕是 UNDO，不是「返回計分」

**規格**：第 6 節頁面六列出 `[下一局]` `[返回計分]` `[結束比賽]`。

**實作**：`UNDO` / `NEXT` / `END`。

**原因**：進入局結果頁時分數已經套用完畢，因此「下一局」與「返回計分」會執行完全相同的動作（回到計分畫面），並排兩顆等效按鈕沒有意義。剛按錯勝利方式時真正需要的出口是撤銷。

---

## D4. 比賽完成頁沒有「儲存結果」按鈕

**規格**：第 6 節頁面七列出 `[新比賽]` `[儲存結果]` `[返回首頁]`。

**實作**：`NEW` / `HOME`，加上一行 `SAVED` / `NOT SAVED` 狀態文字。

**原因**：比賽一結束就依設定頁的 `HISTORY` 開關自動寫入 NVS（`recordFinishedMatch()`）。再放一顆手動儲存按鈕會讓同一場被記錄兩次。開關在設定頁，狀態在完成頁可見。

---

## D5. 設定頁沒有「自動休眠時間」

**規格**：第 6 節頁面八列出「自動休眠時間」。

**實作**：`AppSettings::sleepSec` 欄位已保留並會存進 NVS，但設定頁不顯示這一列。

**原因**：自動休眠本身屬於規格第 12 節的 Phase 5，第一版沒有實作。顯示一個按了不會有任何效果的設定比不顯示更糟。實作休眠後把 `screen_settings.cpp` 的那一列加回來即可，儲存格式不必變。

---

## D6. 勝利點數的 P1／P2 強制對稱

**實作**：在設定頁調整 `NORMAL` / `BURST` / `XTREME` 時，`writeField()` 會同步寫入 P1 與 P2 的對應規則。

**原因**：`RuleSet` 的資料結構允許兩位玩家有不同點數（規格第 5 節的可擴充性保留了這個能力），但 UI 上讓 P1 的爆裂 +2、P2 的爆裂 +3 只會造成困惑。核心不阻止不對稱，只有設定頁強制對稱。要做讓分賽制時，直接寫 `RuleSet` 即可。

---

## D7. 設定頁的 ± 按鈕小於規格的 60×45

**規格**：第 9 節建議按鈕至少 60×45 px。

**實作**：設定頁的 ± 按鈕是 42×34。

**原因**：規格第 9 節的動機是「避免誤觸造成誤加分」。設定頁是低頻操作，按錯一下的代價是數字差一，當場看得到也改得回來。所有與比分相關的按鈕（`P1 WIN`、`P2 WIN`、`UNDO`、`RESET`、勝利類型選單）都維持規格尺寸。

---

## D8. 歷史紀錄存在 NVS，不是 LittleFS

**規格**：第 8 節「使用 ESP32 Preferences 或 LittleFS」。

**實作**：設定與歷史都走 Preferences（NVS）。`partitions.csv` 仍保留一個 12MB 的 `littlefs` 分割區。

**原因**：20 場紀錄 × 52 bytes ≈ 1KB，NVS 綽綽有餘，省掉掛載檔案系統的開機時間與失敗處理。littlefs 分割區保留給 Phase 5 的音效素材（WAV/MP3 才是真正需要檔案系統的東西）。

---

## D9. 歷史紀錄的 timestamp 目前恆為 0

**規格**：第 8 節的 JSON 範例含 `timestamp`。

**實作**：`MatchRecord::timestamp` 欄位存在，但 `nowEpoch()` 回傳 0。

**原因**：板上的 PCF85063 RTC（I²C 0x51）在第一版沒有接。欄位先留著，接上 RTC 後只要改 `src/app/app.cpp` 的 `nowEpoch()` 一個函式，其餘程式與儲存格式都不用動。

---

## 已知風險（尚未在硬體上驗證）

以下是讀官方程式碼時發現、但**沒有實機可以確認**的疑點。第一次燒錄時請特別留意：

### R1. `full_refresh = 1` 搭配 1/10 螢幕大小的緩衝區

官方 `LVGL_Driver.cpp` 設定 `disp_drv.full_refresh = 1`，但繪圖緩衝區只有 `LCD_WIDTH * LCD_HEIGHT / 10`。LVGL 8 的 `full_refresh` 通常要求緩衝區等於整個螢幕。這段是官方原樣保留的程式碼，若實機出現畫面撕裂或只更新局部，先試著把 `full_refresh` 改成 0。

### R2. 繪圖緩衝區配置在內部 SRAM

官方用 `static lv_color_t buf1[LVGL_BUF_LEN]`，兩塊各約 25KB，合計 51KB 佔用內部 SRAM（共 512KB）。板上有 8MB PSRAM，`LVGL_Driver.cpp` 裡也留著改用 `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)` 的註解版本。若內部記憶體吃緊再切換。

### R3. `LV_MEM_SIZE` 從官方的 48KB 調到 64KB

多開了 Montserrat 20/28/36/48 四種字體（字體本身在 flash，不佔 heap），但畫面元件變多。若開機即當機或 LVGL 報 out-of-memory，先確認這個值。畫面切換一律用 `lv_scr_load_anim(..., auto_del=true)`，舊畫面會被釋放。

### R4. 觸控座標方向未驗證

規格第 9 節要求確認螢幕旋轉方向與觸控座標對應。官方 `Lvgl_Touchpad_Read()` 直接把 CST816 的 x/y 餵給 LVGL，沒有做任何轉換。若實機上點左邊卻反應在右邊，校正方式見 `README.md`。
