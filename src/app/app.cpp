#include "app.h"

#include <cstring>

namespace bey {

Match g_match;

void applySettingsToMatch() {
    const AppSettings& s = g_store.settings();
    g_match.configure(s.match, s.rules);
}

uint32_t nowEpoch() {
    // Phase 1 不接 PCF85063 RTC；歷史紀錄的 timestamp 先留 0。
    // 接上後把這裡換成 RTC 讀值即可，其餘程式不必改。
    return 0;
}

void recordFinishedMatch() {
    if (!g_match.finished()) {
        return;
    }
    if (!g_store.settings().match.saveHistory) {
        return;
    }

    MatchRecord rec{};
    std::strncpy(rec.p1Name, g_match.config().p1Name, kNameLen - 1);
    std::strncpy(rec.p2Name, g_match.config().p2Name, kNameLen - 1);
    rec.p1Name[kNameLen - 1] = '\0';
    rec.p2Name[kNameLen - 1] = '\0';
    rec.score1 = static_cast<uint8_t>(g_match.scoreP1());
    rec.score2 = static_cast<uint8_t>(g_match.scoreP2());
    // round() 指向「下一局」，已完成局數要減 1。
    const int played = g_match.round() - 1;
    rec.rounds = static_cast<uint8_t>(played > 0 ? played : 0);
    rec.winner = static_cast<uint8_t>(g_match.winner());
    rec.timestamp = nowEpoch();

    g_store.appendHistory(rec);
}

}  // namespace bey
