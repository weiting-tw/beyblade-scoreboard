// 玩家名稱選擇（規格第 4 節：「可以讓使用者輸入或選擇」）
//
// 這裡走「選擇」而非「輸入」：360×360 圓形螢幕放不下可用的 QWERTY 鍵盤，
// 四個角會被切掉，按鍵也會小於規格第 9 節的觸控目標下限。
// 預設清單見 kPresets；要改成自由輸入請見 docs/DECISIONS.md。
#include <cstring>

#include "../app/app.h"
#include "ui.h"
#include "ui_theme.h"

namespace bey {
namespace ui {
namespace {

// 換行分隔，直接餵給 lv_roller。
constexpr const char* kPresetOptions =
    "P1\n"
    "P2\n"
    "玩家一\n"
    "玩家二\n"
    "WEITING\n"
    "OPPONENT\n"
    "RED\n"
    "BLUE\n"
    "DRAGOON\n"
    "DRANZER\n"
    "VALKYRIE\n"
    "GUEST";

lv_obj_t* s_roller1 = nullptr;
lv_obj_t* s_roller2 = nullptr;

void onScreenDel(lv_event_t*) {
    s_roller1 = nullptr;
    s_roller2 = nullptr;
}

void onCancel(lv_event_t*) { showReady(); }

void onOk(lv_event_t*) {
    if (s_roller1 == nullptr || s_roller2 == nullptr) {
        showReady();
        return;
    }
    AppSettings& s = g_store.mutableSettings();
    lv_roller_get_selected_str(s_roller1, s.match.p1Name, kNameLen);
    lv_roller_get_selected_str(s_roller2, s.match.p2Name, kNameLen);
    s.match.p1Name[kNameLen - 1] = '\0';
    s.match.p2Name[kNameLen - 1] = '\0';
    g_store.save();
    applySettingsToMatch();
    showReady();
}

// 把目前設定的名稱對回清單索引；不在清單中則回 0。
uint16_t indexOf(const char* name) {
    uint16_t idx = 0;
    const char* p = kPresetOptions;
    while (*p != '\0') {
        const char* end = std::strchr(p, '\n');
        const size_t len = (end != nullptr) ? static_cast<size_t>(end - p) : std::strlen(p);
        if (std::strlen(name) == len && std::strncmp(p, name, len) == 0) {
            return idx;
        }
        if (end == nullptr) {
            break;
        }
        p = end + 1;
        ++idx;
    }
    return 0;
}

lv_obj_t* makeRoller(lv_obj_t* parent, lv_coord_t dx, lv_color_t col,
                     const char* current) {
    lv_obj_t* r = lv_roller_create(parent);
    lv_roller_set_options(r, kPresetOptions, LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(r, 4);
    lv_obj_set_width(r, 130);
    lv_obj_align(r, LV_ALIGN_CENTER, dx, -15);
    lv_obj_set_style_bg_color(r, colMuted(), LV_PART_MAIN);
    lv_obj_set_style_border_width(r, 0, LV_PART_MAIN);
    lv_obj_set_style_text_font(r, &font_tc_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(r, colText(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(r, col, LV_PART_SELECTED);
    lv_obj_set_style_text_font(r, &font_tc_16, LV_PART_SELECTED);
    lv_roller_set_selected(r, indexOf(current), LV_ANIM_OFF);
    return r;
}

}  // namespace

void showNames() {
    setCurrentScreen(ScreenId::Names);
    const MatchConfig& cfg = g_store.settings().match;

    lv_obj_t* s = makeScreen();
    lv_obj_add_event_cb(s, onScreenDel, LV_EVENT_DELETE, nullptr);
    makeLabel(s, "玩家名稱", &font_tc_22, colAccent(), 0, -130);

    s_roller1 = makeRoller(s, -72, colP1(), cfg.p1Name);
    s_roller2 = makeRoller(s, 72, colP2(), cfg.p2Name);

    makeButton(s, "取消", -52, 105, 100, 48, colMuted(), onCancel, nullptr,
               &font_tc_16);
    makeButton(s, "確定", 52, 105, 100, 48, colP2(), onOk, nullptr);

    loadScreen(s, Nav::Forward);
}

}  // namespace ui
}  // namespace bey
