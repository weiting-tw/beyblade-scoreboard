// 音訊硬體診斷韌體（不是計分器的一部分）
//
//     .venv/bin/pio run -e audio_probe -t upload && .venv/bin/pio device monitor
//
// 目的：在投入語音控制開發前，先確認這片板子的音訊硬體實際狀況。
// 從軟體無法得知麥克風膠囊與喇叭是否真的焊上去了，只能實測。
//
// 會回答三個問題：
//   1. ES8311（喇叭 codec）與 ES7210（麥克風 ADC）有沒有在 I2C 上回應？
//      —— 這決定「codec 晶片是否存在」，是最關鍵的一題。
//   2. 麥克風有沒有訊號？—— 印出即時音量條，對著板子講話會跳動。
//   3. 喇叭有沒有聲音？—— 每 4 秒播一次 1kHz 嗶聲，這題需要耳朵。
//
// 腳位全部取自官方範例 03_audio_out_no_tf 與 06_esp_sr。
#include <Arduino.h>
#include <Wire.h>

#include "ESP_I2S.h"
#include "es7210.h"
#include "es8311.h"

namespace {

constexpr gpio_num_t kI2cSda = GPIO_NUM_11;
constexpr gpio_num_t kI2cScl = GPIO_NUM_10;

constexpr gpio_num_t kI2sMclk = GPIO_NUM_2;
constexpr gpio_num_t kI2sBclk = GPIO_NUM_48;
constexpr gpio_num_t kI2sLrck = GPIO_NUM_38;
constexpr gpio_num_t kI2sDout = GPIO_NUM_47;
constexpr gpio_num_t kI2sDin = GPIO_NUM_39;

constexpr gpio_num_t kSpeakerEnable = GPIO_NUM_9;

constexpr uint32_t kSampleRate = 16000;
constexpr uint32_t kMclkMultiple = 256;

constexpr uint8_t kAddrEs8311 = 0x18;
constexpr uint8_t kAddrEs7210 = 0x40;

I2SClass i2s;

bool g_es8311Ok = false;
bool g_es7210Ok = false;

// --- 1. I2C 掃描 --------------------------------------------------------

const char* identify(uint8_t addr) {
    switch (addr) {
        case 0x15: return "CST816S 觸控";
        case 0x18: return "ES8311 喇叭 codec   <-- 語音輸出";
        case 0x40: return "ES7210 麥克風 ADC   <-- 語音輸入";
        case 0x51: return "PCF85063 RTC";
        case 0x55: return "BQ27220 電量計";
        case 0x6B: return "QMI8658 IMU";
        default: return "未知";
    }
}

void scanI2c() {
    Serial.println("\n===== I2C 掃描 (SDA=11, SCL=10) =====");
    int found = 0;
    for (uint8_t addr = 1; addr < 127; ++addr) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.printf("  0x%02X  %s\n", addr, identify(addr));
            ++found;
            if (addr == kAddrEs8311) {
                g_es8311Ok = true;
            }
            if (addr == kAddrEs7210) {
                g_es7210Ok = true;
            }
        }
    }
    Serial.printf("  共 %d 個裝置\n", found);

    Serial.println("\n----- 音訊硬體判定 -----");
    Serial.printf("  ES8311 (喇叭輸出) : %s\n",
                  g_es8311Ok ? "存在" : "*** 無回應 ***");
    Serial.printf("  ES7210 (麥克風)   : %s\n",
                  g_es7210Ok ? "存在" : "*** 無回應 ***");
    if (!g_es8311Ok && !g_es7210Ok) {
        Serial.println("  兩顆 codec 都沒回應 —— 這片板子可能沒有音訊硬體，");
        Serial.println("  離線語音控制需要外接 I2S 麥克風與功放。");
    }
    Serial.println();
}

// --- 2. codec 初始化 ----------------------------------------------------

bool initEs8311() {
    es8311_handle_t h = es8311_create(I2C_NUM_0, ES8311_ADDRRES_0);
    if (h == nullptr) {
        Serial.println("[ES8311] es8311_create 失敗");
        return false;
    }
    const es8311_clock_config_t clk = {
        .mclk_inverted = false,
        .sclk_inverted = false,
        .mclk_from_mclk_pin = true,
        .mclk_frequency = kSampleRate * kMclkMultiple,
        .sample_frequency = kSampleRate,
    };
    if (es8311_init(h, &clk, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16) != ESP_OK) {
        Serial.println("[ES8311] es8311_init 失敗");
        return false;
    }
    es8311_voice_volume_set(h, 70, nullptr);
    es8311_microphone_config(h, false);  // 麥克風走 ES7210，不用 ES8311 的
    Serial.println("[ES8311] 初始化完成，音量 70");
    return true;
}

