// 頁面六：局結果（規格第 6 節）
//
// 規格原本列的三顆按鈕是 [下一局] [返回計分] [結束比賽]。分數在進入本頁前
// 就已經套用完畢，因此「下一局」與「返回計分」會是完全相同的動作。
// 中間那顆改成 UNDO —— 剛按錯勝利方式時，這是唯一真正需要的出口。
// 詳見 docs/DECISIONS.md。
#include <cstdio>

#include "../app/app.h"
#include "../app/feedback.h"
#include "status_chip.h"
#include "ui.h"
#include "ui_theme.h"

namespace bey {
namespace ui {
namespace {

void onNext(lv_event_t*) { showScore(Nav::Forward); }

void onUndo(lv_event_t*) {
    if (g_match.undo()) {
        feedbackPlay(Sfx::Undo);
        feedbackHaptic(25);
    }
    showScore();
}

void doEnd() {
    // 提前結束：分數不變，依目前比分判勝負後記錄結果。
    g_match.forceFinish();
    recordFinishedMatch();
    showComplete();
}

void onEnd(lv_event_t*) { showConfirmOverlay("結束比賽？", doEnd); }

}  // namespace

void showRound() {
    lv_obj_t* s = makeScreen();

    const ScoreEvent* ev = g_match.lastEvent();

    char buf[48];
    std::snprintf(buf, sizeof(buf), "第 %d 局",
                  ev != nullptr ? ev->roundNumber : g_match.round());
    makeLabel(s, buf, &font_tc_22, colSubtle(), 0, -130);

    if (ev != nullptr) {
        const ScoreRule& rule = g_match.rule(ev->result);
        const bool p1Scored = rule.p1Points > rule.p2Points;
        const bool p2Scored = rule.p2Points > rule.p1Points;
        const lv_color_t col =
            p1Scored ? colP1() : (p2Scored ? colP2() : colSubtle());

        // rule.name 形如「P1 爆裂勝」；直接顯示即可，設定頁不允許改名。
        // Xtreme Finish 是本作最高分的收尾方式，值得一個專屬的金色光環。
        const bool isXtreme = (ev->result == ResultType::P1Xtreme ||
                               ev->result == ResultType::P2Xtreme);
        if (isXtreme) {
            animRingBurst(s, colAccent(), 1500);
        }

        lv_obj_t* t = makeLabel(s, rule.name, &font_tc_30, col, 0, -80);
        lv_obj_set_width(t, 260);
        lv_label_set_long_mode(t, LV_LABEL_LONG_DOT);
        lv_obj_align(t, LV_ALIGN_CENTER, 0, -80);
        animFadeIn(t, 260);

        const int pts = p1Scored ? rule.p1Points : rule.p2Points;
        if (pts > 0) {
            std::snprintf(buf, sizeof(buf), "+%d", pts);
        } else {
            std::snprintf(buf, sizeof(buf), "無得分");
        }
        lv_obj_t* p = makeLabel(s, buf, &font_tc_30, colAccent(), 0, -35);
        // 得分越高彈得越誇張：+1 幾乎不動，Xtreme 的 +3 明顯放大。
        animPop(p, static_cast<uint16_t>(pts >= 3 ? 60 : (pts >= 2 ? 120 : 180)),
                256, 420, 180);
    }

    std::snprintf(buf, sizeof(buf), "%d : %d", g_match.scoreP1(),
                  g_match.scoreP2());
    makeLabel(s, buf, &lv_font_montserrat_48, colText(), 0, 25);

    makeButton(s, "撤銷", -78, 100, 72, 48, colMuted(), onUndo, nullptr,
               &font_tc_16);
    makeButton(s, "下一局", 0, 100, 72, 48, colP2(), onNext, nullptr,
               &font_tc_16);
    makeButton(s, "結束", 78, 100, 72, 48, colDanger(), onEnd, nullptr,
               &font_tc_16);

    loadScreen(s, Nav::Forward);
    // 頂部弧帶留給這頁自己的內容，把狀態晶片收起來。
    // 低電量時 statusChipSetHidden 會忽略這個要求。
    statusChipSetHidden(true);
}

}  // namespace ui
}  // namespace bey
