#include "evaluator.h"
#include "core/card/rank.h"
#include "../plugins/counter_risk.h"
#include <set>

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

    for (const Card& c : play) {
        int rank = getCardRank(c.point);
        if (rank >= 13) gain -= P.earlyBigPenalty;
        else if (rank >= 10) gain -= P.earlyMidPenalty;
        else gain -= rank;
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
        const std::set<std::string> specialPoints = {"7", "大鬼", "小鬼", "5", "2", "3"};
        int handSpecialCount = 0;
        for (const Card& c : player.hand)
            if (specialPoints.count(c.point)) handSpecialCount++;
        if (handSpecialCount >= 3) {
            for (const Card& c : play) {
                if (specialPoints.count(c.point))
                    gain -= P.specialKeepBonus;
            }
        }
    }

    {
        std::set<std::string> deduped;
        for (const Card& c : play) {
            if (deduped.count(c.point)) continue;
            deduped.insert(c.point);
            int inHand = 0;
            for (const Card& h : player.hand)
                if (h.point == c.point) inHand++;
            int inPlay = 0;
            for (const Card& pc : play)
                if (pc.point == c.point) inPlay++;
            if (inHand >= 2 && inPlay == 1)
                gain -= P.splitPairPenalty;
        }
    }

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