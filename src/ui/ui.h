// 畫面切換入口（規格第 6 節的八個頁面）。
//
// 每個 show 函式都重新建立整個畫面，載入後舊畫面會被 LVGL 釋放。
// 因此各畫面之間不得共用 widget 指標，狀態一律放在 g_match / g_store。
#pragma once

#include "../voice/voice.h"  // VoiceCmd
#include "match_core.h"
#include "ui_theme.h"  // Nav

namespace bey {
namespace ui {

void init();  // 建立第一個畫面（首頁）

void showHome();      // 頁面一：首頁／待機
void showFormat();    // 頁面二：賽制選擇
void showReady();     // 頁面三：比賽準備
void showNames();     // 玩家名稱選擇（自頁面三進入）
void showCountdown();  // 頁面四：倒數 3-2-1-GO

// 頁面五：主計分。
// 預設 Replace —— 撤銷／重設會重畫同一頁，那時不該有轉場位移。
// 從倒數進場請傳 Nav::Fade，從局結果按「下一局」請傳 Nav::Forward。
void showScore(Nav nav = Nav::Replace);
void showRound();     // 頁面六：局結果
void showComplete();  // 頁面七：比賽完成
void showSettings();  // 頁面八：設定
void showHistory();   // 歷史紀錄

// 套用一次結果，並依比賽是否結束切到局結果或完成畫面。
void applyResultAndAdvance(ResultType r);

// 勝利類型選單。疊在目前畫面之上（lv_layer_top），不切換畫面。
void showWinTypeOverlay(bool forPlayer1);

// 二次確認對話框（重設用，規格第 9 節）。
void showConfirmOverlay(const char* message, void (*onConfirm)());

// 派送一個語音命令。必須在 LVGL 執行緒上呼叫（由 loop() 的 voicePoll 驅動）。
void handleVoiceCommand(VoiceCmd cmd);

}  // namespace ui
}  // namespace bey
