// 頂部狀態晶片（電量）。
//
// 建在 lv_layer_top 上，是唯一一個跨畫面存活的 widget —— 各 show 函式都會
// 重建整個畫面，把電量放進畫面裡就得在九個地方各做一份，而且 lv_timer 會
// 追著被 auto_del 釋放的 label 跑。
//
// Z 序：必須在 ui::init() 最先建立，才會被 overlay 的半透明遮罩蓋住，
// 而不是浮在確認對話框上面。
#pragma once

namespace bey {
namespace ui {

// 建立晶片與更新用的 lv_timer。電量計不存在時什麼都不做。
void statusChipInit();

// 是否隱藏。loadScreen() 會統一設回 false，需要淨空版面的畫面
// （計分、倒數、局結果）在載入後自行設 true。
// 低電量時會忽略這個要求 —— 那種時候使用者需要看到。
void statusChipSetHidden(bool hidden);

}  // namespace ui
}  // namespace bey
