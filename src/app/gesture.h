// 觸控手勢。
//
// CST816 自己就會判滑動方向，不必從座標軌跡重算。但它的 gesture 暫存器在
// 手指離開後不會自動清掉 —— 實測一次上滑之後，接下來四十幾次輪詢都還在
// 回報 UP（座標是 0,0，因為已經沒有觸點）。直接接上去的話一次滑動會觸發
// 幾十次動作，所以這裡做邊緣偵測，一次滑動只產生一個事件。
//
// 產生端在 LVGL 的 indev 讀取回呼，消費端在 loop()，兩者都在 LVGL 執行緒
// 上，所以單一變數就夠，不需要佇列或鎖。
#pragma once

#include <cstdint>

namespace bey {

enum class Gesture : uint8_t {
    None,
    SwipeUp,
    SwipeDown,
    SwipeLeft,
    SwipeRight,
};

// 餵入一次觸控取樣。hwGesture 是 CST816 自己判的手勢原始值
// （0=無 1=上 2=下 3=左 4=右）。所有判定都在這裡面做。
void touchFeed(int16_t x, int16_t y, bool down, uint8_t hwGesture);

// 取出待處理的手勢。沒有時回傳 false。
bool gesturePoll(Gesture& out);

// 最後一個觸控點。touch_data 在 indev 讀取回呼尾端會被清零，
// 想在別處看到座標就得在那裡先存一份。
struct TouchPoint {
    int16_t x;
    int16_t y;
    bool down;
};

TouchPoint touchLast();

// 沿螢幕邊緣旋轉。CST816 判不出這個手勢，是從觸控座標的極角累積算的。
//
// 取走自上次呼叫以來累積的格數（正 = 順時針），並歸零。用計數器而不是
// 單一事件：一格 30 度、轉一圈 12 格，手指快轉時一次輪詢就可能跨過兩格，
// 單一事件會被後來的蓋掉。
int8_t gestureTakeRotation();

}  // namespace bey
