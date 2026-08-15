// 音效與震動的介面預留（規格第 10 節）。
//
// 第一版全部是 no-op：板上雖有 ES8311/ES7210 與喇叭致能腳 GPIO9，
// 但規格明訂「不得因音訊功能影響第一版計分器穩定性」，所以只固定介面，
// 不初始化 I2S。Phase 5 只需替換 feedback.cpp，呼叫端一行都不用改。
//
// Phase 5 音訊腳位（來自規格第 10 節，尚未驗證）：
//   Codec ES8311 / MIC ES7210 / SPK_EN GPIO9
//   I2S MCLK 2, BCLK 48, LRCK 38, DOUT 47, DIN 39
#pragma once

#include <cstdint>

namespace bey {

enum class Sfx : uint8_t {
    Tick,       // 倒數 3 / 2 / 1
    Go,         // GO!
    ScoreNormal,
    ScoreBurst,
    ScoreXtreme,
    Undo,
    RoundEnd,
    MatchWin,
};

void feedbackInit();

// 播放音效。enableSound 為 false 時直接略過。
void feedbackPlay(Sfx sfx);

// 震動回饋。enableVibration 為 false 時直接略過。
void feedbackHaptic(uint16_t ms);

}  // namespace bey
