#include <iostream>
#include <algorithm>
#include <vector>
#include "core/card/card.h"
#include "core/card/cardtype.h"
#include "core/card/deck.h"
#include "core/player.h"
#include "ai/engine/ai_engine.h"
#include "ai/ai_levels.h"

int main() {
    Deck deck;
    deck.cards = {};

    std::vector<Card> bHand = {
        Card{"大鬼", "", 0},
        Card{"6", "红桃", 0},
        Card{"5", "黑桃", 5},
        Card{"3", "方块", 0},
        Card{"小鬼", "", 0}
    };
    std::vector<Card> aHand = {
        Card{"10", "方块", 0},
        Card{"6", "黑桃", 0},
        Card{"9", "黑桃", 0},
        Card{"8", "梅花", 0}
    };

    Player pB; pB.hand = bHand; pB.totalScore = 0;
    Player pA; pA.hand = aHand; pA.totalScore = 0;

    AIEngineConfig cfg = buildAIEngineConfig(6);
    AIEngine eng(cfg);
    eng.setOpponentHand(pA.hand);

    CardTypeResult prev;
    prev.type = CardType::Invalid;

    // Leading with deck empty
    std::cout << "Test 1: deck empty, leading" << std::endl;
    Deck d1; d1.cards = {};
    try {
        auto play = eng.choosePlay(pB, pA, prev, d1, 0);
        std::cout << "  => ok, play=";
        if (play.empty()) std::cout << "(pass)";
        else for (auto& c : play) std::cout << c.point << c.suit << " ";
        std::cout << std::endl;
    } catch (std::exception& e) {
        std::cout << "  => EXCEPTION: " << e.what() << std::endl;
    }

    // Leading with 44-card deck
    std::cout << "Test 2: deck 44 cards, leading" << std::endl;
    Deck d2 = createStandardDeck();
    // Remove cards already in hands
    auto removeCard = [&](const Card& c) {
        auto it = std::find_if(d2.cards.begin(), d2.cards.end(),
            [&](const Card& d) { return d.point == c.point && d.suit == c.suit; });
        if (it != d2.cards.end()) d2.cards.erase(it);
    };
    for (auto& c : bHand) removeCard(c);
    for (auto& c : aHand) removeCard(c);
    std::cout << "deck=" << d2.cards.size() << std::endl;

    try {
        auto play = eng.choosePlay(pB, pA, prev, d2, 0);
        std::cout << "  => ok, play=";
        if (play.empty()) std::cout << "(pass)";
        else for (auto& c : play) std::cout << c.point << c.suit << " ";
        std::cout << std::endl;
    } catch (std::exception& e) {
        std::cout << "  => EXCEPTION: " << e.what() << std::endl;
    }

    // Leading with 1-card deck
    std::cout << "Test 3: deck 1 card, leading" << std::endl;
    Deck d3;
    d3.cards = { Card{"4", "黑桃", 0} };
    try {
        auto play = eng.choosePlay(pB, pA, prev, d3, 0);
        std::cout << "  => ok, play=";
        if (play.empty()) std::cout << "(pass)";
        else for (auto& c : play) std::cout << c.point << c.suit << " ";
        std::cout << std::endl;
    } catch (std::exception& e) {
        std::cout << "  => EXCEPTION: " << e.what() << std::endl;
    }

    std::cout << "Done\n";
    return 0;
}