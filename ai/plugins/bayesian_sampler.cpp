#include "bayesian_sampler.h"
#include "core/card/deck.h"
#include <random>
#include <algorithm>
#include <cmath>
#include <numeric>

static double decayForPoint(const std::string& point) {
   if (point == "7" || point == "大鬼" || point == "小鬼") return 0.95;
   if (point == "5" || point == "2"  || point == "3")   return 0.92;
   if (point == "10" || point == "K")                    return 0.85;
   if (point == "4" || point == "6" ||
      point == "8" || point == "9")                     return 0.80;
return 0.85;
}

std::vector<BayesianHandSample> sampleOpponentHandsBayesian(
    const CardTracker& tracker,
    const std::vector<Card>& myHand,
    int opponentHandSize,
    int sampleCount,
    int roundsPlayed)
{
    // 1. 构建候选牌池（和均匀版一样：去掉我的手牌和已出的牌）
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

    if (pool.empty() || opponentHandSize <= 0) return {};

    // 2. 计算每张牌的贝叶斯权重
    //    weight = decay_factor ^ roundsPlayed
    std::vector<double> pointWeights;
    pointWeights.reserve(pool.size());
    for (const auto& card : pool) {
        double d = decayForPoint(card.point);
        double w = std::pow(d, roundsPlayed);
        pointWeights.push_back(w);
    }

    // 3. 加权无放回采样（exponential sort 技巧）
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_real_distribution<double> uniform(0.0, 1.0);

    std::vector<BayesianHandSample> results;
    results.reserve(sampleCount);

    for (int s = 0; s < sampleCount; ++s) {
        BayesianHandSample sample;

        // 为每张候选牌生成指数键
        std::vector<double> keys(pool.size());
        for (size_t i = 0; i < pool.size(); ++i) {
            double u = uniform(rng);
            if (u < 1e-12) u = 1e-12;
            keys[i] = -std::log(u) / pointWeights[i];
        }

        // 按 key 降序排列，取前 N 张
        std::vector<size_t> indices(pool.size());
        std::iota(indices.begin(), indices.end(), 0);
        std::sort(indices.begin(), indices.end(),
           [&](size_t a, size_t b) { return keys[a] < keys[b]; });

        int n = std::min(opponentHandSize, (int)pool.size());
        for (int j = 0; j < n; ++j) {
            sample.hand.push_back(pool[indices[j]]);
        }

        results.push_back(sample);
    }

    return results;
}