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

// 順序必須與 feedback.cpp 的 kSfx 陣列一致（有 static_assert 把關）。
enum class Sfx : uint8_t {
    Tick,     // 通用提示音（例如設定頁調音量時的試聽）
    Count3,   // 倒數「Three」
    Count2,   // 「Two」
    Count1,   // 「One」
    Go,       // 「Go Shoot!」
    ScoreNormal,
    ScoreBurst,
    ScoreXtreme,
    Undo,
    RoundEnd,
    MatchWin,
    Count,    // 哨兵，不是音效
};

// 語音播報片段。與 Sfx 分開：Sfx 是短提示音，這些是拼句子用的語音。
enum class Voice : uint8_t {
    None,  // 哨兵：第二段留空時用
    PlayerOne,
    PlayerTwo,
    NormalFinish,
    BurstFinish,
    XtremeFinish,
    Wins,
};

void feedbackInit();

// 播放音效。enableSound 為 false 時直接略過。
void feedbackPlay(Sfx sfx);

// 播報一句，例如「Player One」+「Burst Finish」。
//
// 兩段放在同一個佇列項目而不是連送兩次：佇列只有三格，滿的時候後送的會被
// 丟棄，分兩次送就可能只播出半句。enableAnnounce 為 false 時直接略過。
void feedbackAnnounce(Voice first, Voice second = Voice::None);

// 正在播報。語音辨識應該在這段期間丟棄結果 —— 功放正放著英文，
// 而 AEC 是關的，麥克風聽得見自己。
bool feedbackIsSpeaking();

// 震動回饋。enableVibration 為 false 時直接略過。
void feedbackHaptic(uint16_t ms);

}  // namespace bey
