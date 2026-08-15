#include "audio_bus.h"

#include <Arduino.h>

#include "es8311.h"

namespace bey {
namespace {

// 腳位取自官方範例 03_audio_out_no_tf。
constexpr gpio_num_t kI2sMclk = GPIO_NUM_2;
constexpr gpio_num_t kI2sBclk = GPIO_NUM_48;
constexpr gpio_num_t kI2sLrck = GPIO_NUM_38;
constexpr gpio_num_t kI2sDout = GPIO_NUM_47;
constexpr gpio_num_t kSpeakerEnable = GPIO_NUM_9;

constexpr uint32_t kMclkMultiple = 256;

I2SClass s_i2s;
es8311_handle_t s_es8311 = nullptr;  // 留著才能事後調音量
bool s_ready = false;
bool s_hasSpeaker = false;

bool initSpeakerCodec() {
    es8311_handle_t h = es8311_create(I2C_NUM_0, ES8311_ADDRESS_0);
    if (h == nullptr) {
        Serial.println("[audio] ES8311 建立失敗，沒有音效輸出");
        return false;
    }
    const es8311_clock_config_t clk = {
        .mclk_inverted = false,
        .sclk_inverted = false,
        .mclk_from_mclk_pin = true,
        .mclk_frequency = kAudioSampleRate * kMclkMultiple,
        .sample_frequency = kAudioSampleRate,
    };
    if (es8311_init(h, &clk, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16) != ESP_OK) {
        Serial.println("[audio] ES8311 初始化失敗");
        return false;
    }
    // 麥克風走 ES7210，ES8311 自己的麥克風輸入不啟用。
    es8311_microphone_config(h, false);
    s_es8311 = h;  // 音量之後由 audioBusSetVolume() 設定
    return true;
}

}  // namespace

bool audioBusBegin() {
    if (s_ready) {
        return true;
    }

    pinMode(kSpeakerEnable, OUTPUT);
    digitalWrite(kSpeakerEnable, LOW);  // 預設關閉，播放時才開

    s_hasSpeaker = initSpeakerCodec();
    if (!s_hasSpeaker) {
        Serial.println("[audio] codec 不可用，音訊功能停用");
        return false;
    }

    // DIN 給 -1：只建立 TX 通道。麥克風（ES7210）目前沒有任何使用者。
    s_i2s.setPins(kI2sBclk, kI2sLrck, kI2sDout, -1, kI2sMclk);
    s_i2s.setTimeout(1000);
    // STEREO：ES8311 是立體聲介面，單聲道片段在 feedback.cpp 複製到左右兩聲道。
    if (!s_i2s.begin(I2S_MODE_STD, kAudioSampleRate, I2S_DATA_BIT_WIDTH_16BIT,
                     I2S_SLOT_MODE_STEREO)) {
        Serial.println("[audio] I2S 初始化失敗");
        return false;
    }

    s_ready = true;
    Serial.printf("[audio] 匯流排就緒 %uHz\n", (unsigned)kAudioSampleRate);
    return true;
}

bool audioBusReady() { return s_ready; }
bool audioBusHasSpeaker() { return s_ready && s_hasSpeaker; }

I2SClass& audioBusI2S() { return s_i2s; }

void audioBusSpeakerEnable(bool on) {
    digitalWrite(kSpeakerEnable, on ? HIGH : LOW);
}

void audioBusSetVolume(uint8_t volume) {
    if (s_es8311 == nullptr) {
        return;
    }
    if (volume > 100) {
        volume = 100;
    }
    // 直接把 100 餵給 codec 會破音：語音片段在產生階段已經正規化到 85% 滿
    // 刻度，再用 0dB 推這顆小喇叭就超過它的線性範圍了。UI 的 0..100 映射到
    // codec 的 0..kCodecMax，使用者仍有完整刻度，只是頂端不再是失真區。
    //
    // 80 是保守起點，實際的破音臨界沒有量過 —— 覺得不夠大聲就往上調，
    // 開始糊掉就往下。
    constexpr uint8_t kCodecMax = 80;
    es8311_voice_volume_set(s_es8311,
                            static_cast<int>(volume) * kCodecMax / 100, nullptr);
}

}  // namespace bey
