// 把目前畫面經序列埠傳回開發機。
//
// 存在的理由：這台板子在遠端，改完版面沒有人能看螢幕。所有 maxR 計算都只能
// 靠算術驗收，而算術驗收不出「顏色對不對」「元件有沒有互相遮住」這類問題。
//
// 從 flush callback 擷取而不是用 lv_snapshot_take：flush 拿到的是真正送進
// LCD 的內容，包含 lv_layer_top 上的狀態晶片與所有 overlay；lv_snapshot_take
// 只能對單一物件，抓不到跨圖層的疊加結果。
//
// 用法（板子端由 loop() 偵測序列埠收到 's' 觸發）：
//   .venv/bin/python tools/screenshot.py -o shot.png
#pragma once

#include <lvgl.h>

namespace bey {

// 擷取一張並輸出。必須在 LVGL 執行緒上呼叫。
// 期間會阻塞數秒（序列埠傳輸），只用於除錯。
void screenshotCapture();

// 由 LVGL 的 flush callback 呼叫。非擷取中時是一個 bool 判斷就返回。
void screenshotOnFlush(const lv_area_t* area, const lv_color_t* px);

}  // namespace bey
