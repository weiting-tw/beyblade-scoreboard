// 共用的音訊匯流排：一條 I2S 同時餵語音辨識（RX）與音效播放（TX）。
//
// 為什麼要有這一層：ES7210（麥克風）與 ES8311（喇叭）掛在**同一組 I2S**
// 腳位上。語音模組與音效模組各自 begin() 一次會互相踩到，所以匯流排的
// 所有權集中在這裡，兩邊都只跟這裡拿。
//
// Arduino 的 I2SClass 在 DOUT 與 DIN 都設定時會建立全雙工通道
// （ESP_I2S.cpp: `if (_dout >= 0 && _din >= 0) i2s_new_channel(&cfg, &tx, &rx)`），
// 因此 read 與 write 走不同通道，可以同時進行。
//
// 取樣率固定 16kHz —— 那是 esp-sr 的硬性要求，音效必須遷就。
#pragma once

#include "ESP_I2S.h"

namespace bey {

constexpr uint32_t kAudioSampleRate = 16000;

// 初始化 I2C 上的兩顆 codec 與 I2S 匯流排。必須在 voiceBegin() 與
// feedbackInit() 之前呼叫，且 I2C_Init() 要更早。
bool audioBusBegin();

bool audioBusReady();
bool audioBusHasMic();      // ES7210 是否就緒（語音辨識的前提）
bool audioBusHasSpeaker();  // ES8311 是否就緒（音效的前提）

// 共用的 I2S 實例。未初始化時仍可安全取得，只是操作會失敗。
I2SClass& audioBusI2S();

// 喇叭功率放大器致能（GPIO9）。不播音時關掉可省電並避免底噪。
void audioBusSpeakerEnable(bool on);

}  // namespace bey
