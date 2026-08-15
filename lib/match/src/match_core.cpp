#include "match_core.h"

#include <cstring>

namespace bey {
namespace {

void setRule(ScoreRule& r, const char* name, int8_t p1, int8_t p2,
             bool countsAsRound) {
    std::strncpy(r.name, name, sizeof(r.name) - 1);
    r.name[sizeof(r.name) - 1] = '\0';
    r.p1Points = p1;
    r.p2Points = p2;
    r.countsAsRound = countsAsRound;
}

void setName(char* dst, const char* src) {
    std::strncpy(dst, src, kNameLen - 1);
    dst[kNameLen - 1] = '\0';
}

}  // namespace

RuleSet defaultRuleSet() {
    RuleSet rs{};
    // name 會直接顯示在局結果頁。UTF-8 中文一字 3 bytes，24 bytes 的欄位
    // 足夠放「P1 爆裂勝」(3 + 9 = 12 bytes) 這種長度。
    setRule(rs.rules[static_cast<int>(ResultType::P1Normal)], "P1 普通勝", 1, 0, true);
    setRule(rs.rules[static_cast<int>(ResultType::P1Burst)], "P1 爆裂勝", 2, 0, true);
    setRule(rs.rules[static_cast<int>(ResultType::P1Xtreme)], "P1 Xtreme", 3, 0, true);
    setRule(rs.rules[static_cast<int>(ResultType::P2Normal)], "P2 普通勝", 0, 1, true);
    setRule(rs.rules[static_cast<int>(ResultType::P2Burst)], "P2 爆裂勝", 0, 2, true);
    setRule(rs.rules[static_cast<int>(ResultType::P2Xtreme)], "P2 Xtreme", 0, 3, true);
    // 雙方同時出界：本局計入局數但雙方皆不得分。
    setRule(rs.rules[static_cast<int>(ResultType::DoubleOut)], "雙方出界", 0, 0, true);
    // 無效局：不得分、也不推進局數。
    setRule(rs.rules[static_cast<int>(ResultType::NoContest)], "無效局", 0, 0, false);
    return rs;
}

MatchConfig defaultConfig() {
    MatchConfig c{};
    c.targetScore = 3;
    c.maxRounds = 0;
    c.enableSound = false;
    c.saveHistory = true;
    setName(c.p1Name, "P1");
    setName(c.p2Name, "P2");
    return c;
}

Match::Match() : cfg_(defaultConfig()), rules_(defaultRuleSet()) { reset(); }

void Match::configure(const MatchConfig& cfg, const RuleSet& rules) {
    cfg_ = cfg;
    // 目標分數為 0 或負數會讓比賽在開始瞬間就結束，夾成合法下限。
    if (cfg_.targetScore < 1) {
        cfg_.targetScore = 1;
    }
    if (cfg_.maxRounds < 0) {
        cfg_.maxRounds = 0;
    }
    cfg_.p1Name[kNameLen - 1] = '\0';
    cfg_.p2Name[kNameLen - 1] = '\0';
    rules_ = rules;
    reset();
}

void Match::reset() {
    p1_ = 0;
    p2_ = 0;
    round_ = 1;
    started_ = false;
    finished_ = false;
    winner_ = Winner::None;
    histCount_ = 0;
}

void Match::start() {
    if (finished_) {
        reset();
    }
    started_ = true;
}

void Match::pushHistory(const ScoreEvent& e) {
    if (histCount_ == kUndoDepth) {
        // 堆疊已滿，丟掉最舊的一筆。
        std::memmove(&hist_[0], &hist_[1], sizeof(ScoreEvent) * (kUndoDepth - 1));
        histCount_ = kUndoDepth - 1;
    }
    hist_[histCount_++] = e;
}

bool Match::applyResult(ResultType result) {
    if (!started_ || finished_) {
        return false;
    }
    const int idx = static_cast<int>(result);
    if (idx < 0 || idx >= kResultCount) {
        return false;
    }

    ScoreEvent ev{};
    ev.roundNumber = round_;
    ev.result = result;
    ev.prevP1 = p1_;
    ev.prevP2 = p2_;
    ev.prevRound = round_;
    ev.prevFinished = finished_;
    ev.prevWinner = winner_;
    pushHistory(ev);

    const ScoreRule& r = rules_.rules[idx];
    p1_ += r.p1Points;
    p2_ += r.p2Points;
    // 使用者可能把點數設成負值，分數不允許低於 0。
    if (p1_ < 0) {
        p1_ = 0;
    }
    if (p2_ < 0) {
        p2_ = 0;
    }
    if (r.countsAsRound) {
        ++round_;
    }

    evaluate();
    return true;
}

void Match::decideByScore() {
    if (p1_ > p2_) {
        winner_ = Winner::P1;
    } else if (p2_ > p1_) {
        winner_ = Winner::P2;
    } else {
        winner_ = Winner::Draw;
    }
}

void Match::evaluate() {
    if (p1_ >= cfg_.targetScore || p2_ >= cfg_.targetScore) {
        finished_ = true;
        decideByScore();
        return;
    }

    // maxRounds 為已完成的局數上限；round_ 指向「下一局」，
    // 所以打完第 maxRounds 局後 round_ 會是 maxRounds + 1。
    if (cfg_.maxRounds > 0 && round_ > cfg_.maxRounds) {
        finished_ = true;
        decideByScore();
    }
}

void Match::forceFinish() {
    if (!started_ || finished_) {
        return;
    }
    finished_ = true;
    decideByScore();
}

bool Match::undo() {
    if (histCount_ == 0) {
        return false;
    }
    const ScoreEvent& ev = hist_[--histCount_];
    p1_ = ev.prevP1;
    p2_ = ev.prevP2;
    round_ = ev.prevRound;
    finished_ = ev.prevFinished;
    winner_ = ev.prevWinner;
    return true;
}

const char* Match::winnerName() const {
    switch (winner_) {
        case Winner::P1:
            return cfg_.p1Name;
        case Winner::P2:
            return cfg_.p2Name;
        default:
            return nullptr;
    }
}

}  // namespace bey
