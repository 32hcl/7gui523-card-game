#pragma once
#include <vector>
#include "core/card/card.h"
#include "cardtracker.h"

struct OpponentHandSample {
    std::vector<Card> hand;
    double weight = 1.0;
};

std::vector<OpponentHandSample> sampleOpponentHands(
    const CardTracker& tracker,
    const std::vector<Card>& myHand,
    int opponentHandSize,
    int sampleCount);