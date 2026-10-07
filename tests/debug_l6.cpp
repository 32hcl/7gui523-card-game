#include <iostream>
#include <algorithm>
#include <random>
#include <stdexcept>
#include "core/card/deck.h"
#include "core/player.h"
#include "core/card/cardtype.h"
#include "core/rule/score.h"
#include "core/rule/special.h"
#include "ai/plugins/cardtracker.h"
#include "ai/engine/ai_engine.h"
#include "ai/ai_levels.h"

int main() {
    int seed = 109;
    std::cout << "seed=" << seed << std::flush;
    std::mt19937 rng(seed);
    Deck deck = createStandardDeck();
    std::shuffle(deck.cards.begin(), deck.cards.end(), rng);

    Player pA = createPlayer("A");
    Player pB = createPlayer("B");
    dealCards(pA, deck, 5);
    dealCards(pB, deck, 5);

    std::cout << " A:" << std::flush;
    for (auto& c : pA.hand) std::cout << c.point << c.suit << " " << std::flush;
    std::cout << "B:" << std::flush;
    for (auto& c : pB.hand) std::cout << c.point << c.suit << " " << std::flush;
    std::cout << "deck:" << deck.cards.size() << std::flush;

    AIEngine engA(buildAIEngineConfig(1));
    AIEngine engB(buildAIEngineConfig(6));
    engA.setOpponentHand(pB.hand);
    engB.setOpponentHand(pA.hand);

    std::cout << " eng_ok" << std::flush;

    Player* cur = &pA;
    Player* opp = &pB;
    Player* lastP = nullptr;
    std::vector<Card> table;
    CardTypeResult lp = CardTypeResult{};
    CardTracker tracker;

    for (int turn = 0; turn < 100; turn++) {
        std::cout << " T" << turn << "(" << (cur == &pA ? "A" : "B") << ")" << std::flush;
        int ts = calculateScore(table);
        std::vector<Card> play;
        if (cur == &pA) play = engA.choosePlay(*cur, *opp, lp, deck, ts);
        else {
            std::cout << "(B_hand:" << std::flush;
            for (auto&c:cur->hand) std::cout<<c.point<<c.suit<<std::flush;
            std::cout<<")"<<std::flush;
            // Test: try calling the searcher directly
            std::cout << "calling..." << std::flush;
            try {
                play = engB.choosePlay(*cur, *opp, lp, deck, ts);
                std::cout << "ok" << std::flush;
            } catch (std::exception& e) {
                std::cout << "EXCEPTION:" << e.what() << std::flush;
            } catch (...) {
                std::cout << "UNKNOWN_EXCEPTION" << std::flush;
            }
        }

        if (play.empty()) {
            std::cout << "p" << std::flush;
            if (lp.type == CardType::Invalid) {
                if (!deck.cards.empty()) {
                    refillToFive(*cur, deck);
                    refillToFive(*opp, deck);
                }
                std::swap(cur, opp);
                if (deck.cards.empty() && pA.hand.empty() && pB.hand.empty()) break;
            } else {
                settleScoreCards(*lastP, table);
                table.clear();
            }
            lp = CardTypeResult{};
            continue;
        }

        // Print played cards
        std::cout << "[" << std::flush;
        for (auto& c : play) std::cout << c.point << c.suit << std::flush;
        std::cout << "]" << std::flush;

        auto parsed = parseCardType(play);
        if (parsed.type == CardType::Invalid) { std::cout << " BADTYPE"; break; }
        if (lp.type != CardType::Invalid && !canBeat(parsed, lp)) { std::cout << " BADBEAT"; break; }

        for (auto& c : play) {
            auto it = std::find_if(cur->hand.begin(), cur->hand.end(),
                [&](const Card& h) { return h.point == c.point && h.suit == c.suit; });
            if (it != cur->hand.end()) cur->hand.erase(it);
        }
        for (auto& c : play) table.push_back(c);
        tracker.recordPlayed(play);
        lp = parsed;
        lastP = cur;

        if (checkSpecialVictory(*cur)) { std::cout << " SPCL"; break; }

        if (cur->hand.empty() && deck.cards.empty()) {
            settleScoreCards(*cur, table);
            settleScoreCards(*cur, opp->hand);
            opp->hand.clear();
            break;
        }

        std::swap(cur, opp);
    }

    std::cout << " DONE A=" << pA.totalScore << " B=" << pB.totalScore << "\n";
    return 0;
}