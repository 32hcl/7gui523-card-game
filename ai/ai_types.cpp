#include "ai_types.h"

std::vector<int> AIParams::toVector() const {
    return {
        cardCountWeight, finishBonus,
        tableScoreWeight,
        earlyBigPenalty, earlyMidPenalty,
        midScorePenalty,
        stealThreshold, stealMultiplier, noConfidencePenalty,
        deckTopBonus, deckTopSpecialPenalty,
        splitPairPenalty,
        specialKeepBonus, specialMinRemaining,
        bombKeepPenalty, rocketKeepPenalty,
        endgameBombBonus, endgameRocketBonus
    };
}

AIParams AIParams::fromVector(const std::vector<int>& v) {
    AIParams p;
    /* 修改：用 paramCount() 替代硬编码18，防止新增字段后遗漏更新 */
    if (v.size() < (size_t)paramCount()) return p;
    size_t i = 0;
    p.cardCountWeight       = v[i++];
    p.finishBonus           = v[i++];
    p.tableScoreWeight      = v[i++];
    p.earlyBigPenalty       = v[i++];
    p.earlyMidPenalty       = v[i++];
    p.midScorePenalty       = v[i++];
    p.stealThreshold        = v[i++];
    p.stealMultiplier       = v[i++];
    p.noConfidencePenalty   = v[i++];
    p.deckTopBonus          = v[i++];
    p.deckTopSpecialPenalty = v[i++];
    p.splitPairPenalty      = v[i++];
    p.specialKeepBonus      = v[i++];
    p.specialMinRemaining   = v[i++];
    p.bombKeepPenalty       = v[i++];
    p.rocketKeepPenalty     = v[i++];
    p.endgameBombBonus      = v[i++];
    p.endgameRocketBonus    = v[i++];
    return p;
}

int AIParams::paramCount() { return 18; }