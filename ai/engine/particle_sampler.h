#pragma once

#include <vector>
#include "core/card/card.h"
#include "core/tracker/cardtracker.h"
#include "sampler.h"
#include "ai/sampler/bayesian_sampler.h"

class ParticleSampler : public Sampler {
public:
    std::vector<OpponentHandSample> sample(const CardTracker& tracker,
                                            const std::vector<Card>& myHand,
                                            int opponentHandSize,
                                            int sampleCount,
                                            int turnsElapsed) const override {
        auto bayesianSamples = sampleOpponentHandsBayesian(tracker, myHand, opponentHandSize, sampleCount, turnsElapsed);
        std::vector<OpponentHandSample> result;
        result.reserve(bayesianSamples.size());
        for (const auto& s : bayesianSamples) {
            OpponentHandSample sample;
            sample.hand = s.hand;
            sample.weight = s.weight;
            result.push_back(sample);
        }
        return result;
    }
};