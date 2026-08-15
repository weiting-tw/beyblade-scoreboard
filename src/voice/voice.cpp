#include "voice.h"

#include <Arduino.h>
#include <Wire.h>

#include <cstring>

#include "../audio/audio_bus.h"
#include "bey_sr.h"

namespace bey {
namespace {

QueueHandle_t s_queue = nullptr;
volatile VoiceState s_state = VoiceState::Disabled;

// --- 命令詞表 ---------------------------------------------------------
//
// **命令必須用無聲調拼音，不能用漢字。**
//
// esp-sr 文件說 MultiNet6/7 中文「only accepts grapheme inputs」，聽起來像是
// 可以直接寫漢字。實測不行 —— 註冊繁體漢字時 mn7_cn 對每一條都回：
//     E (1880) MN_COMMAND: invalid command, please check format, 一號爆裂勝.
// 而同樣內容改成無聲調拼音就全部接受。esp-sr 隨附的預設命令表
// (model/multinet_model/fst/commands_cn.txt) 也確實是拼音格式。
// （簡體漢字未測試；拼音已驗證可用，沒有理由再冒險。）
//
// label 是給人看的，phrase 是給模型的，兩者不可混用。
//
// 命令刻意都設計成 3 個音節以上。esp-sr 對短命令的誤觸發率明顯較高，
// 而「重新開始」誤觸發的代價是整場比分歸零。
struct CmdSpec {
    VoiceCmd cmd;
    const char* label;   // UI 與 log 顯示用
    const char* phrase;  // 餵給 mn7_cn 的無聲調拼音
};

constexpr CmdSpec kCommands[] = {
    {VoiceCmd::P1Normal, "一號普通勝", "yi hao pu tong sheng"},
    {VoiceCmd::P1Burst, "一號爆裂勝", "yi hao bao lie sheng"},
    {VoiceCmd::P1Xtreme, "一號極限勝", "yi hao ji xian sheng"},
    {VoiceCmd::P2Normal, "二號普通勝", "er hao pu tong sheng"},
    {VoiceCmd::P2Burst, "二號爆裂勝", "er hao bao lie sheng"},
    {VoiceCmd::P2Xtreme, "二號極限勝", "er hao ji xian sheng"},
    {VoiceCmd::Undo, "取消上一分", "qu xiao shang yi fen"},
    {VoiceCmd::Reset, "重新開始", "chong xin kai shi"},
    {VoiceCmd::Start, "開始比賽", "kai shi bi sai"},
    {VoiceCmd::NextRound, "下一局", "xia yi ju"},
    {VoiceCmd::EndMatch, "結束比賽", "jie shu bi sai"},
};

constexpr int kCommandCount = sizeof(kCommands) / sizeof(kCommands[0]);

bey_sr_cmd_t s_srCommands[kCommandCount];

// bey_sr_cmd_t 的 str / phoneme 是固定長度字元陣列（不是指標），必須複製。
void setEntry(bey_sr_cmd_t& dst, int id, const char* label, const char* phrase) {
    dst.command_id = id;
    std::strncpy(dst.str, label, sizeof(dst.str) - 1);
    dst.str[sizeof(dst.str) - 1] = '\0';
    std::strncpy(dst.phoneme, phrase, sizeof(dst.phoneme) - 1);
    dst.phoneme[sizeof(dst.phoneme) - 1] = '\0';
}

void buildCommandTable() {
    for (int i = 0; i < kCommandCount; ++i) {
        setEntry(s_srCommands[i], static_cast<int>(kCommands[i].cmd),
                 kCommands[i].label, kCommands[i].phrase);
    }
}

// esp-sr 的 feed 任務靠這個回呼取得音訊。
esp_err_t i2sFill(void* arg, void* out, size_t len, size_t* bytesRead,
                  uint32_t timeoutMs) {
    (void)arg;
    (void)timeoutMs;
    const size_t got = audioBusI2S().readBytes(static_cast<char*>(out), len);
    *bytesRead = got;
    return (got > 0) ? ESP_OK : ESP_FAIL;
}

// --- 辨識事件 ---------------------------------------------------------

void onSrEvent(void* arg, bey_sr_event_t event, int commandId, int phraseId) {
    (void)arg;
    switch (event) {
        case BEY_SR_EVENT_WAKEWORD:
            Serial.println("[voice] 偵測到喚醒詞");
            break;

        case BEY_SR_EVENT_WAKEWORD_CHANNEL:
            // 通道確認後才切到命令模式，這是官方流程。
            s_state = VoiceState::Awake;
            bey_sr_set_mode(BEY_SR_MODE_COMMAND);
            Serial.println("[voice] 已喚醒，請說指令");
            break;

        case BEY_SR_EVENT_TIMEOUT:
            s_state = VoiceState::Listening;
            bey_sr_set_mode(BEY_SR_MODE_WAKEWORD);
            Serial.println("[voice] 逾時，回到待機");
            break;

        case BEY_SR_EVENT_COMMAND: {
            const char* label = "(未知)";
            if (phraseId >= 0 && phraseId < kCommandCount) {
                label = s_srCommands[phraseId].str;
            }
            Serial.printf("[voice] 辨識到命令 %d：%s\n", commandId, label);

            if (commandId >= 0 && commandId < static_cast<int>(VoiceCmd::Count) &&
                s_queue != nullptr) {
                const auto cmd = static_cast<VoiceCmd>(commandId);
                // 佇列滿了就丟棄：語音命令過期沒有意義，
                // 積壓一堆舊命令一次套用比漏掉一次更糟。
                xQueueSend(s_queue, &cmd, 0);
            }

            s_state = VoiceState::Listening;
            bey_sr_set_mode(BEY_SR_MODE_WAKEWORD);
            break;
        }

        default:
            break;
    }
}

}  // namespace

bool voiceBegin() {
    s_state = VoiceState::Disabled;

    // I2S 與 codec 由 audio_bus 統一初始化（音效輸出共用同一條匯流排）。
    if (!audioBusHasMic()) {
        Serial.println("[voice] 麥克風不可用，語音辨識停用");
        return false;
    }

    s_queue = xQueueCreate(4, sizeof(VoiceCmd));
    if (s_queue == nullptr) {
        Serial.println("[voice] 建立命令佇列失敗");
        return false;
    }

    buildCommandTable();

    const esp_err_t err = bey_sr_start(i2sFill, nullptr, BEY_SR_CHANNELS_STEREO,
                                       BEY_SR_MODE_WAKEWORD, s_srCommands,
                                       kCommandCount, onSrEvent, nullptr);
    if (err != ESP_OK) {
        Serial.printf("[voice] bey_sr_start 失敗：%s\n", esp_err_to_name(err));
        Serial.println("[voice] 最常見原因是 model 分割區沒有燒入 srmodels.bin，");
        Serial.println("[voice] 或模型包裡沒有 nihaoxiaozhi / mn*_cn 模型。");
        return false;
    }

    s_state = VoiceState::Listening;
    Serial.printf("[voice] 就緒，%d 個命令\n", kCommandCount);
    Serial.println("[voice] 說「你好小智」喚醒");
    return true;
}

VoiceState voiceState() { return s_state; }

const char* voiceStatusText() {
    switch (s_state) {
        case VoiceState::Listening:
            return "聆聽中";
        case VoiceState::Awake:
            return "請說指令";
        default:
            return "語音停用";
    }
}

bool voicePoll(VoiceCmd& out) {
    if (s_queue == nullptr) {
        return false;
    }
    return xQueueReceive(s_queue, &out, 0) == pdTRUE;
}

const char* voiceCmdLabel(VoiceCmd cmd) {
    const int i = static_cast<int>(cmd);
    if (i < 0 || i >= kCommandCount) {
        return "";
    }
    return kCommands[i].label;
}

}  // namespace bey
