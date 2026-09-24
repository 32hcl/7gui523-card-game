#include "ai2_liar.h"
#include "ai1_idiot.h"
#include "core/variant/variant_registry.h"
#include <random>
#include <map>
#include <algorithm>

static const std::vector<std::string> kNonJokerPoints = {
    "A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K"
};

static int calcScore(const std::string& point) {
    if (point == "5")  return 5;
    if (point == "10") return 10;
    if (point == "K")  return 20;
    return 0;
}

void ai2_variant_deck(std::vector<Card>& deck) {
    static std::mt19937 rng(std::random_device{}());
    static std::uniform_int_distribution<int> dist(0, (int)kNonJokerPoints.size() - 1);

    for (Card& c : deck) {
        if (c.point == "4" || c.point == "6") {
            VariantRegistry::record(c.seq, c);
            c.point = kNonJokerPoints[dist(rng)];
            c.score = calcScore(c.point);
        }
    }
}

std::vector<Card> ai2_liar_choose(
    const Player& me,
    const Player& opp,
    const CardTypeResult& lastPlay,
    const Deck& myDeck,
    int tableScore
)
{
    std::map<std::string, int> pointCount;
    for (const Card& c : me.hand) {
        if (c.point != "大鬼" && c.point != "小鬼") {
            pointCount[c.point]++;
        }
    }

    for (const auto& kv : pointCount) {
        if (kv.second >= 5) {
            std::vector<Card> fiveCards;
            for (const Card& c : me.hand) {
                if (c.point == kv.first && fiveCards.size() < 5) {
                    fiveCards.push_back(c);
                }
            }

            bool isFirst = (lastPlay.type == CardType::Invalid);
            if (isFirst) {
                return fiveCards;
            }

            if (lastPlay.type != CardType::Special523) {
                return fiveCards;
            }
            break;
        }
    }

    return ai1_idiot_choose(me, opp, lastPlay, myDeck, tableScore);
}