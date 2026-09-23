#include <iostream>
#include <algorithm>
#include <random>
#include <vector>
#include "core/card/card.h"
#include "core/card/cardtype.h"
#include "core/card/deck.h"
#include "core/player.h"
#include "ai/engine/ai_engine.h"
#include "ai/engine/ai_levels.h"

int main() {
    Deck full = createStandardDeck();
    std::mt19937 rng(42);

    for (int test = 0; test < 1000; test++) {
        std::vector<Card> pool = full.cards;
        std::shuffle(pool.begin(), pool.end(), rng);

        std::vector<Card> hand(pool.begin(), pool.begin() + 5);
        Player me;
        me.hand = hand;
        me.totalScore = 0;
        Player opp;
        opp.hand = {};
        opp.totalScore = 0;
        Deck deck;
        deck.cards = {};

        AIEngineConfig cfg = buildAIEngineConfig(6);
        AIEngine eng(cfg);

        CardTypeResult prev;
        prev.type = CardType::Invalid;

        auto play = eng.choosePlay(me, opp, prev, deck, 0);
        if (test % 100 == 0) {
            std::cout << "test " << test << ": ";
            for (auto& c : hand) std::cout << c.point << c.suit << " ";
            std::cout << "-> ";
            for (auto& c : play) std::cout << c.point << c.suit << " ";
            std::cout << "\n";
        }
    }
    std::cout << "1000 tests passed\n";
    return 0;
}