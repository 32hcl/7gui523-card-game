#include <iostream>
#include <algorithm>
#include <vector>
#include "core/card/card.h"
#include "core/card/cardtype.h"
#include "core/player.h"
#include "core/card/deck.h"
#include "ai/engine/ai_engine.h"
#include "ai/engine/ai_levels.h"

int main() {
    for (int depth = 6; depth >= 1; depth--) {
        std::vector<Card> hand = {
            Card{"大鬼", "", 0},
            Card{"6", "红桃", 0},
            Card{"5", "黑桃", 5},
            Card{"3", "方块", 0},
            Card{"小鬼", "", 0}
        };
        Player me;
        me.hand = hand;
        me.totalScore = 0;
        Player opp;
        opp.hand = {};
        opp.totalScore = 0;
        Deck deck;
        deck.cards = {};

        AIEngineConfig cfg = buildAIEngineConfig(6);
        cfg.searchDepth = depth;
        AIEngine eng(cfg);

        CardTypeResult prev;
        prev.type = CardType::Invalid;

        std::cout << "depth=" << depth << " ... " << std::flush;
        auto play = eng.choosePlay(me, opp, prev, deck, 0);
        std::cout << " ok, play=";
        for (auto& c : play) std::cout << c.point << c.suit << " ";
        std::cout << "\n";
    }
    std::cout << "All depths ok\n";
    return 0;
}