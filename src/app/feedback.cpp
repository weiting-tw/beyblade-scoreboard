#include "feedback.h"

#include <Arduino.h>

#include <cmath>

#include "../audio/audio_bus.h"
#include "../audio/clips/voice_clips.h"
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
// 有語音片段的項目，這裡的合成音只是片段缺席時的退路。
constexpr SfxDef kSfx[] = {
    /* Tick        */ {{{880, 60}}},
    /* Count3      */ {{{880, 60}}},
    /* Count2      */ {{{880, 60}}},
    /* Count1      */ {{{880, 60}}},
    /* Go          */ {{{660, 70}, {990, 70}, {1320, 160}}},
    /* ScoreNormal */ {{{988, 90}}},
    /* ScoreBurst  */ {{{988, 80}, {1319, 130}}},
    /* ScoreXtreme */ {{{988, 70}, {1319, 70}, {1568, 70}, {2093, 180}}},
    /* Undo        */ {{{660, 70}, {440, 110}}},
    /* RoundEnd    */ {{{1175, 60}, {0, 40}, {1175, 60}}},
    /* MatchWin    */ {{{1047, 110}, {1319, 110}, {1568, 110}, {2093, 300}}},
};

constexpr int kSfxCount = sizeof(kSfx) / sizeof(kSfx[0]);
static_assert(kSfxCount == static_cast<int>(Sfx::Count),
              "kSfx 的筆數與順序必須與 Sfx 列舉一致");

// 振幅。留很多餘裕：這顆小喇叭推太大會破音，而且音效只是提示不是主角。
constexpr float kAmplitude = 0.28f;

QueueHandle_t s_queue = nullptr;
TaskHandle_t s_task = nullptr;

// 一次處理 512 frame（32ms）。I2S 的 DMA 只有 6 x 240 = 1440 frame（90ms），
// 而且 auto_clear = true —— 餵不及時它會直接輸出靜音，聽起來就是斷音。
// 寫得大塊一點可以減少呼叫次數、拉開與 underrun 的距離。
// stereo 16bit -> 2KB，放 static 不吃堆疊。
constexpr int kChunkFrames = 512;
int16_t s_chunk[kChunkFrames * 2];

// --- 語音片段 ---------------------------------------------------------
//
// 有錄好的語音就用語音，沒有才退回合成音。片段由 tools/gen_voice_clips.py
// 在 Mac 上用 `say` 產生（板上的 esp-sr TTS 只有標頭檔沒有實作，
// 而且是中文專用，講不出 "GO SHOOT"）。
bool getClip(Sfx sfx, const int16_t** data, unsigned* len) {
    switch (sfx) {
        case Sfx::Count3:
            *data = clip_three;
            *len = clip_three_len;
            return true;
        case Sfx::Count2:
            *data = clip_two;
            *len = clip_two_len;
            return true;
        case Sfx::Count1:
            *data = clip_one;
            *len = clip_one_len;
            return true;
        case Sfx::Go:
            *data = clip_go;
            *len = clip_go_len;
            return true;
        default:
            return false;
    }
}

// 片段是單聲道，I2S 開在 STEREO，因此要複製到左右兩聲道。
void playPcm(const int16_t* mono, unsigned len) {
    unsigned done = 0;
    while (done < len) {
        const unsigned n =
            (len - done < kChunkFrames) ? (len - done) : kChunkFrames;
        for (unsigned i = 0; i < n; ++i) {
            const int16_t v = mono[done + i];
            s_chunk[i * 2] = v;
            s_chunk[i * 2 + 1] = v;
        }
        audioBusI2S().write(reinterpret_cast<uint8_t*>(s_chunk),
                            static_cast<size_t>(n) * 2 * sizeof(int16_t));
        done += n;
    }
}

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

// 功放開啟後的暖機時間。Class-D 功放有防爆音的軟啟動斜坡，
// 剛致能的頭幾十毫秒輸出是被壓低的。
constexpr uint32_t kAmpWarmupMs = 35;

// 閒置多久才關功放。倒數是 1 秒一拍，這個值必須大於一拍，
// 否則每個數字之間功放都會關掉再開，每段都要重新爬斜坡。
constexpr uint32_t kAmpIdleMs = 1500;

// 推一段靜音把 DMA 緩衝區裡的尾巴擠出去。
// i2s.write() 是「排進 DMA 就返回」，不是「已經放完」。
void flushDma() {
    constexpr int kSilenceFrames = kChunkFrames;
    static const int16_t silence[kChunkFrames * 2] = {0};
    for (int i = 0; i < 4; ++i) {
        audioBusI2S().write(reinterpret_cast<uint8_t*>(
                                const_cast<int16_t*>(silence)),
                            kSilenceFrames * 2 * sizeof(int16_t));
    }
}

void audioTask(void*) {
    Sfx sfx;
    bool ampOn = false;

    for (;;) {
        // 用逾時等待而不是無限等待：逾時代表一段時間沒有音效了，
        // 這時才把功放關掉。
        if (xQueueReceive(s_queue, &sfx, pdMS_TO_TICKS(kAmpIdleMs)) != pdTRUE) {
            if (ampOn) {
                flushDma();
                audioBusSpeakerEnable(false);
                ampOn = false;
            }
            continue;
        }

        const int idx = static_cast<int>(sfx);
        if (idx < 0 || idx >= kSfxCount) {
            continue;
        }

        // 功放只在「本來是關的」時候才開並暖機。整段倒數期間它會一直開著，
        // 所以 Three/Two/One/Go 的音量一致 —— 之前每段都開關一次，
        // 短的「Two」「One」整段幾乎都落在軟啟動斜坡裡，所以越後面越小聲。
        if (!ampOn) {
            audioBusSpeakerEnable(true);
            ampOn = true;
            vTaskDelay(pdMS_TO_TICKS(kAmpWarmupMs));
        }

        const int16_t* clip = nullptr;
        unsigned clipLen = 0;
        if (getClip(sfx, &clip, &clipLen)) {
            playPcm(clip, clipLen);
        } else {
            for (const Tone& t : kSfx[idx].tones) {
                if (t.ms == 0) {
                    break;  // 這個音效的音已經放完
                }
                playTone(t);
            }
        }
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
    //
    // 優先權 6 是刻意高於 esp-sr 的 feed/detect 任務（都是 5）。
    // 音效播放是硬即時：錯過 DMA 期限，auto_clear 會補靜音，立刻聽得出斷音；
    // 語音辨識晚幾毫秒則完全無感。原本設 2 會被 core 0 上的 SR Feed
    // （跑 AFE 前處理，很吃 CPU）餓死，播出來的語音斷斷續續。
    if (xTaskCreatePinnedToCore(audioTask, "sfx", 4096, nullptr, 6, &s_task, 0) !=
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
