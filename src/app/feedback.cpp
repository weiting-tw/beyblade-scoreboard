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
// 全部即時合成，不用音檔：一段 200ms 的 WAV 要近 9KB，八個音效就要佔 littlefs
// 還得處理檔案系統；合成只要幾行程式，延遲也最低。
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

// 佇列元素。sfx == Sfx::Count 代表這是一則播報，內容看 a / b。
// 整句放同一個項目，佇列滿時要嘛整句丟掉、要嘛整句播完，不會播出半句。
struct AudioMsg {
    Sfx sfx;
    Voice a;
    Voice b;
    Voice c;
};

TaskHandle_t s_task = nullptr;

// 要求中止目前這段播放。playPcm 每個 chunk 檢查一次，所以最慢 32ms 就停。
volatile bool s_abort = false;

// 一次處理 512 frame（22.05kHz 下約 23ms）。I2S 的 DMA 只有
// 6 x 240 = 1440 frame（約 65ms），
// 而且 auto_clear = true —— 餵不及時它會直接輸出靜音，聽起來就是斷音。
// 寫得大塊一點可以減少呼叫次數、拉開與 underrun 的距離。
// stereo 16bit -> 2KB，放 static 不吃堆疊。
constexpr int kChunkFrames = 512;
int16_t s_chunk[kChunkFrames * 2];

// --- 語音片段 ---------------------------------------------------------
//
// 有錄好的語音就用語音，沒有才退回合成音。
// 片段由 tools/gen_voice_clips.py 在 Mac 上用 piper TTS 產生。
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

bool getVoiceClip(Voice v, const int16_t** data, unsigned* len) {
    switch (v) {
        case Voice::PlayerOne:
            *data = clip_player_one;
            *len = clip_player_one_len;
            return true;
        case Voice::PlayerTwo:
            *data = clip_player_two;
            *len = clip_player_two_len;
            return true;
        case Voice::SpinFinish:
            *data = clip_spin;
            *len = clip_spin_len;
            return true;
        case Voice::OverFinish:
            *data = clip_over;
            *len = clip_over_len;
            return true;
        case Voice::BurstFinish:
            *data = clip_burst;
            *len = clip_burst_len;
            return true;
        case Voice::XtremeFinish:
            *data = clip_xtreme;
            *len = clip_xtreme_len;
            return true;
        case Voice::Wins:
            *data = clip_wins;
            *len = clip_wins_len;
            return true;
        default:
            return false;
    }
}

// 片段是單聲道，I2S 開在 STEREO，因此要複製到左右兩聲道。
void playPcm(const int16_t* mono, unsigned len) {
    unsigned done = 0;
    while (done < len) {
        if (s_abort) {
            return;
        }
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

// 播報時段與段之間的停頓。連著播「Xtreme Finish」「Player One」「Wins」
// 三段中間沒有空隙的話，會糊成一長串聽不出斷句。
constexpr uint32_t kAnnounceGapMs = 140;

void playSilence(uint32_t ms) {
    int total = static_cast<int>(kAudioSampleRate) * static_cast<int>(ms) / 1000;
    while (total > 0) {
        const int n = (total < kChunkFrames) ? total : kChunkFrames;
        for (int i = 0; i < n * 2; ++i) {
            s_chunk[i] = 0;
        }
        audioBusI2S().write(reinterpret_cast<uint8_t*>(s_chunk),
                            static_cast<size_t>(n) * 2 * sizeof(int16_t));
        total -= n;
    }
}

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
    AudioMsg msg;
    bool ampOn = false;

    for (;;) {
        // 用逾時等待而不是無限等待：逾時代表一段時間沒有音效了，
        // 這時才把功放關掉。
        if (xQueueReceive(s_queue, &msg, pdMS_TO_TICKS(kAmpIdleMs)) != pdTRUE) {
            if (ampOn) {
                flushDma();
                audioBusSpeakerEnable(false);
                ampOn = false;
            }
            continue;
        }

        // 每則新訊息都從沒有中止要求的狀態開始。
        s_abort = false;

        const bool isAnnounce = (msg.sfx == Sfx::Count);
        const int idx = static_cast<int>(msg.sfx);
        if (!isAnnounce && (idx < 0 || idx >= kSfxCount)) {
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

        if (isAnnounce) {
            const Voice parts[3] = {msg.a, msg.b, msg.c};
            bool first = true;
            for (const Voice v : parts) {
                if (getVoiceClip(v, &clip, &clipLen)) {
                    if (!first) {
                        playSilence(kAnnounceGapMs);
                    }
                    playPcm(clip, clipLen);
                    first = false;
                }
            }
        } else if (getClip(msg.sfx, &clip, &clipLen)) {
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
    s_queue = xQueueCreate(3, sizeof(AudioMsg));
    if (s_queue == nullptr) {
        Serial.println("[feedback] 建立音效佇列失敗");
        return;
    }

    // 獨立任務：合成與 I2S 寫入都會阻塞，放在 LVGL 執行緒會造成畫面卡頓。
    //
    // 優先權 6 刻意訂得高。音效播放是硬即時：I2S 的 DMA 只有 90ms 且
    // auto_clear = true，錯過期限它會直接輸出靜音，立刻聽得出斷音。
    // 這個值原本是為了壓過 esp-sr 的任務（已移除）而訂，理由消失了但結論不變 ——
    // core 0 上任何吃 CPU 的東西都不該有機會把它餓死。
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
    AudioMsg msg{sfx, Voice::None, Voice::None, Voice::None};
    xQueueSend(s_queue, &msg, 0);
}

void feedbackAnnounce(Voice first, Voice second, Voice third) {
    if (!g_store.settings().enableAnnounce) {
        return;
    }
    if (s_queue == nullptr || first == Voice::None) {
        return;
    }
    AudioMsg msg{Sfx::Count, first, second, third};
    xQueueSend(s_queue, &msg, 0);
}

void feedbackStop() {
    if (s_queue == nullptr) {
        return;
    }
    // 先設中止旗標再清佇列：反過來的話清完之後、旗標設好之前，
    // 音效任務可能又取走一則剛送進來的訊息。
    s_abort = true;
    xQueueReset(s_queue);
}

void feedbackHaptic(uint16_t ms) {
    if (!g_store.settings().enableVibration) {
        return;
    }
    (void)ms;  // 板上沒有震動馬達，要外接才能實作
}

}  // namespace bey
