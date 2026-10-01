#include "sampler.h"
#include "core/card/deck.h"
#include <chrono>
#include <random>
#include <algorithm>

PerfectSampler::PerfectSampler(const std::vector<Card>& opponentHand)
    : opponentHand_(opponentHand) {}

std::vector<OpponentHandSample> PerfectSampler::sample(const CardTracker& tracker,
                                                        const std::vector<Card>& myHand,
                                                        int opponentHandSize,
                                                        int sampleCount,
                                                        int turnsElapsed) const {
    (void)tracker; (void)myHand; (void)opponentHandSize; (void)turnsElapsed;
    std::vector<OpponentHandSample> results;
    results.reserve(sampleCount);
    for (int i = 0; i < sampleCount; ++i) {
        OpponentHandSample sample;
        sample.hand = opponentHand_;
        sample.weight = 1.0;
        results.push_back(sample);
    }
    return results;
}

std::vector<OpponentHandSample> UniformSampler::sample(const CardTracker& tracker,
                                                        const std::vector<Card>& myHand,
                                                        int opponentHandSize,
                                                        int sampleCount,
                                                        int turnsElapsed) const {
    static const std::vector<std::string> allPoints = {
        "A","2","3","4","5","6","7","8","9","10","J","Q","K",
        "大鬼","小鬼"
    };

    Deck fullDeck = createStandardDeck();
    std::vector<Card> pool = fullDeck.cards;

    for (const auto& myCard : myHand) {
        auto it = std::find_if(pool.begin(), pool.end(),
            [&](const Card& c) {
                return c.point == myCard.point && c.suit == myCard.suit;
            });
        if (it != pool.end()) pool.erase(it);
    }

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

    std::mt19937 rng(static_cast<unsigned>(
        std::chrono::steady_clock::now().time_since_epoch().count() + turnsElapsed));

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