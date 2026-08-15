// 圓形 360×360 螢幕的版面基礎（規格第 6、9 節）。
//
// 核心約束：螢幕是圓的，四角不存在。任何元件的四個角都必須落在半徑
// kSafeR 的圓內，否則會被切掉。widthAtY() 把這件事變成可計算的數字，
// 而不是每個畫面各自目測。
#pragma once

#include <lvgl.h>

#include "fonts.h"

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

// 轉場方向。方向要對應「使用者感覺自己往哪走」，走錯方向比沒有動畫更糟。
enum class Nav : uint8_t {
    Forward,  // 往流程深處（首頁→賽制→準備）：畫面向左推
    Back,     // 退回上一層：畫面向右推
    Replace,  // 同一頁重畫（撤銷後更新比分）：不做轉場，否則會像跳頁
    Rise,     // 儀式性進場（比賽完成）：由下往上覆蓋
    Fade,     // 狀態切換（倒數→計分）：淡入
};

// 切換畫面並刪除舊畫面以釋放 LVGL heap。
void loadScreen(lv_obj_t* scr, Nav nav = Nav::Forward);

// --- 動畫工具 -----------------------------------------------------------
//
// 縮放動畫靠 LVGL 8 的 transform_zoom（256 = 原尺寸）。這會讓物件先被畫到
// 一塊暫存圖層再變形，需要 LV_DRAW_COMPLEX=1 與 LV_LAYER_SIMPLE_BUF_SIZE
// 的緩衝區，兩者在 include/lv_conf.h 都已滿足。

// 從 fromZoom 彈到 toZoom，帶輕微過衝。用於數字與得分的強調。
void animPop(lv_obj_t* obj, uint16_t fromZoom, uint16_t toZoom, uint32_t ms,
             uint32_t delay = 0);

// 透明度淡入。
void animFadeIn(lv_obj_t* obj, uint32_t ms, uint32_t delay = 0);

// 讓 label 的數字從 from 跑到 to（比分揭曉用）。label 內容會被覆寫成純整數。
void animCountUp(lv_obj_t* label, int from, int to, uint32_t ms,
                 uint32_t delay = 0);

// 從中心擴散並淡出的光環，無限循環。用於勝利慶祝。
// 回傳建立出來的環物件（隨 parent 一起被釋放）。
lv_obj_t* animRingBurst(lv_obj_t* parent, lv_color_t color, uint32_t ms,
                        uint32_t delay = 0);

// 置中對齊的文字標籤。
lv_obj_t* makeLabel(lv_obj_t* parent, const char* text, const lv_font_t* font,
                    lv_color_t color, lv_coord_t dx, lv_coord_t dy);

// 圓角按鈕。w/h 會被夾到觸控目標下限之上。
// font 傳 nullptr 時用 font_tc_22；窄按鈕請改傳 &font_tc_16。
lv_obj_t* makeButton(lv_obj_t* parent, const char* text, lv_coord_t dx,
                     lv_coord_t dy, lv_coord_t w, lv_coord_t h,
                     lv_color_t color, lv_event_cb_t cb, void* userData,
                     const lv_font_t* font = nullptr);

}  // namespace ui
}  // namespace bey
