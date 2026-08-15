// 計分核心的主機端單元測試：pio test -e native
//
// 這一層不需要板子，因此規格第 5、7 節的計分與撤銷行為可以在硬體到位前就鎖死。
#include <unity.h>

#include <cstdio>

#include "match_core.h"

using namespace bey;

namespace {

Match makeMatch(int targetScore = 3, int maxRounds = 0) {
    Match m;
    MatchConfig cfg = defaultConfig();
    cfg.targetScore = targetScore;
    cfg.maxRounds = maxRounds;
    m.configure(cfg, defaultRuleSet());
    m.start();
    return m;
}

}  // namespace

void setUp(void) {}
void tearDown(void) {}

// --- 預設值 --------------------------------------------------------------

void test_default_config_is_four_point_match(void) {
    // Beyblade X 常規賽制是 4 分制，改這個預設要記得同步升 kSettingsMagic，
    // 否則 NVS 裡的舊值會蓋過新預設。
    const MatchConfig c = defaultConfig();
    TEST_ASSERT_EQUAL_INT(4, c.targetScore);
    TEST_ASSERT_EQUAL_INT(0, c.maxRounds);
    TEST_ASSERT_EQUAL_STRING("P1", c.p1Name);
    TEST_ASSERT_EQUAL_STRING("P2", c.p2Name);
}

void test_default_rules_are_1_2_3(void) {
    const RuleSet rs = defaultRuleSet();
    TEST_ASSERT_EQUAL_INT(1, rs.rules[(int)ResultType::P1Normal].p1Points);
    TEST_ASSERT_EQUAL_INT(2, rs.rules[(int)ResultType::P1Burst].p1Points);
    TEST_ASSERT_EQUAL_INT(3, rs.rules[(int)ResultType::P1Xtreme].p1Points);
    TEST_ASSERT_EQUAL_INT(1, rs.rules[(int)ResultType::P2Normal].p2Points);
    TEST_ASSERT_EQUAL_INT(2, rs.rules[(int)ResultType::P2Burst].p2Points);
    TEST_ASSERT_EQUAL_INT(3, rs.rules[(int)ResultType::P2Xtreme].p2Points);
}

void test_fresh_match_is_zeroed(void) {
    Match m;
    TEST_ASSERT_EQUAL_INT(0, m.scoreP1());
    TEST_ASSERT_EQUAL_INT(0, m.scoreP2());
    TEST_ASSERT_EQUAL_INT(1, m.round());
    TEST_ASSERT_FALSE(m.started());
    TEST_ASSERT_FALSE(m.finished());
}

// --- 計分 ----------------------------------------------------------------

void test_cannot_score_before_start(void) {
    Match m;  // 尚未 start()
    TEST_ASSERT_FALSE(m.applyResult(ResultType::P1Normal));
    TEST_ASSERT_EQUAL_INT(0, m.scoreP1());
}

void test_normal_win_adds_one_and_advances_round(void) {
    Match m = makeMatch();
    TEST_ASSERT_TRUE(m.applyResult(ResultType::P1Normal));
    TEST_ASSERT_EQUAL_INT(1, m.scoreP1());
    TEST_ASSERT_EQUAL_INT(0, m.scoreP2());
    TEST_ASSERT_EQUAL_INT(2, m.round());
    TEST_ASSERT_FALSE(m.finished());
}

void test_xtreme_finish_reaches_target_in_one_round(void) {
    Match m = makeMatch(3);
    m.applyResult(ResultType::P1Xtreme);  // +3，一擊達標
    TEST_ASSERT_EQUAL_INT(3, m.scoreP1());
    TEST_ASSERT_TRUE(m.finished());
    TEST_ASSERT_EQUAL_INT((int)Winner::P1, (int)m.winner());
    TEST_ASSERT_EQUAL_STRING("P1", m.winnerName());
}

void test_burst_then_normal_wins_three_point_match(void) {
    Match m = makeMatch(3);
    m.applyResult(ResultType::P2Burst);   // P2 +2
    TEST_ASSERT_FALSE(m.finished());
    m.applyResult(ResultType::P2Normal);  // P2 +1 -> 3
    TEST_ASSERT_TRUE(m.finished());
    TEST_ASSERT_EQUAL_INT((int)Winner::P2, (int)m.winner());
}

void test_score_after_finish_is_rejected(void) {
    Match m = makeMatch(3);
    m.applyResult(ResultType::P1Xtreme);
    TEST_ASSERT_TRUE(m.finished());
    TEST_ASSERT_FALSE(m.applyResult(ResultType::P2Normal));
    TEST_ASSERT_EQUAL_INT(0, m.scoreP2());
}

