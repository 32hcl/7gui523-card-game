#include "ai/sampler/uniform_sampler.h"
#include "core/card/deck.h"
#include <random>
#include <algorithm>

std::vector<OpponentHandSample> sampleOpponentHands(
    const CardTracker& tracker,
    const std::vector<Card>& myHand,
    int opponentHandSize,
    int sampleCount)
{
    Deck fullDeck = createStandardDeck();
    std::vector<Card> pool = fullDeck.cards;

    for (const auto& myCard : myHand) {
        auto it = std::find_if(pool.begin(), pool.end(),
            [&](const Card& c) {
                return c.point == myCard.point && c.suit == myCard.suit;
            });
        if (it != pool.end()) pool.erase(it);
    }

    std::vector<std::string> allPoints = {
        "A","2","3","4","5","6","7","8","9","10","J","Q","K",
        "大鬼","小鬼"
    };
    for (const auto& point : allPoints) {
        int played = tracker.playedCount(point);
        int removed = 0;
        for (auto it = pool.begin(); it != pool.end() && removed < played; ) {
            if (it->point == point) {
                it = pool.erase(it);
                removed++;
            } else {
                ++it;
            }
        }
    }

    std::random_device rd;
    std::mt19937 rng(rd());

    std::vector<OpponentHandSample> results;
    results.reserve(sampleCount);
    for (int i = 0; i < sampleCount; ++i) {
        OpponentHandSample sample;
        sample.weight = 1.0;
        std::shuffle(pool.begin(), pool.end(), rng);
        int n = std::min(opponentHandSize, (int)pool.size());
        sample.hand.assign(pool.begin(), pool.begin() + n);
        results.push_back(sample);
    }

    return results;
}