// 圓形 360×360 螢幕的版面基礎（規格第 6、9 節）。
//
// 核心約束：螢幕是圓的，四角不存在。任何元件的四個角都必須落在半徑
// kSafeR 的圓內，否則會被切掉。widthAtY() 把這件事變成可計算的數字，
// 而不是每個畫面各自目測。
#pragma once

#include <lvgl.h>

namespace bey {
namespace ui {

constexpr lv_coord_t kScreenW = 360;
constexpr lv_coord_t kScreenH = 360;
constexpr lv_coord_t kCenterX = kScreenW / 2;
constexpr lv_coord_t kCenterY = kScreenH / 2;

// 內容安全半徑。實際面板半徑 180，留 14px 給圓周切邊與觸控邊緣不準。
constexpr lv_coord_t kSafeR = 166;

// 觸控目標下限（規格第 9 節：寬 60、高 45 以上）。
constexpr lv_coord_t kMinBtnW = 60;
constexpr lv_coord_t kMinBtnH = 45;

// 顏色
lv_color_t colBg();
lv_color_t colP1();
lv_color_t colP2();
lv_color_t colAccent();
lv_color_t colMuted();
lv_color_t colText();
lv_color_t colSubtle();  // 次要說明文字（colMuted 太暗，只適合當背景）
lv_color_t colDanger();

// 在距離中心垂直 dy 處，安全圓內可用的最大總寬度。
// 例：一列按鈕的下緣 dy 越大，可用寬度越窄。
lv_coord_t widthAtY(lv_coord_t dy);

// 一個高度 h、中心位於 dy 的水平長條，其可用最大寬度
// （取上下緣中較窄的一邊）。
lv_coord_t barWidth(lv_coord_t dy, lv_coord_t h);

// 建立一個黑底全螢幕容器（不自動載入）。
lv_obj_t* makeScreen();

// 淡入切換畫面，並刪除舊畫面以釋放 LVGL heap。
void loadScreen(lv_obj_t* scr);

// 置中對齊的文字標籤。
lv_obj_t* makeLabel(lv_obj_t* parent, const char* text, const lv_font_t* font,
                    lv_color_t color, lv_coord_t dx, lv_coord_t dy);

// 圓角按鈕。w/h 會被夾到觸控目標下限之上。
// font 傳 nullptr 時用 montserrat_20；窄按鈕請改傳 &lv_font_montserrat_14。
lv_obj_t* makeButton(lv_obj_t* parent, const char* text, lv_coord_t dx,
                     lv_coord_t dy, lv_coord_t w, lv_coord_t h,
                     lv_color_t color, lv_event_cb_t cb, void* userData,
                     const lv_font_t* font = nullptr);

}  // namespace ui
}  // namespace bey
