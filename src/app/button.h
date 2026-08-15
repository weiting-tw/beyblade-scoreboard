// 實體按鍵（BOOT，GPIO0）。
//
// 板上有 PWR 與 BOOT 兩顆，但實測只有 BOOT 讀得到 —— 把候選腳位全設成上拉
// 輸入後逐一按過，只有 GPIO0 有變化，PWR 是接到電源管理電路而不是可讀的
// GPIO。這其實合理：那顆是拿來開關機的，不該讓韌體攔截。
//
// GPIO0 是 strapping pin，但那只在 reset 那一瞬間判定，開機之後就是普通 GPIO。
#pragma once

namespace bey {

void buttonBegin();

// 按下的那一刻回傳 true 一次。放開不產生事件。
bool buttonPressed();

}  // namespace bey
