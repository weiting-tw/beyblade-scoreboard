#include "power.h"

#include <Arduino.h>

namespace bey {
namespace power {
namespace {

BatteryState s_state;
bool s_available = false;
bool s_low = false;

constexpr uint8_t kLowEnter = 15;
constexpr uint8_t kLowExit = 18;

}  // namespace

void begin() {
    s_available = bq27220Present();
    if (s_available) {
        poll();
        Serial.printf("[power] 電量計就緒 %u%% %umV\n", s_state.percent,
                      s_state.milliVolts);
    } else {
        Serial.println("[power] 找不到電量計，停用電量顯示");
    }
}

bool available() { return s_available; }

void poll() {
    if (!s_available) {
        return;
    }
    if (!bq27220Read(s_state)) {
        return;  // 單次讀取失敗就沿用上一筆，不要讓顯示閃掉
    }

    if (s_low) {
        if (s_state.percent > kLowExit) {
            s_low = false;
        }
    } else if (s_state.percent <= kLowEnter) {
        s_low = true;
    }
}

const BatteryState& state() { return s_state; }

bool low() { return s_low; }

}  // namespace power
}  // namespace bey
