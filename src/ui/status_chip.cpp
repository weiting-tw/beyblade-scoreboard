#include "status_chip.h"

#include <cstdio>

#include "../app/power.h"
#include "../app/settings_store.h"
#include "ui_theme.h"

namespace bey {
namespace ui {
namespace {

lv_obj_t* s_chip = nullptr;
bool s_hiddenRequested = false;

// 30 秒。電量本來就變得慢，讀太勤只是白佔 I2C 匯流排 ——
// 觸控每 30ms 就要輪詢一次，兩者共用同一條線。
constexpr uint32_t kPollMs = 30000;

// 頂部弧帶。字高 19，上緣落在 -161，半寬約 20：
// maxR = sqrt(20^2 + 161^2) = 162.2 < kSafeR(166)。
constexpr lv_coord_t kChipY = -152;

void applyVisibility() {
    if (s_chip == nullptr) {
        return;
    }
    // 低電量時無視隱藏要求 —— 這正是使用者需要看到它的時候。
    const bool hide = s_hiddenRequested && !power::low();
    if (hide) {
        lv_obj_add_flag(s_chip, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(s_chip, LV_OBJ_FLAG_HIDDEN);
    }
}

void refresh() {
    if (s_chip == nullptr) {
        return;
    }

    const BatteryState& b = power::state();
    char buf[16];
    // 用純數字加 '%'，不用 LV_SYMBOL_BATTERY_* —— 那些符號在 montserrat 的
    // 0xF000 區，而嵌入的是漢字子集字型，只含 ASCII 加漢字，符號會變方塊。
    // 外部供電中前綴 '+'。
    std::snprintf(buf, sizeof(buf), "%s%u%%", b.external ? "+" : "", b.percent);
    lv_label_set_text(s_chip, buf);
    lv_obj_set_style_text_color(s_chip, power::low() ? colDanger() : colSubtle(),
                                LV_PART_MAIN);
    applyVisibility();
}

void onTick(lv_timer_t*) {
    power::poll();
    refresh();
}

}  // namespace

void statusChipInit() {
    if (!power::available() || !g_store.settings().enableBatteryBadge) {
        return;
    }

    s_chip = lv_label_create(lv_layer_top());
    lv_obj_set_style_text_font(s_chip, &font_tc_16, LV_PART_MAIN);
    lv_obj_align(s_chip, LV_ALIGN_CENTER, 0, kChipY);
    // 純顯示，不吃觸控 —— 否則會擋掉底下畫面頂端的操作。
    lv_obj_clear_flag(s_chip, LV_OBJ_FLAG_CLICKABLE);

    refresh();
    lv_timer_create(onTick, kPollMs, nullptr);
}

void statusChipSetHidden(bool hidden) {
    s_hiddenRequested = hidden;
    applyVisibility();
}

}  // namespace ui
}  // namespace bey
