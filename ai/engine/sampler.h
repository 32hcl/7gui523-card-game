#pragma once

#include <vector>
#include "core/card/card.h"
#include "core/tracker/cardtracker.h"
#include "ai/sampler/uniform_sampler.h"

class Sampler {
public:
    virtual ~Sampler() = default;
    virtual std::vector<OpponentHandSample> sample(const CardTracker& tracker,
                                                    const std::vector<Card>& myHand,
                                                    int opponentHandSize,
                                                    int sampleCount,
                                                    int turnsElapsed) const = 0;
};

class PerfectSampler : public Sampler {
public:
    explicit PerfectSampler(const std::vector<Card>& opponentHand);
    std::vector<OpponentHandSample> sample(const CardTracker& tracker,
                                            const std::vector<Card>& myHand,
                                            int opponentHandSize,
                                            int sampleCount,
                                            int turnsElapsed) const override;
private:
    std::vector<Card> opponentHand_;
};

class UniformSampler : public Sampler {
public:
    std::vector<OpponentHandSample> sample(const CardTracker& tracker,
                                            const std::vector<Card>& myHand,
                                            int opponentHandSize,
                                            int sampleCount,
                                            int turnsElapsed) const override;
};