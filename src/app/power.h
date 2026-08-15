// 電池狀態快取。
//
// I2C 匯流排與觸控（CST816S，每 30ms 輪詢一次）共用，所以電量絕對不能另開
// task 去讀 —— poll() 必須在 LVGL 執行緒上呼叫，由 UI 層的 lv_timer 驅動。
#pragma once

#include "../drivers/bq27220.h"

namespace bey {
namespace power {

// 開機時呼叫一次，探測電量計是否存在。
void begin();

// 電量計有沒有回應。false 時 UI 應該整個不顯示電量，而不是顯示 0%。
bool available();

// 讀一次並更新快取。必須在 LVGL 執行緒上呼叫。
void poll();

const BatteryState& state();

// 是否低電量。進出門檻不同（≤15% 進、>18% 出），避免在邊界反覆閃爍。
bool low();

}  // namespace power
}  // namespace bey
