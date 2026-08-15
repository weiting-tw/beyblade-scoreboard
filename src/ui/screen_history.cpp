// 歷史紀錄（規格第 8 節：最近 20 場）
#include <cstdio>
#include <ctime>

#include "../app/app.h"
#include "ui.h"
#include "ui_theme.h"

namespace bey {
namespace ui {
namespace {

void onBack(lv_event_t*) { showHome(); }

void makeRecordRow(lv_obj_t* parent, const MatchRecord& rec) {
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_size(row, 206, 44);
    lv_obj_set_style_bg_color(row, colMuted(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(row, LV_OPA_50, LV_PART_MAIN);
    lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(row, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_all(row, 4, LV_PART_MAIN);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    const auto w = static_cast<Winner>(rec.winner);
    const lv_color_t col =
        (w == Winner::P1) ? colP1() : ((w == Winner::P2) ? colP2() : colSubtle());

    // 左側色條標示誰贏。
    lv_obj_t* bar = lv_obj_create(row);
    lv_obj_set_size(bar, 5, 28);
    lv_obj_align(bar, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(bar, col, LV_PART_MAIN);
    lv_obj_set_style_border_width(bar, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 3, LV_PART_MAIN);

    char buf[64];
    std::snprintf(buf, sizeof(buf), "%s  %u:%u  %s", rec.p1Name,
                  static_cast<unsigned>(rec.score1),
                  static_cast<unsigned>(rec.score2), rec.p2Name);
    lv_obj_t* l = lv_label_create(row);
    lv_label_set_text(l, buf);
    lv_obj_set_style_text_font(l, &font_tc_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, colText(), LV_PART_MAIN);
    lv_obj_set_width(l, 178);
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 12, -7);

    // timestamp 為 0 代表 RTC 讀不到或時間不可信（見 pcf85063.h），
    // 那就只顯示局數，不要印出 1970 那種一看就是壞掉的日期。
    if (rec.timestamp != 0) {
        const time_t t = static_cast<time_t>(rec.timestamp);
        std::tm tm{};
        // gmtime_r 而不是 localtime_r：存進去的已經是本地時間，
        // 再套一次時區會整個偏掉。
        gmtime_r(&t, &tm);
        std::snprintf(buf, sizeof(buf), "%u 局    %02d/%02d %02d:%02d",
                      static_cast<unsigned>(rec.rounds), tm.tm_mon + 1,
                      tm.tm_mday, tm.tm_hour, tm.tm_min);
    } else {
        std::snprintf(buf, sizeof(buf), "%u 局",
                      static_cast<unsigned>(rec.rounds));
    }
    lv_obj_t* sub = lv_label_create(row);
    lv_label_set_text(sub, buf);
    lv_obj_set_style_text_font(sub, &font_tc_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(sub, colSubtle(), LV_PART_MAIN);
    lv_obj_align(sub, LV_ALIGN_LEFT_MID, 12, 10);
}

}  // namespace

void showHistory() {
    lv_obj_t* scr = makeScreen();
    // dy -145 時字框 y 21.5~48.5，與狀態晶片（-152，字框 18.5~37.5）重疊 16px，
    // 實機截圖上兩者疊在一起。移到 -132 後只剩 3px 的行高邊緣重疊，墨跡不會碰到；
    // 下方容器頂端在 y=70，仍有 8.5px 間隙，不必動容器。
    makeLabel(scr, "歷史紀錄", &font_tc_22, colAccent(), 0, -132);

    const int n = g_store.historyCount();
    if (n == 0) {
        makeLabel(scr, "尚無紀錄", &font_tc_22, colSubtle(), 0, 0);
    } else {
        lv_obj_t* cont = lv_obj_create(scr);
        lv_obj_set_size(cont, 230, 210);
        lv_obj_align(cont, LV_ALIGN_CENTER, 0, -5);
        lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(cont, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(cont, 2, LV_PART_MAIN);
        lv_obj_set_style_pad_row(cont, 6, LV_PART_MAIN);
        lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_scroll_dir(cont, LV_DIR_VER);
        lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_AUTO);

        for (int i = 0; i < n; ++i) {
            makeRecordRow(cont, g_store.history(i));
        }
    }

    makeButton(scr, "返回", 0, 128, 110, 44, colMuted(), onBack, nullptr,
               &font_tc_16);

    loadScreen(scr, Nav::Forward);
}

}  // namespace ui
}  // namespace bey
