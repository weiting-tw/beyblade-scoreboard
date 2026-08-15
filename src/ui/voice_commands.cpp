// 語音命令 -> 計分動作的派送。
//
// 一律在 LVGL 執行緒上執行（由 main.cpp 的 loop() 透過 voicePoll() 取出），
// 因此可以安全地切換畫面。
//
// 每個命令都用**比賽狀態**守門，而不是靠「現在在哪一頁」。理由是語音可能在
// 任何時候被觸發，用狀態判斷才不會出現「比賽還沒開始就加分」這種情況。
#include <Arduino.h>

#include "../app/app.h"
#include "../voice/voice.h"
#include "ui.h"
#include "ui_theme.h"

namespace bey {
namespace ui {
namespace {

// 比賽進行中才能計分。
bool scoringAllowed() { return g_match.started() && !g_match.finished(); }

void doVoiceReset() {
    g_match.reset();
    g_match.start();
    showScore();
}

void doVoiceEnd() {
    g_match.forceFinish();
    recordFinishedMatch();
    showComplete();
}

}  // namespace

void handleVoiceCommand(VoiceCmd cmd) {
    Serial.printf("[voice] 套用命令：%s\n", voiceCmdLabel(cmd));

    switch (cmd) {
        // --- 計分 ---
        case VoiceCmd::P1Normal:
        case VoiceCmd::P1Burst:
        case VoiceCmd::P1Xtreme:
        case VoiceCmd::P2Normal:
        case VoiceCmd::P2Burst:
        case VoiceCmd::P2Xtreme: {
            if (!scoringAllowed()) {
                Serial.println("[voice] 略過：比賽尚未開始或已結束");
                return;
            }
            // VoiceCmd 的前六項與 ResultType 的前六項刻意同序，可直接轉換。
            static_assert(static_cast<int>(VoiceCmd::P1Normal) ==
                              static_cast<int>(ResultType::P1Normal),
                          "VoiceCmd 與 ResultType 的順序必須一致");
            static_assert(static_cast<int>(VoiceCmd::P2Xtreme) ==
                              static_cast<int>(ResultType::P2Xtreme),
                          "VoiceCmd 與 ResultType 的順序必須一致");
            applyResultAndAdvance(static_cast<ResultType>(cmd));
            return;
        }

        // --- 修正 ---
        case VoiceCmd::Undo:
            if (g_match.undo()) {
                showScore();
            } else {
                Serial.println("[voice] 略過：沒有可撤銷的計分");
            }
            return;

        case VoiceCmd::Reset:
            // 語音誤觸發的代價在這裡最大（整場歸零），因此與觸控一樣走二次確認。
            // 確認本身必須用手點，語音不能自己確認自己。
            if (g_match.started()) {
                showConfirmOverlay("重設比賽？", doVoiceReset);
            }
            return;

        // --- 流程 ---
        case VoiceCmd::Start:
            if (!g_match.started()) {
                showCountdown();
            } else {
                Serial.println("[voice] 略過：比賽已在進行中");
            }
            return;

        case VoiceCmd::NextRound:
            if (scoringAllowed()) {
                showScore(Nav::Forward);
            }
            return;

        case VoiceCmd::EndMatch:
            if (g_match.started() && !g_match.finished()) {
                showConfirmOverlay("結束比賽？", doVoiceEnd);
            }
            return;

        default:
            return;
    }
}

}  // namespace ui
}  // namespace bey
