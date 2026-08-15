// 離線語音控制（喚醒詞「你好小智」+ 中文命令詞）
//
// 完全離線：不連網、不需伺服器。辨識在 ESP32-S3 上跑，延遲是毫秒級。
// 模型在 assets/srmodels.bin，燒進 partitions.csv 的 `model` 分割區。
//
// 執行緒模型：esp-sr 跑在自己的 FreeRTOS 任務裡，而 LVGL **不是**執行緒安全的。
// 因此辨識結果不直接操作 UI，而是丟進佇列，由主迴圈的 voicePoll() 取出後
// 在 LVGL 執行緒上處理。
#pragma once

#include <cstdint>

namespace bey {

// 語音命令。與 VoiceCmd 對應的中文說法見 voice.cpp 的 kCommands。
enum class VoiceCmd : uint8_t {
    P1Normal,   // 一號普通勝
    P1Burst,    // 一號爆裂勝
    P1Xtreme,   // 一號極限勝
    P2Normal,
    P2Burst,
    P2Xtreme,
    Undo,       // 取消上一分
    Reset,      // 重新開始
    Start,      // 開始比賽
    NextRound,  // 下一局
    EndMatch,   // 結束比賽
    Count,
};

enum class VoiceState : uint8_t {
    Disabled,   // 未啟用，或模型／麥克風初始化失敗
    Listening,  // 待機，等待喚醒詞
    Awake,      // 已喚醒，等待命令（數秒後逾時回到 Listening）
};

// 啟動語音辨識。回傳 false 代表模型載入或麥克風初始化失敗，
// 此時計分器其餘功能不受影響，只是沒有語音。
bool voiceBegin();

VoiceState voiceState();

// 給 UI 顯示的一行狀態文字（「聆聽中」「請說指令」「語音停用」）。
const char* voiceStatusText();

// 從 LVGL 執行緒（主迴圈）呼叫。有待處理命令時填入 out 並回傳 true。
bool voicePoll(VoiceCmd& out);

// 命令的中文顯示名稱。
const char* voiceCmdLabel(VoiceCmd cmd);

}  // namespace bey
