#include "policy.h"
#include <algorithm>

std::vector<Card> GreedyPolicy::select(const std::vector<std::pair<std::vector<Card>, int>>& scoredPlays,
                                        std::mt19937&) {
    if (scoredPlays.empty()) return {};
    auto best = scoredPlays[0];
    for (const auto& p : scoredPlays) {
        if (p.second > best.second) best = p;
    }
    return best.first;
}

TopKPolicy::TopKPolicy(int k, double randomness)
    : k_(k), randomness_(randomness) {}

std::vector<Card> TopKPolicy::select(const std::vector<std::pair<std::vector<Card>, int>>& scoredPlays,
                                      std::mt19937& rng) {
    if (scoredPlays.empty()) return {};

    std::vector<std::pair<std::vector<Card>, int>> sorted = scoredPlays;
    std::sort(sorted.begin(), sorted.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    int k = std::min(k_, (int)sorted.size());

    std::uniform_real_distribution<double> randDist(0.0, 1.0);
    if (randDist(rng) < randomness_) {
        std::uniform_int_distribution<int> idxDist(0, k - 1);
        return sorted[idxDist(rng)].first;
    }

    return sorted[0].first;
}