#include "gesture.h"

#include <cmath>

namespace bey {
namespace {

Gesture s_pending = Gesture::None;
TouchPoint s_last{0, 0, false};

// 螢幕中心與邊緣門檻。實測沿邊緣畫圈時半徑落在 142~191，
// 130 這個門檻讓「貼著邊緣轉」算數、「在中間亂畫」不算。
constexpr float kCx = 180.0f;
constexpr float kCy = 180.0f;
constexpr float kEdgeR = 130.0f;

// 一格 30 度。轉一圈 12 格，配上音量每格 5 就是轉一圈調 60，
// 手感接近實體旋鈕。太小會太靈敏、手抖就跳好幾格。
constexpr float kStepDeg = 30.0f;

// 滑動手勢只認從螢幕邊緣起手的。
//
// 設定頁與歷史頁的內容是可捲動的，往下捲時手指本來就是由下往上滑，
// CST816 一樣回報 SWIPE_UP —— 不看起手位置的話，捲個內容就會跳回上一頁。
// 從底部這一條帶起手才算「返回」，從中間起手的就是單純捲動。
constexpr int16_t kEdgeBand = 60;
constexpr int16_t kScreenH = 360;

int16_t s_startY = -1;

bool s_rotTracking = false;
float s_rotLastAngle = 0.0f;
float s_rotAccum = 0.0f;
int8_t s_rotSteps = 0;

// CST816 的 gesture 暫存器在手指離開後不會自己清掉 —— 實測一次上滑之後，
// 接下來四十幾次輪詢都還在回報 UP（座標是 0,0，因為已經沒有觸點）。
// 只認手指還在螢幕上、而且是從沒有手勢變成有手勢的那一刻。
void feedSwipe(bool down, uint8_t hwGesture) {
    static Gesture lastG = Gesture::None;

    Gesture g = Gesture::None;
    if (down) {
        switch (hwGesture) {
            case 1: g = Gesture::SwipeUp; break;
            case 2: g = Gesture::SwipeDown; break;
            case 3: g = Gesture::SwipeLeft; break;
            case 4: g = Gesture::SwipeRight; break;
            default: break;
        }
    }

    if (g != Gesture::None && lastG == Gesture::None && s_startY >= 0) {
        const bool fromBottom = s_startY >= kScreenH - kEdgeBand;
        const bool fromTop = s_startY < kEdgeBand;
        // 上滑要從底部起手、下滑要從頂部起手。左右滑目前沒有用途。
        if ((g == Gesture::SwipeUp && fromBottom) ||
            (g == Gesture::SwipeDown && fromTop)) {
            // 後來的蓋掉還沒被取走的：手勢是即時操作，過期的沒有意義。
            s_pending = g;
        }
    }
    lastG = g;
}

// 邊緣旋轉：CST816 判不出這個，自己從觸控座標的極角累積算。
void feedRotation(int16_t x, int16_t y, bool down) {
    if (!down) {
        s_rotTracking = false;
        s_rotAccum = 0.0f;
        return;
    }

    const float dx = static_cast<float>(x) - kCx;
    const float dy = static_cast<float>(y) - kCy;
    if (dx * dx + dy * dy < kEdgeR * kEdgeR) {
        // 手指移進中央就停止追蹤，重新進到邊緣時從頭算，
        // 免得「進來又出去」被算成一段連續旋轉。
        s_rotTracking = false;
        s_rotAccum = 0.0f;
        return;
    }

    const float a = atan2f(dy, dx) * 180.0f / static_cast<float>(M_PI);
    if (!s_rotTracking) {
        s_rotTracking = true;
        s_rotLastAngle = a;
        s_rotAccum = 0.0f;
        return;
    }

    // 折回 -180..180 再累加，否則每轉過正下方就會多算一整圈。
    float d = a - s_rotLastAngle;
    while (d > 180.0f) d -= 360.0f;
    while (d < -180.0f) d += 360.0f;
    s_rotLastAngle = a;
    s_rotAccum += d;

    while (s_rotAccum >= kStepDeg) {
        s_rotAccum -= kStepDeg;
        if (s_rotSteps < 100) s_rotSteps++;
    }
    while (s_rotAccum <= -kStepDeg) {
        s_rotAccum += kStepDeg;
        if (s_rotSteps > -100) s_rotSteps--;
    }
}

}  // namespace

void touchFeed(int16_t x, int16_t y, bool down, uint8_t hwGesture) {
    // 這一筆觸控的起點：滑動手勢要靠它分辨「從邊緣起手」與「在中間捲動」。
    if (down && !s_last.down) {
        s_startY = y;
    }

    feedSwipe(down, hwGesture);
    feedRotation(x, y, down);

    s_last.down = down;
    if (down) {
        s_last.x = x;
        s_last.y = y;
    } else {
        s_startY = -1;
    }
}

bool gesturePoll(Gesture& out) {
    if (s_pending == Gesture::None) {
        return false;
    }
    out = s_pending;
    s_pending = Gesture::None;
    return true;
}

int8_t gestureTakeRotation() {
    const int8_t n = s_rotSteps;
    s_rotSteps = 0;
    return n;
}

TouchPoint touchLast() { return s_last; }

}  // namespace bey
