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

}  // namespace bey
