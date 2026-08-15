// 音效、語音播報與震動（規格第 10 節）。
//
// 播放跑在自己的 FreeRTOS 任務上，I2S 與 codec 由 audio/audio_bus.* 擁有。
// 呼叫端一律非阻塞：這些函式只是把訊息丟進佇列，佇列滿了就丟棄。
//
// 震動仍是 no-op —— 板上沒有震動馬達，要外接才能實作。
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
    SpinFinish,
    OverFinish,
    BurstFinish,
    XtremeFinish,
    Wins,
};

void feedbackInit();

// 播放音效。enableSound 為 false 時直接略過。
void feedbackPlay(Sfx sfx);

// 播報一句，最多三段。例如得分時「Player One」+「Burst Finish」，
// 比賽結束時「Xtreme Finish」+「Player One」+「Wins」。
//
// 整句放同一個佇列項目而不是連送幾次：佇列只有三格，滿的時候後送的會被
// 丟棄，分次送就可能只播出半句。enableAnnounce 為 false 時直接略過。
void feedbackAnnounce(Voice first, Voice second = Voice::None,
                      Voice third = Voice::None);

// 立刻停止播放並清空佇列。
//
// 用在「新動作把舊聲音蓋掉」的時機，例如按下一局時上一局的勝利播報還沒
// 播完 —— 佇列是 FIFO，不清的話倒數的 Three 會排在播報後面，畫面已經在
// 數秒了聲音還落後一大截，兩者糊在一起。
void feedbackStop();

// 震動回饋。
//
// 板上沒有震動馬達，目前是 no-op —— 呼叫點（得分、撤銷）留著，外接馬達
// 之後只要填這個函式就會動。設定頁不顯示震動開關：一個撥了沒反應的開關
// 比缺一個功能更讓人困惑。AppSettings::enableVibration 欄位保留，
// 移掉它要升 kSettingsMagic，為了一個沒有 UI 的欄位不值得。
void feedbackHaptic(uint16_t ms);

}  // namespace bey
