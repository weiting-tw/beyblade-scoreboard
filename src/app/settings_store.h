// 設定與歷史紀錄的持久化（規格第 8 節）。
//
// 一律走 NVS（Arduino Preferences）：
//   - 設定整包以 blob 存，避免逐欄位讀寫；改版時靠 magic 判斷並回退預設值
//   - 歷史紀錄固定長度環狀陣列，最多 20 場
// littlefs 分割區保留給 Phase 5 的音效素材，第一版不使用。
#pragma once

#include <cstdint>

#include "match_core.h"

namespace bey {

constexpr int kMaxHistory = 20;

// blob 版本識別。改動下列 struct 版面時務必 +1，否則會讀到舊版亂數。
// v4：新增 volume 欄位（struct 版面改變，必須升版）。
// v3：預設賽制從 3 分改為 4 分。純粹改預設值不需要動版面，但不升版的話
//     NVS 裡存的舊值會蓋過新預設，使用者永遠看不到新的預設。
constexpr uint32_t kSettingsMagic = 0x42455904;  // BEY + v4

struct AppSettings {
    uint32_t magic;
    MatchConfig match;
    RuleSet rules;
    uint8_t brightness;  // 10..100
    uint8_t volume;      // 0..100，喇叭音量
    uint16_t sleepSec;   // 0 = 不自動休眠
    bool enableVibration;
};

AppSettings defaultSettings();

struct MatchRecord {
    char p1Name[kNameLen];
    char p2Name[kNameLen];
    uint8_t score1;
    uint8_t score2;
    uint8_t rounds;
    uint8_t winner;      // bey::Winner
    uint32_t timestamp;  // RTC epoch 秒；取不到時為 0
};

class SettingsStore {
   public:
    // 開機時呼叫一次。讀不到或 magic 不符時寫入預設值。
    void begin();

    const AppSettings& settings() const { return s_; }
    AppSettings& mutableSettings() { return s_; }
    void save();
    void restoreDefaults();

    // --- 歷史紀錄 ---
    int historyCount() const { return histCount_; }
    // index 0 = 最新一場
    const MatchRecord& history(int index) const;
    void appendHistory(const MatchRecord& rec);
    void clearHistory();

   private:
    void loadHistory();
    void saveHistory();

    AppSettings s_;
    MatchRecord hist_[kMaxHistory];
    int histCount_ = 0;
};

// 全域單例，UI 層直接取用。
extern SettingsStore g_store;

}  // namespace bey