bool initEs7210() {
    es7210_dev_handle_t h = nullptr;
    const es7210_i2c_config_t i2cCfg = {
        .i2c_port = I2C_NUM_0,
        .i2c_addr = ES7210_ADDRRES_00,
    };
    if (es7210_new_codec(&i2cCfg, &h) != ESP_OK || h == nullptr) {
        Serial.println("[ES7210] es7210_new_codec 失敗");
        return false;
    }
    // 指定初始化的順序必須與 es7210.h 的宣告順序一致，否則 C++ 編譯失敗。
    const es7210_codec_config_t cfg = {
        .sample_rate_hz = kSampleRate,
        .mclk_ratio = kMclkMultiple,
        .i2s_format = ES7210_I2S_FMT_I2S,
        .bit_width = ES7210_I2S_BITS_16B,
        .mic_bias = ES7210_MIC_BIAS_2V87,
        .mic_gain = ES7210_MIC_GAIN_30DB,
        .flags = {.tdm_enable = false},
    };
    if (es7210_config_codec(h, &cfg) != ESP_OK) {
        Serial.println("[ES7210] es7210_config_codec 失敗");
        return false;
    }
    // 數位音量的有效範圍是 -95..32 dB（見 es7210.cpp），給 0 表示不額外增減；
    // 真正的靈敏度由上面的 mic_gain 30dB 類比增益決定。
    es7210_config_volume(h, 0);
    Serial.println("[ES7210] 初始化完成，類比增益 30dB / 數位 0dB");
    return true;
}

// --- 3. 音量條 ----------------------------------------------------------

void printLevel(int peak) {
    // 16bit 滿刻度 32767。用對數尺度才看得出小訊號。
    const int bars = (peak <= 0) ? 0 : static_cast<int>(20.0f * log10f(peak / 32767.0f) / 3.0f) + 20;
    const int n = bars < 0 ? 0 : (bars > 20 ? 20 : bars);

    char bar[24];
    for (int i = 0; i < 20; ++i) {
        bar[i] = (i < n) ? '#' : '.';
    }
    bar[20] = '\0';
    Serial.printf("  麥克風 [%s] peak=%5d\n", bar, peak);
}

// 1kHz 正弦波，播 250ms。
void playBeep() {
    constexpr int kMs = 250;
    constexpr int kSamples = kSampleRate * kMs / 1000;
    static int16_t buf[kSamples];
    for (int i = 0; i < kSamples; ++i) {
        const float t = static_cast<float>(i) / kSampleRate;
        buf[i] = static_cast<int16_t>(12000.0f * sinf(2.0f * PI * 1000.0f * t));
    }
    digitalWrite(kSpeakerEnable, HIGH);
    i2s.write(reinterpret_cast<uint8_t*>(buf), sizeof(buf));
    Serial.println("  >>> 已送出 1kHz 嗶聲（250ms）—— 有聽到嗎？");
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(2000);  // 等 USB CDC 連上，否則前幾行會掉

    Serial.println("\n\n########## 音訊硬體診斷 ##########");

    Wire.begin(kI2cSda, kI2cScl, 100000);
    scanI2c();

    if (g_es8311Ok) {
        initEs8311();
    }
    if (g_es7210Ok) {
        initEs7210();
    }

    pinMode(kSpeakerEnable, OUTPUT);
    digitalWrite(kSpeakerEnable, LOW);

    i2s.setPins(kI2sBclk, kI2sLrck, kI2sDout, kI2sDin, kI2sMclk);
    if (!i2s.begin(I2S_MODE_STD, kSampleRate, I2S_DATA_BIT_WIDTH_16BIT,
                   I2S_SLOT_MODE_MONO, I2S_STD_SLOT_LEFT)) {
        Serial.println("[I2S] 匯流排初始化失敗");
    } else {
        Serial.println("[I2S] 匯流排就緒 16kHz/16bit/mono");
    }

    Serial.println("\n對著板子講話或拍手，下面的音量條應該會跳動。");
    Serial.println("每 4 秒會播一次嗶聲。\n");
}

void loop() {
    static uint32_t lastBeep = 0;

    // 讀一小段麥克風資料算峰值
    constexpr int kFrame = 512;
    static int16_t rx[kFrame];
    const size_t got = i2s.readBytes(reinterpret_cast<char*>(rx), sizeof(rx));

    int peak = 0;
    const int n = static_cast<int>(got / sizeof(int16_t));
    for (int i = 0; i < n; ++i) {
        const int v = rx[i] < 0 ? -rx[i] : rx[i];
        if (v > peak) {
            peak = v;
        }
    }
    printLevel(peak);

    if (millis() - lastBeep > 4000) {
        lastBeep = millis();
        playBeep();
        digitalWrite(kSpeakerEnable, LOW);
    }

    delay(200);
}
