// 共用的音訊匯流排：ES8311 codec + 一條 I2S（只用 TX）。
//
// 為什麼要有這一層：I2S 與 codec 的所有權集中在這裡，播放端（feedback.cpp）
// 只跟這裡拿實例，不自己 begin()。
//
// 取樣率 22.05kHz。語音播報的齒擦音（Xtreme / Finish 的 sh、th）落在 4–8kHz，
// 16kHz 只有 8kHz 頻寬會把它們削掉；22.05k 給到 11kHz 頻寬就夠，
// 而且是 piper TTS 的原生輸出取樣率，產生片段時不需要重新取樣。
// 44.1k 只會讓片段體積翻倍，聽感上換不到東西。
#pragma once

#include "ESP_I2S.h"

namespace bey {

constexpr uint32_t kAudioSampleRate = 22050;

// 初始化 ES8311 與 I2S 匯流排。必須在 feedbackInit() 之前呼叫，
// 且 I2C_Init() 要更早。
bool audioBusBegin();

bool audioBusReady();
bool audioBusHasSpeaker();  // ES8311 是否就緒（音效的前提）

// 共用的 I2S 實例。未初始化時仍可安全取得，只是操作會失敗。
I2SClass& audioBusI2S();

// 喇叭功率放大器致能（GPIO9）。不播音時關掉可省電並避免底噪。
void audioBusSpeakerEnable(bool on);

// 喇叭音量 0..100，由 ES8311 codec 的類比增益實現。
// 在 codec 端調比在軟體端乘倍數好 —— 軟體乘會把量化雜訊一起放大。
void audioBusSetVolume(uint8_t volume);

}  // namespace bey
