#include "feedback.h"

#include <Arduino.h>

#include <cmath>

#include "../audio/audio_bus.h"
#include "settings_store.h"

namespace bey {
namespace {

// --- 音色設計 ---------------------------------------------------------
//
// 全部即時合成，不用音檔：16kHz 下一段 200ms 的 WAV 要 6.4KB，八個音效就要
// 佔 littlefs 還得處理檔案系統；合成只要幾行程式，延遲也最低。
//
// 每個音效是一串「頻率 + 長度」。頻率 0 代表靜音（音之間的間隔）。
struct Tone {
    uint16_t freq;  // Hz，0 = 靜音
    uint16_t ms;
};

constexpr int kMaxTones = 5;

struct SfxDef {
    Tone tones[kMaxTones];
};

// 音高走向本身就在傳達語意：得分往上、撤銷往下、分數越高音階越長。
constexpr SfxDef kSfx[] = {
    /* Tick        */ {{{880, 60}}},
    /* Go          */ {{{660, 70}, {990, 70}, {1320, 160}}},
    /* ScoreNormal */ {{{988, 90}}},
    /* ScoreBurst  */ {{{988, 80}, {1319, 130}}},
    /* ScoreXtreme */ {{{988, 70}, {1319, 70}, {1568, 70}, {2093, 180}}},
    /* Undo        */ {{{660, 70}, {440, 110}}},
    /* RoundEnd    */ {{{1175, 60}, {0, 40}, {1175, 60}}},
    /* MatchWin    */ {{{1047, 110}, {1319, 110}, {1568, 110}, {2093, 300}}},
};

constexpr int kSfxCount = sizeof(kSfx) / sizeof(kSfx[0]);

// 振幅。留很多餘裕：這顆小喇叭推太大會破音，而且音效只是提示不是主角。
constexpr float kAmplitude = 0.28f;

QueueHandle_t s_queue = nullptr;
TaskHandle_t s_task = nullptr;

// 一次合成 256 frame（16ms）。stereo 16bit -> 1KB，放 static 不吃堆疊。
constexpr int kChunkFrames = 256;
int16_t s_chunk[kChunkFrames * 2];

// 播放單一音。phase 跨 chunk 累積，避免每個 chunk 從 0 開始造成爆音。
void playTone(const Tone& t) {
    const int total = static_cast<int>(kAudioSampleRate) * t.ms / 1000;
    if (total <= 0) {
        return;
    }

    // 頭尾各做一段淡入淡出。方波／正弦突然起停會有明顯的「喀」聲，
    // 在小喇叭上比音效本身還大聲。
    const int fade = (total < 160) ? total / 4 : 40;

    float phase = 0.0f;
    const float step = 2.0f * static_cast<float>(M_PI) * t.freq / kAudioSampleRate;

    int done = 0;
    while (done < total) {
        const int n = (total - done < kChunkFrames) ? (total - done) : kChunkFrames;
        for (int i = 0; i < n; ++i) {
            float env = 1.0f;
            const int pos = done + i;
            if (fade > 0) {
                if (pos < fade) {
                    env = static_cast<float>(pos) / fade;
                } else if (pos > total - fade) {
                    env = static_cast<float>(total - pos) / fade;
                }
            }
            int16_t v = 0;
            if (t.freq > 0) {
                v = static_cast<int16_t>(32767.0f * kAmplitude * env * sinf(phase));
                phase += step;
                if (phase > 2.0f * static_cast<float>(M_PI)) {
                    phase -= 2.0f * static_cast<float>(M_PI);
                }
            }
            s_chunk[i * 2] = v;
            s_chunk[i * 2 + 1] = v;
        }
        audioBusI2S().write(reinterpret_cast<uint8_t*>(s_chunk),
                            static_cast<size_t>(n) * 2 * sizeof(int16_t));
        done += n;
    }
}

void audioTask(void*) {
    Sfx sfx;
    for (;;) {
        if (xQueueReceive(s_queue, &sfx, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        const int idx = static_cast<int>(sfx);
        if (idx < 0 || idx >= kSfxCount) {
            continue;
        }

        audioBusSpeakerEnable(true);
        for (const Tone& t : kSfx[idx].tones) {
            if (t.ms == 0) {
                break;  // 這個音效的音已經放完
            }
            playTone(t);
        }
        // I2S 是有緩衝的，立刻關功放會把尾巴切掉。
        vTaskDelay(pdMS_TO_TICKS(40));
        audioBusSpeakerEnable(false);
    }
}

}  // namespace

void feedbackInit() {
    if (!audioBusHasSpeaker()) {
        Serial.println("[feedback] 沒有可用的喇叭，音效停用");
        return;
    }

    // 佇列很短：音效過期就沒有意義，堆積一串舊音效比漏掉一個更糟。
    s_queue = xQueueCreate(3, sizeof(Sfx));
    if (s_queue == nullptr) {
        Serial.println("[feedback] 建立音效佇列失敗");
        return;
    }

    // 獨立任務：合成與 I2S 寫入都會阻塞，放在 LVGL 執行緒會造成畫面卡頓。
    if (xTaskCreatePinnedToCore(audioTask, "sfx", 4096, nullptr, 2, &s_task, 0) !=
        pdPASS) {
        Serial.println("[feedback] 建立音效任務失敗");
        s_task = nullptr;
        return;
    }

    Serial.println("[feedback] 音效就緒");
}

void feedbackPlay(Sfx sfx) {
    if (!g_store.settings().match.enableSound) {
        return;
    }
    if (s_queue == nullptr) {
        return;
    }
    // 佇列滿了就丟棄，絕不阻塞呼叫端（多半是 LVGL 執行緒）。
    xQueueSend(s_queue, &sfx, 0);
}

void feedbackHaptic(uint16_t ms) {
    if (!g_store.settings().enableVibration) {
        return;
    }
    (void)ms;  // 板上沒有震動馬達，要外接才能實作
}

}  // namespace bey