void test_double_out_counts_round_but_no_points(void) {
    Match m = makeMatch();
    m.applyResult(ResultType::DoubleOut);
    TEST_ASSERT_EQUAL_INT(0, m.scoreP1());
    TEST_ASSERT_EQUAL_INT(0, m.scoreP2());
    TEST_ASSERT_EQUAL_INT(2, m.round());  // 局數有推進
}

void test_no_contest_does_not_advance_round(void) {
    Match m = makeMatch();
    m.applyResult(ResultType::NoContest);
    TEST_ASSERT_EQUAL_INT(0, m.scoreP1());
    TEST_ASSERT_EQUAL_INT(1, m.round());  // 局數不動
}

// --- 撤銷（規格第 7 節）--------------------------------------------------

void test_undo_restores_previous_score(void) {
    Match m = makeMatch();
    m.applyResult(ResultType::P1Normal);   // 1:0
    m.applyResult(ResultType::P1Xtreme);   // 4:0（已達標）
    TEST_ASSERT_TRUE(m.finished());

    TEST_ASSERT_TRUE(m.undo());
    TEST_ASSERT_EQUAL_INT(1, m.scoreP1());
    TEST_ASSERT_EQUAL_INT(2, m.round());
    TEST_ASSERT_FALSE(m.finished());      // 撤銷也要解除結束狀態
    TEST_ASSERT_EQUAL_INT((int)Winner::None, (int)m.winner());
}

void test_undo_on_empty_history_returns_false(void) {
    Match m = makeMatch();
    TEST_ASSERT_FALSE(m.undo());
}

void test_undo_supports_at_least_five_steps(void) {
    Match m = makeMatch(99);  // 目標拉高，避免中途結束
    for (int i = 0; i < 5; ++i) {
        m.applyResult(ResultType::P1Normal);
    }
    TEST_ASSERT_EQUAL_INT(5, m.scoreP1());
    for (int i = 0; i < 5; ++i) {
        TEST_ASSERT_TRUE(m.undo());
    }
    TEST_ASSERT_EQUAL_INT(0, m.scoreP1());
    TEST_ASSERT_EQUAL_INT(1, m.round());
    TEST_ASSERT_FALSE(m.undo());
}

void test_undo_stack_drops_oldest_when_full(void) {
    Match m = makeMatch(99);
    const int n = kUndoDepth + 3;
    for (int i = 0; i < n; ++i) {
        m.applyResult(ResultType::P1Normal);
    }
    TEST_ASSERT_EQUAL_INT(n, m.scoreP1());
    TEST_ASSERT_EQUAL_INT(kUndoDepth, m.undoAvailable());

    for (int i = 0; i < kUndoDepth; ++i) {
        TEST_ASSERT_TRUE(m.undo());
    }
    // 最舊的 3 筆已被丟棄，分數只能退回到 3 分
    TEST_ASSERT_EQUAL_INT(3, m.scoreP1());
    TEST_ASSERT_FALSE(m.undo());
}

// --- 提前結束與最近事件 --------------------------------------------------

void test_force_finish_decides_by_score(void) {
    Match m = makeMatch(99);
    m.applyResult(ResultType::P1Burst);  // 2:0
    TEST_ASSERT_FALSE(m.finished());
    m.forceFinish();
    TEST_ASSERT_TRUE(m.finished());
    TEST_ASSERT_EQUAL_INT((int)Winner::P1, (int)m.winner());
}

void test_force_finish_draw_when_level(void) {
    Match m = makeMatch(99);
    m.applyResult(ResultType::P1Normal);
    m.applyResult(ResultType::P2Normal);
    m.forceFinish();
    TEST_ASSERT_TRUE(m.finished());
    TEST_ASSERT_EQUAL_INT((int)Winner::Draw, (int)m.winner());
}

void test_force_finish_before_start_is_noop(void) {
    Match m;  // 未 start()
    m.forceFinish();
    TEST_ASSERT_FALSE(m.finished());
}

void test_last_event_reports_latest_scoring(void) {
    Match m = makeMatch(99);
    TEST_ASSERT_NULL(m.lastEvent());

    m.applyResult(ResultType::P2Xtreme);
    const ScoreEvent* ev = m.lastEvent();
    TEST_ASSERT_NOT_NULL(ev);
    TEST_ASSERT_EQUAL_INT((int)ResultType::P2Xtreme, (int)ev->result);
    TEST_ASSERT_EQUAL_INT(1, ev->roundNumber);
    TEST_ASSERT_EQUAL_INT(0, ev->prevP2);

    // 撤銷後最近事件也要跟著退回。
    m.undo();
    TEST_ASSERT_NULL(m.lastEvent());
}

