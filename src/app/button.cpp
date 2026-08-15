#include "button.h"

#include <Arduino.h>

namespace bey {
namespace {

constexpr int kPin = 0;  // BOOT

// 實測按鍵很乾淨（連按四下、八次變化都沒有彈跳），但機械接點本來就會抖，
// 加一道最小間隔比事後查一個偶發的重複觸發便宜。
constexpr uint32_t kDebounceMs = 200;

bool s_lastDown = false;
uint32_t s_lastEventMs = 0;

}  // namespace

void buttonBegin() { pinMode(kPin, INPUT_PULLUP); }

bool buttonPressed() {
    const bool down = (digitalRead(kPin) == LOW);
    const bool edge = down && !s_lastDown;
    s_lastDown = down;

    if (!edge) {
        return false;
    }
    const uint32_t now = millis();
    if (now - s_lastEventMs < kDebounceMs) {
        return false;
    }
    s_lastEventMs = now;
    return true;
}

}  // namespace bey
