#pragma once

#include "core/card/card.h"
#include "core/card/cardtype.h"
#include "core/card/deck.h"
#include "core/card/rank.h"
#include "core/player.h"
#include "cardtracker.h"

inline constexpr int kCounterRiskWeight = 2;

inline int estimateCounterRisk(const std::vector<Card>& play,
                                const Player& player,
                                const Player& opponent,
                                const CardTracker* tracker,
                                const CardTypeResult& parsed,
                                int tableScore,
                                const Deck& deck,
                                DeckSide mySide = DeckSide::PlayerA) {
    (void)play;
    (void)player;
    if (!tracker) return 0;
    if (opponent.hand.empty()) return 0;
    if (parsed.type == CardType::Rocket) return 0;
    if (parsed.type == CardType::Special523) return 0;

    auto it = RANK_MAP.find(parsed.keyPoint);
    if (it == RANK_MAP.end()) return 0;
    int myRank = it->second;

    int oppBeatsCount = 0;
    for (const auto& kv : RANK_MAP) {
        if (kv.second < myRank) continue;
        double oppMightHave = tracker->expectedOpponentCount(kv.first,
                                   (int)opponent.hand.size(),
                                   (int)deck.cards.size(),
                                   mySide);
        if (oppMightHave > 0.5) oppBeatsCount++;
    }

    if (oppBeatsCount == 0) return 0;

    int weight = 1 + tableScore / 10;
    return oppBeatsCount * weight;
}