// --- 賽制設定 ------------------------------------------------------------

void test_custom_rules_are_honoured(void) {
    Match m;
    RuleSet rs = defaultRuleSet();
    rs.rules[(int)ResultType::P1Normal].p1Points = 5;  // 使用者自訂
    MatchConfig cfg = defaultConfig();
    cfg.targetScore = 5;
    m.configure(cfg, rs);
    m.start();

    m.applyResult(ResultType::P1Normal);
    TEST_ASSERT_EQUAL_INT(5, m.scoreP1());
    TEST_ASSERT_TRUE(m.finished());
}

void test_invalid_target_score_is_clamped(void) {
    Match m;
    MatchConfig cfg = defaultConfig();
    cfg.targetScore = 0;  // 非法值
    m.configure(cfg, defaultRuleSet());
    TEST_ASSERT_EQUAL_INT(1, m.config().targetScore);
}

void test_max_rounds_ends_match_on_points(void) {
    Match m = makeMatch(99, 3);  // 目標分數極高，只靠局數上限結束
    m.applyResult(ResultType::P1Normal);  // round -> 2
    m.applyResult(ResultType::P2Burst);   // round -> 3
    TEST_ASSERT_FALSE(m.finished());
    m.applyResult(ResultType::P1Normal);  // round -> 4 > 3，結束
    TEST_ASSERT_TRUE(m.finished());
    // P1 2 : 2 P2 -> 平手
    TEST_ASSERT_EQUAL_INT(2, m.scoreP1());
    TEST_ASSERT_EQUAL_INT(2, m.scoreP2());
    TEST_ASSERT_EQUAL_INT((int)Winner::Draw, (int)m.winner());
    TEST_ASSERT_NULL(m.winnerName());
}

void test_custom_player_names(void) {
    Match m;
    MatchConfig cfg = defaultConfig();
    // 明確指定 3 分制，讓下方一記 Xtreme(+3) 就結束比賽。
    // 原本這裡依賴「預設就是 3 分」，預設改成 4 分後整個測試就失效了 ——
    // 這個測試要驗的是名稱，不該綁在賽制預設值上。
    cfg.targetScore = 3;
    std::snprintf(cfg.p1Name, kNameLen, "Weiting");
    std::snprintf(cfg.p2Name, kNameLen, "Opponent");
    m.configure(cfg, defaultRuleSet());
    m.start();
    m.applyResult(ResultType::P1Xtreme);
    TEST_ASSERT_EQUAL_STRING("Weiting", m.winnerName());
}

void test_reset_clears_everything(void) {
    Match m = makeMatch();
    m.applyResult(ResultType::P1Xtreme);
    m.reset();
    TEST_ASSERT_EQUAL_INT(0, m.scoreP1());
    TEST_ASSERT_EQUAL_INT(1, m.round());
    TEST_ASSERT_FALSE(m.started());
    TEST_ASSERT_FALSE(m.finished());
    TEST_ASSERT_EQUAL_INT(0, m.undoAvailable());
}

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(test_default_config_is_four_point_match);
    RUN_TEST(test_default_rules_are_1_2_3);
    RUN_TEST(test_fresh_match_is_zeroed);

    RUN_TEST(test_cannot_score_before_start);
    RUN_TEST(test_normal_win_adds_one_and_advances_round);
    RUN_TEST(test_xtreme_finish_reaches_target_in_one_round);
    RUN_TEST(test_burst_then_normal_wins_three_point_match);
    RUN_TEST(test_score_after_finish_is_rejected);
    RUN_TEST(test_double_out_counts_round_but_no_points);
    RUN_TEST(test_no_contest_does_not_advance_round);

    RUN_TEST(test_undo_restores_previous_score);
    RUN_TEST(test_undo_on_empty_history_returns_false);
    RUN_TEST(test_undo_supports_at_least_five_steps);
    RUN_TEST(test_undo_stack_drops_oldest_when_full);

    RUN_TEST(test_force_finish_decides_by_score);
    RUN_TEST(test_force_finish_draw_when_level);
    RUN_TEST(test_force_finish_before_start_is_noop);
    RUN_TEST(test_last_event_reports_latest_scoring);

    RUN_TEST(test_custom_rules_are_honoured);
    RUN_TEST(test_invalid_target_score_is_clamped);
    RUN_TEST(test_max_rounds_ends_match_on_points);
    RUN_TEST(test_custom_player_names);
    RUN_TEST(test_reset_clears_everything);

    return UNITY_END();
}
