#include <iostream>
#include <random>
#include "core/card/deck.h"
#include "core/player.h"
#include "core/rule/score.h"
#include "ai/ai.h"
#include "ai/searcher/minimax.h"
#include "ai/engine/ai_engine.h"
#include "ai/engine/ai_levels.h"

static int getCardTypeInt(const std::vector<Card>& play) {
    if (play.empty()) return -1;
    return (int)parseCardType(play).type;
}

static std::string playStr(const std::vector<Card>& play) {
    if (play.empty()) return "PASS";
    std::string s;
    for (auto& c : play) s += c.point + c.suit + " ";
    return s;
}

static bool playEq(const std::vector<Card>& a, const std::vector<Card>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i].point != b[i].point || a[i].suit != b[i].suit) return false;
    }
    return true;
}

int main() {
    std::mt19937 rng(42);
    int matchCount = 0, total = 0;

    for (int g = 0; g < 10; ++g) {
        Deck deck = createStandardDeck();
        std::shuffle(deck.cards.begin(), deck.cards.end(), rng);

        Player p1 = createPlayer("P1");
        Player p2 = createPlayer("P2");
        dealCards(p1, deck, 5);
        dealCards(p2, deck, 5);

        AIEngine engine(buildAIEngineConfig(7));
        engine.setOpponentHand(p2.hand);

        CardTypeResult prev; prev.type = CardType::Invalid;

        std::cout << "[" << (g + 1) << "] P1: " << playStr(p1.hand) << std::endl;

        auto newPlay = engine.choosePlay(p1, p2, prev, deck, 0);
        auto oldPlay = searchBestPlayCheat(p1, p2, prev, deck, 0, 6);

        std::cout << "  New: " << playStr(newPlay) << "type=" << getCardTypeInt(newPlay) << std::endl;
        std::cout << "  Old: " << playStr(oldPlay) << "type=" << getCardTypeInt(oldPlay) << std::endl;
        total++;
        if (playEq(newPlay, oldPlay)) { std::cout << "  MATCH" << std::endl; matchCount++; }
        else std::cout << "  DIFF!" << std::endl;
        std::cout << std::endl;
    }

    std::cout << "=== Responding ===" << std::endl;

    for (int g = 0; g < 10; ++g) {
        Deck deck = createStandardDeck();
        std::shuffle(deck.cards.begin(), deck.cards.end(), rng);

        Player p1 = createPlayer("P1");
        Player p2 = createPlayer("P2");
        dealCards(p1, deck, 5);
        dealCards(p2, deck, 5);

        AIEngine engine(buildAIEngineConfig(7));
        engine.setOpponentHand(p1.hand);

        auto allPlays = enumerateLegalPlays(p1);
        CardTypeResult prev;
        for (auto& p : allPlays) {
            auto parsed = parseCardType(p);
            if (parsed.type == CardType::Single) {
                prev = parsed;
                break;
            }
        }
        if (prev.type == CardType::Invalid) continue;

        std::cout << "[" << (g + 1) << "] P2: " << playStr(p2.hand)
                  << " prev=Single" << std::endl;

        auto newPlay = engine.choosePlay(p2, p1, prev, deck, 0);
        auto oldPlay = searchBestPlayCheat(p2, p1, prev, deck, 0, 6);

        std::cout << "  New: " << playStr(newPlay) << std::endl;
        std::cout << "  Old: " << playStr(oldPlay) << std::endl;
        total++;
        if (playEq(newPlay, oldPlay)) { std::cout << "  MATCH" << std::endl; matchCount++; }
        else std::cout << "  DIFF!" << std::endl;
        std::cout << std::endl;
    }

    std::cout << "===== " << matchCount << "/" << total << " match =====" << std::endl;
    return 0;
}