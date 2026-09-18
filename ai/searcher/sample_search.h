#pragma once
#include <vector>
#include "core/player.h"
#include "core/card/cardtype.h"
#include "core/card/deck.h"
#include "core/tracker/cardtracker.h"
#include "ai/searcher/search_params.h"

std::vector<Card> searchBestPlaySampled(
    const Player& me,
    const Player& opp,
    const CardTypeResult& previous,
    const Deck& deck,
    int tableScore,
    const CardTracker& tracker,
    const SearchParams& params,
    int sampleCount,
    int topK = 999,
    int roundsPlayed = 0,
    bool useBayesian = false);