#include "feedback.h"

#include <Arduino.h>

#include "settings_store.h"

namespace bey {

void feedbackInit() {
    // Phase 5 才會在此初始化 I2S + ES8311，並把 GPIO9 (SPK_EN) 拉低待命。
}

void feedbackPlay(Sfx sfx) {
    if (!g_store.settings().match.enableSound) {
        return;
    }
    (void)sfx;  // Phase 5 實作
}

void feedbackHaptic(uint16_t ms) {
    if (!g_store.settings().enableVibration) {
        return;
    }
    (void)ms;  // Phase 5 實作（震動馬達尚未接線）
}

}  // namespace bey
