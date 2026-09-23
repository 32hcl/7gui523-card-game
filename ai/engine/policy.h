#pragma once

#include <vector>
#include <utility>
#include <random>
#include "core/card/card.h"

class Policy {
public:
    virtual ~Policy() = default;
    virtual std::vector<Card> select(const std::vector<std::pair<std::vector<Card>, int>>& scoredPlays,
                                      std::mt19937& rng) = 0;
};

class GreedyPolicy : public Policy {
public:
    std::vector<Card> select(const std::vector<std::pair<std::vector<Card>, int>>& scoredPlays,
                              std::mt19937& rng) override;
};

class TopKPolicy : public Policy {
public:
    explicit TopKPolicy(int k = 3, double randomness = 0.2);
    std::vector<Card> select(const std::vector<std::pair<std::vector<Card>, int>>& scoredPlays,
                              std::mt19937& rng) override;
private:
    int k_;
    double randomness_;
};