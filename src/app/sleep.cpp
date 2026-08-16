#include "sleep.h"

#include <Arduino.h>

#include "../bsp/Display_ST77916.h"
#include "../ui/ui.h"
#include "settings_store.h"

namespace bey {
namespace {

uint32_t s_lastActivityMs = 0;
bool s_off = false;

bool inMatch() {
    switch (ui::currentScreen()) {
        case ui::ScreenId::Countdown:
        case ui::ScreenId::Score:
        case ui::ScreenId::Round:
        case ui::ScreenId::Complete:
            return true;
        default:
            return false;
    }
}

void wake() {
    if (!s_off) {
        return;
    }
    s_off = false;
    Set_Backlight(g_store.settings().brightness);
    Serial.println("[sleep] 喚醒");
}

}  // namespace

void sleepNoteActivity() {
    s_lastActivityMs = millis();
    wake();
}

bool sleepScreenOff() { return s_off; }

void sleepPoll() {
    const uint16_t sec = g_store.settings().sleepSec;
    if (sec == 0 || inMatch()) {
        // 設定為 0 或比賽進行中。已經熄掉的話（例如在設定頁熄了才切回比賽）
        // 要點亮，不然使用者會對著黑屏。
        wake();
        s_lastActivityMs = millis();
        return;
    }
    if (s_off) {
        return;
    }
    if (millis() - s_lastActivityMs >= static_cast<uint32_t>(sec) * 1000UL) {
        s_off = true;
        Set_Backlight(0);
        Serial.printf("[sleep] 閒置 %u 秒，關背光\n", (unsigned)sec);
    }
}

}  // namespace bey
