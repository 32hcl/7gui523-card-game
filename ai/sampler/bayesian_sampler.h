#pragma once
#include <vector>
#include <string>
#include "core/card/card.h"
#include "core/tracker/cardtracker.h"

struct BayesianHandSample {
    std::vector<Card> hand;
    double weight = 1.0;
};

std::vector<BayesianHandSample> sampleOpponentHandsBayesian(
    const CardTracker& tracker,
    const std::vector<Card>& myHand,
    int opponentHandSize,
    int sampleCount,
    int roundsPlayed);