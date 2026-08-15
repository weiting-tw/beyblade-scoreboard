// 繁體中文子集字型。
//
// 由 tools/gen_fonts.py 從 Noto Sans CJK TC（OFL 授權，可嵌入）產生。
// 只含 ASCII 0x20–0x7F 加上專案原始碼裡實際出現的漢字 —— 全字集要 1–2MB，
// 子集只要幾十 KB。
//
// **新增中文字串後必須重跑 tools/gen_fonts.py**，否則新字會顯示成空白方塊。
//
// 尺寸選用：
//   font_tc_16  列表、次要說明、窄按鈕
//   font_tc_22  一般按鈕與標題（等同原本的 montserrat_20）
//   font_tc_30  大標題、勝者、確認訊息
// 純數字的大字（比分、倒數）仍用 lv_font_montserrat_36 / 48 ——
// 數字不需要漢字，Montserrat 的字面也比較適合計分板。
#pragma once

#include <lvgl.h>

LV_FONT_DECLARE(font_tc_16);
LV_FONT_DECLARE(font_tc_22);
LV_FONT_DECLARE(font_tc_30);
