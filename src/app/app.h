// 應用層全域狀態與生命週期。
#pragma once

#include "match_core.h"
#include "settings_store.h"

namespace bey {

// 目前這場比賽。UI 各畫面共用同一個實例。
extern Match g_match;

// 依 g_store 的設定重新套用到 g_match（改完設定後呼叫）。
void applySettingsToMatch();

// 把已結束的比賽寫入歷史（受 saveHistory 設定控制）。
void recordFinishedMatch();

// 讀 RTC 取得 epoch 秒；讀不到回傳 0。
uint32_t nowEpoch();

// 本局計時。Spin Finish 比的就是誰轉得久，把它變成看得到的數字。
//
// 用 millis() 而不是 RTC：要的是經過時間，秒級的牆鐘時間解析度不夠，
// 而且 RTC 讀一次要走 I2C。
void markRoundStart();
uint32_t roundElapsedMs();

}  // namespace bey
