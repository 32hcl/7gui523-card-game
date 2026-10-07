#include "evaluator.h"
#include "ai/ai.h"
#include "core/card/rank.h"
#include "../plugins/counter_risk.h"
#include <algorithm>

int SimpleEvaluator::evaluate(const std::vector<Card>& play,
                               const Player& player,
                               const Player& /*opponent*/,
                               const Deck& /*deck*/,
                               const CardTypeResult& /*previous*/,
                               int tableScore,
                               const CardTracker* /*tracker*/,
                               DeckSide /*mySide*/) const {
    int gain = 0;
    gain += tableScore * 2;
    gain += (int)play.size() * 10;
    if (play.size() == player.hand.size()) gain += 1000;

    for (const Card& c : play) {
        int rank = getCardRank(c.point);
        if (rank >= 13) gain -= rank * 3;
        else if (rank >= 10) gain -= rank * 2;
        else gain -= rank;
    }

    for (const Card& c : play) {
        if (c.score > 0 && tableScore > 0) gain += c.score * 2;
    }

    return gain;
}

int SmartEvaluator::evaluate(const std::vector<Card>& play,
                              const Player& player,
                              const Player& opponent,
                              const Deck& deck,
                              const CardTypeResult& /*previous*/,
                              int tableScore,
                              const CardTracker* tracker,
                              DeckSide mySide) const {
    int gain = 0;
    gain += tableScore * 2;
    gain += (int)play.size() * 10;
    if (play.size() == player.hand.size()) gain += 1000;

    for (const Card& c : play) {
        int rank = getCardRank(c.point);
        if (rank >= 13) gain -= rank * 3;
        else if (rank >= 10) gain -= rank * 2;
        else gain -= rank;
    }

    for (const Card& c : play) {
        if (c.score > 0 && tableScore > 0) gain += c.score * 2;
    }

    if (tracker) {
        for (const Card& c : play) {
            if (tracker->isExhausted(c.point, mySide)) gain += 30;
        }
        auto parsed = parseCardType(play);
        int risk = estimateCounterRisk(play, player, opponent, tracker, parsed, tableScore, deck, mySide);
        gain -= risk * kCounterRiskWeight;
    }

    return gain;
}

AdvancedEvaluator::AdvancedEvaluator(const AIParams& params) : params_(params) {}

int AdvancedEvaluator::evaluate(const std::vector<Card>& play,
                                 const Player& player,
                                 const Player& opponent,
                                 const Deck& deck,
                                 const CardTypeResult& /*previous*/,
                                 int tableScore,
                                 const CardTracker* tracker,
                                 DeckSide mySide) const {
    const AIParams& P = params_;
    int gain = 0;
    gain += tableScore * P.tableScoreWeight;
    gain += (int)play.size() * P.cardCountWeight;
    if (play.size() == player.hand.size()) gain += P.finishBonus;

    // 前期判定：双牌堆模式每方牌堆初始27张，剩余过半(>=14)视为前期
    constexpr int kEarlyStageDeckRemaining = 14;
    const bool isEarlyStage = (int)deck.cards.size() >= kEarlyStageDeckRemaining;
    for (const Card& c : play) {
        // 基础成本：牌点越大越不愿早出（RANK_MAP 为七鬼523自定义序，7=15最大）
        gain -= getCardRank(c.point);
        // 分值牌(5/10/K，每张10分)前期打出等于白送分：
        //   10/K 牌点小、出了易被压，惩罚最重；5 牌点大(rank12)，惩罚次之
        // 注意：必须按 point 判定，不能用 rank 阈值——旧代码误用传统扑克
        //   rank>=13(=K/A) 阈值，在自定义 RANK_MAP 下实际命中 7/大鬼/小鬼
        if (isEarlyStage) {
            if (c.point == "10" || c.point == "K") gain -= P.earlyBigPenalty;
            else if (c.point == "5") gain -= P.earlyMidPenalty;
        }
    }

    for (const Card& c : play) {
        if (c.score > 0 && tableScore == 0) gain -= P.midScorePenalty;
    }

    if (tableScore >= P.stealThreshold) {
        for (const Card& c : play) {
            if (c.score > 0) gain += c.score * P.stealMultiplier;
        }
    } else {
        for (const Card& c : play) {
            if (c.score > 0) gain -= tableScore * P.noConfidencePenalty;
        }
    }

    if (tracker) {
        for (const Card& c : play) {
            if (tracker->isExhausted(c.point, mySide)) gain += P.deckTopBonus;
        }
    }

    {
        // 特殊胜利需凑齐5类关键牌：7 + 鬼(大鬼/小鬼任一) + 5 + 2 + 3。
        // 双鬼只算1类——旧实现按张计数，双鬼在手会被灌水成2类。
        const auto hasPoint = [&](const std::string& p) {
            return std::any_of(player.hand.begin(), player.hand.end(),
                               [&](const Card& c) { return c.point == p; });
        };
        const bool has7 = hasPoint("7");
        const bool hasJoker = hasPoint("大鬼") || hasPoint("小鬼");
        const bool has5 = hasPoint("5");
        const bool has2 = hasPoint("2");
        const bool has3 = hasPoint("3");
        const int handSpecialKinds =
            (int)has7 + (int)hasJoker + (int)has5 + (int)has2 + (int)has3;

        if (handSpecialKinds >= 3) {
            // 缺的关键牌在牌局中(双方均未打出)至少剩 specialMinRemaining 张，
            // 才有希望凑齐；否则不为不可能的特殊胜利支付保留代价
            const auto globalLeft = [&](const std::string& p) -> int {
                if (!tracker) return 99;  // 无tracker信息：按可凑齐处理
                return tracker->totalCount(p)
                     - tracker->playedCount(p, DeckSide::PlayerA)
                     - tracker->playedCount(p, DeckSide::Boss);
            };
            bool feasible = true;
            if (!has7) feasible = feasible && globalLeft("7") >= P.specialMinRemaining;
            if (!has5) feasible = feasible && globalLeft("5") >= P.specialMinRemaining;
            if (!has2) feasible = feasible && globalLeft("2") >= P.specialMinRemaining;
            if (!has3) feasible = feasible && globalLeft("3") >= P.specialMinRemaining;
            if (!hasJoker) {
                const int jokersLeft = globalLeft("大鬼") + globalLeft("小鬼");
                feasible = feasible && jokersLeft >= P.specialMinRemaining;
            }
            if (feasible) {
                for (const Card& c : play) {
                    if (c.point == "7" || c.point == "大鬼" || c.point == "小鬼" ||
                        c.point == "5" || c.point == "2" || c.point == "3") {
                        gain -= P.specialKeepBonus;
                    }
                }
            }
        }
    }

    // 拆对/拆三张/拆四张：统一复用 breaksCombo，与 AI2 硬编码评分保持同一判定
    if (breaksCombo(play, player)) gain -= P.splitPairPenalty;

    {
        auto parsed = parseCardType(play);
        if (parsed.type == CardType::Bomb) gain -= P.bombKeepPenalty;
        else if (parsed.type == CardType::Rocket) gain -= P.rocketKeepPenalty;
    }

    if ((int)opponent.hand.size() <= 2) {
        auto parsed = parseCardType(play);
        if (parsed.type == CardType::Bomb) gain += P.endgameBombBonus;
        else if (parsed.type == CardType::Rocket) gain += P.endgameRocketBonus;
    }

    if (tracker) {
        auto parsedRisk = parseCardType(play);
        int risk = estimateCounterRisk(play, player, opponent, tracker, parsedRisk, tableScore, deck, mySide);
        gain -= risk * kCounterRiskWeight;
    }

    return gain;
}