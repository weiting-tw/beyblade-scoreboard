#include "settings_store.h"

#include <Preferences.h>

#include <cstring>

namespace bey {

SettingsStore g_store;

namespace {

Preferences prefs;

constexpr const char* kNamespace = "bey";
constexpr const char* kKeySettings = "cfg";
constexpr const char* kKeyHistory = "hist";
constexpr const char* kKeyHistCount = "histN";

}  // namespace

AppSettings defaultSettings() {
    AppSettings s{};
    s.magic = kSettingsMagic;
    s.match = defaultConfig();
    s.rules = defaultRuleSet();
    s.brightness = 80;
    s.volume = 75;
    s.sleepSec = 0;  // 第一版預設不自動休眠，避免比賽中途熄屏
    s.enableVibration = false;
    s.enableAnnounce = true;
    s.enableBatteryBadge = true;
    s.enableGestures = true;
    return s;
}

void SettingsStore::begin() {
    prefs.begin(kNamespace, /*readOnly=*/false);

    AppSettings loaded{};
    const size_t got = prefs.getBytes(kKeySettings, &loaded, sizeof(loaded));
    if (got == sizeof(loaded) && loaded.magic == kSettingsMagic) {
        s_ = loaded;
        // 防禦：NVS 內容可能來自被中斷的寫入。
        if (s_.match.targetScore < 1) {
            s_.match.targetScore = 1;
        }
        if (s_.brightness < 10) {
            s_.brightness = 10;
        }
        if (s_.brightness > 100) {
            s_.brightness = 100;
        }
        if (s_.volume > 100) {
            s_.volume = 100;
        }
        s_.match.p1Name[kNameLen - 1] = '\0';
        s_.match.p2Name[kNameLen - 1] = '\0';
    } else {
        s_ = defaultSettings();
        prefs.putBytes(kKeySettings, &s_, sizeof(s_));
    }

    loadHistory();
}

void SettingsStore::save() {
    s_.magic = kSettingsMagic;
    prefs.putBytes(kKeySettings, &s_, sizeof(s_));
}

void SettingsStore::restoreDefaults() {
    s_ = defaultSettings();
    save();
}

// --- 歷史紀錄 -----------------------------------------------------------

void SettingsStore::loadHistory() {
    histCount_ = prefs.getInt(kKeyHistCount, 0);
    if (histCount_ < 0 || histCount_ > kMaxHistory) {
        histCount_ = 0;
    }
    if (histCount_ > 0) {
        const size_t want = sizeof(MatchRecord) * static_cast<size_t>(histCount_);
        if (prefs.getBytes(kKeyHistory, hist_, want) != want) {
            histCount_ = 0;  // 資料不完整就整批丟棄，不冒險顯示垃圾
        }
    }
}

void SettingsStore::saveHistory() {
    prefs.putInt(kKeyHistCount, histCount_);
    if (histCount_ > 0) {
        prefs.putBytes(kKeyHistory, hist_,
                       sizeof(MatchRecord) * static_cast<size_t>(histCount_));
    }
}

const MatchRecord& SettingsStore::history(int index) const {
    if (index < 0) {
        index = 0;
    }
    if (index >= histCount_) {
        index = histCount_ > 0 ? histCount_ - 1 : 0;
    }
    return hist_[index];
}

void SettingsStore::appendHistory(const MatchRecord& rec) {
    // index 0 永遠是最新一場，滿了就擠掉最舊的。
    const int keep = (histCount_ < kMaxHistory) ? histCount_ : kMaxHistory - 1;
    if (keep > 0) {
        std::memmove(&hist_[1], &hist_[0], sizeof(MatchRecord) * static_cast<size_t>(keep));
    }
    hist_[0] = rec;
    if (histCount_ < kMaxHistory) {
        ++histCount_;
    }
    saveHistory();
}

void SettingsStore::clearHistory() {
    histCount_ = 0;
    prefs.putInt(kKeyHistCount, 0);
    prefs.remove(kKeyHistory);
}

}  // namespace bey
