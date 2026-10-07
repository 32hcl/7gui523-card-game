#include "level_boss.h"
#include "core/variant/variant_registry.h"
#include "ai/ai.h"
#include <random>

static const std::vector<std::string> kReplacementPoints = {
    "A", "2", "3", "5", "7", "8", "9", "10"
};

static int calcScore(const std::string& point) {
    if (point == "5")  return 5;
    if (point == "10") return 10;
    if (point == "K")  return 20;
    return 0;
}

void ai2_variant_deck(std::vector<Card>& deck) {
    /* 修改：static → thread_local，避免多线程竞争（规范1：资源安全） */
    thread_local std::mt19937 rng(std::random_device{}());
    thread_local std::uniform_int_distribution<int> dist(0, (int)kReplacementPoints.size() - 1);

    for (Card& c : deck) {
        if (c.point == "4" || c.point == "6") {
            VariantRegistry::record(c.seq, c);
            c.point = kReplacementPoints[dist(rng)];
            c.score = calcScore(c.point);
        }
    }
}

std::vector<Card> ai2_liar_choose(
    const Player& me,
    const Player& /*opp*/,
    const CardTypeResult& lastPlay,
    const Deck& /*myDeck*/,
    int /*tableScore*/
)
{
    return aiChoosePlayAI1(me, lastPlay);
}