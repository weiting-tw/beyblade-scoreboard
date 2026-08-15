#include "audio_bus.h"

#include <Arduino.h>

#include "es7210.h"
#include "es8311.h"

namespace bey {
namespace {

// 腳位取自官方範例 03_audio_out_no_tf 與 06_esp_sr。
constexpr gpio_num_t kI2sMclk = GPIO_NUM_2;
constexpr gpio_num_t kI2sBclk = GPIO_NUM_48;
constexpr gpio_num_t kI2sLrck = GPIO_NUM_38;
constexpr gpio_num_t kI2sDout = GPIO_NUM_47;
constexpr gpio_num_t kI2sDin = GPIO_NUM_39;
constexpr gpio_num_t kSpeakerEnable = GPIO_NUM_9;

constexpr uint32_t kMclkMultiple = 256;

I2SClass s_i2s;
bool s_ready = false;
bool s_hasMic = false;
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
    es8311_voice_volume_set(h, 75, nullptr);
    // 麥克風走 ES7210，ES8311 自己的麥克風輸入不啟用。
    es8311_microphone_config(h, false);
    return true;
}

bool initMicCodec() {
    es7210_dev_handle_t h = nullptr;
    const es7210_i2c_config_t i2cCfg = {
        .i2c_port = I2C_NUM_0,
        .i2c_addr = ES7210_ADDRRES_00,
    };
    if (es7210_new_codec(&i2cCfg, &h) != ESP_OK || h == nullptr) {
        Serial.println("[audio] ES7210 建立失敗，沒有語音輸入");
        return false;
    }
    // 欄位順序必須與 es7210.h 的宣告一致。
    const es7210_codec_config_t cfg = {
        .sample_rate_hz = kAudioSampleRate,
        .mclk_ratio = kMclkMultiple,
        .i2s_format = ES7210_I2S_FMT_I2S,
        .bit_width = ES7210_I2S_BITS_16B,
        .mic_bias = ES7210_MIC_BIAS_2V87,
        .mic_gain = ES7210_MIC_GAIN_30DB,
        .flags = {.tdm_enable = false},
    };
    if (es7210_config_codec(h, &cfg) != ESP_OK) {
        Serial.println("[audio] ES7210 設定失敗");
        return false;
    }
    es7210_config_volume(h, 0);  // 有效範圍 -95..32 dB
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
    s_hasMic = initMicCodec();

    if (!s_hasSpeaker && !s_hasMic) {
        Serial.println("[audio] 兩顆 codec 都不可用，音訊功能停用");
        return false;
    }

    s_i2s.setPins(kI2sBclk, kI2sLrck, kI2sDout, kI2sDin, kI2sMclk);
    s_i2s.setTimeout(1000);
    // STEREO：ES7210 是多通道 ADC，esp-sr 的 AFE 期待雙通道輸入。
    // DOUT 與 DIN 都有設，I2SClass 會建立全雙工通道，播放與辨識可同時進行。
    if (!s_i2s.begin(I2S_MODE_STD, kAudioSampleRate, I2S_DATA_BIT_WIDTH_16BIT,
                     I2S_SLOT_MODE_STEREO)) {
        Serial.println("[audio] I2S 初始化失敗");
        return false;
    }

    s_ready = true;
    Serial.printf("[audio] 匯流排就緒 %uHz，麥克風=%s 喇叭=%s\n",
                  (unsigned)kAudioSampleRate, s_hasMic ? "有" : "無",
                  s_hasSpeaker ? "有" : "無");
    return true;
}

bool audioBusReady() { return s_ready; }
bool audioBusHasMic() { return s_ready && s_hasMic; }
bool audioBusHasSpeaker() { return s_ready && s_hasSpeaker; }

I2SClass& audioBusI2S() { return s_i2s; }

void audioBusSpeakerEnable(bool on) {
    digitalWrite(kSpeakerEnable, on ? HIGH : LOW);
}

}  // namespace bey
