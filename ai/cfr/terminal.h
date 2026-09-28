#pragma once

#include <vector>
#include "core/card/card.h"

double computeTerminalUtility(
    const std::vector<Card>& myHand,
    const std::vector<Card>& oppHand,
    int myCollectedScore,
    int oppCollectedScore,
    int myDeckRemaining,
    int oppDeckRemaining,
    int tableScore,
    bool specialWin,
    bool isMyPerspective
);