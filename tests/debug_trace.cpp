#include <iostream>
#include <algorithm>
#include <random>
#include <stdexcept>
#include "core/card/deck.h"
#include "core/player.h"
#include "core/card/cardtype.h"
#include "core/rule/score.h"
#include "ai/ai.h"
#include "ai/ai_types.h"
#include "ai/engine/ai_engine.h"
#include "ai/engine/ai_levels.h"

static int g_callDepth = 0;
static int g_callCount = 0;

// Declare advancePosition so we can wrap it
GamePosition advancePosition(const GamePosition& s, const std::vector<Card>& move);

// Wrap advancePosition to add depth tracing
static int g_advanceCalls = 0;
GamePosition traceAdvance(const GamePosition& s, const std::vector<Card>& move) {
    g_advanceCalls++;
    bool leading = s.lastPlay.type == CardType::Invalid;
    bool isPass = move.empty();
    if (isPass && leading) {
        std::cerr << "[CRASH] advancePosition #" << g_advanceCalls
                  << " myTurn=" << s.myTurn
                  << " myH=" << s.myHand.size()
                  << " oppH=" << s.oppHand.size()
                  << " deck=" << s.deckCards.size()
                  << " depth_guess=" << g_callDepth << std::endl;
        // Print my cards
        std::cerr << "  myHand: ";
        for (auto& c : s.myHand) std::cerr << c.point << c.suit << " ";
        std::cerr << std::endl;
        std::cerr << "  oppHand: ";
        for (auto& c : s.oppHand) std::cerr << c.point << c.suit << " ";
        std::cerr << std::endl;
    }
    return advancePosition(s, move);
}

// We can't easily wrap genLegalMoves from separate translation unit,
// but we can trace from the advancePosition side.

int main() {
    Deck deck = createStandardDeck();
    std::mt19937 rng(42);
    std::shuffle(deck.cards.begin(), deck.cards.end(), rng);

    std::vector<Card> handA, handB;
    for (int i = 0; i < 5; i++) handA.push_back(deck.cards.back()), deck.cards.pop_back();
    for (int i = 0; i < 5; i++) handB.push_back(deck.cards.back()), deck.cards.pop_back();

    Player pA, pB;
    pA.hand = handA; pA.totalScore = 0;
    pB.hand = handB; pB.totalScore = 0;

    std::cout << "A:";
    for (auto& c : pA.hand) std::cout << c.point << c.suit << " ";
    std::cout << "\nB:";
    for (auto& c : pB.hand) std::cout << c.point << c.suit << " ";
    std::cout << "\nDeck:" << deck.cards.size() << "\n";

    // First call: A leads (engine A, level 1)
    AIEngineConfig cfgA = buildAIEngineConfig(1);
    AIEngine engA(cfgA);

    CardTypeResult lp;
    lp.type = CardType::Invalid;

    // A plays leading
    auto playA = engA.choosePlay(pA, pB, lp, deck, 0);
    std::cout << "A plays: ";
    if (playA.empty()) std::cout << "PASS";
    else for (auto& c : playA) std::cout << c.point << c.suit << " ";
    std::cout << "\n";

    // Remove A's play from A's hand
    for (auto& c : playA) {
        auto it = std::find_if(pA.hand.begin(), pA.hand.end(),
            [&](const Card& h) { return h.point == c.point && h.suit == c.suit; });
        if (it != pA.hand.end()) pA.hand.erase(it);
    }
    lp = parseCardType(playA);

    // Refill both hands
    auto refill = [](std::vector<Card>& hand, Deck& d) {
        while (hand.size() < 5 && !d.cards.empty()) {
            hand.push_back(d.cards.back()); d.cards.pop_back();
        }
    };
    refill(pA.hand, deck);
    refill(pB.hand, deck);

    std::cout << "After A plays, B hand:";
    for (auto& c : pB.hand) std::cout << c.point << c.suit << " ";
    std::cout << "\nA hand:";
    for (auto& c : pA.hand) std::cout << c.point << c.suit << " ";
    std::cout << "\nDeck:" << deck.cards.size() << "\n";

    // Second call: B responds (engine B, level 6 minimax)
    AIEngineConfig cfgB = buildAIEngineConfig(6);
    AIEngine engB(cfgB);

    int tableScore = 0;
    std::cout << "TableScore:" << tableScore << "\n";

    // Use engB on SAME state as debug_l6 T1
    std::cout << "=== Calling engB.choosePlay (responding to " << lp.type << ") ===" << std::endl;
    try {
        auto playB = engB.choosePlay(pB, pA, lp, deck, tableScore);
        std::cout << "B plays: ";
        if (playB.empty()) std::cout << "PASS";
        else for (auto& c : playB) std::cout << c.point << c.suit << " ";
        std::cout << "\n";
    } catch (std::exception& e) {
        std::cout << "EXCEPTION: " << e.what() << "\n";
    } catch (...) {
        std::cout << "UNKNOWN EXCEPTION\n";
    }
    std::cout << "advancePosition calls: " << g_advanceCalls << "\n";

    return 0;
}