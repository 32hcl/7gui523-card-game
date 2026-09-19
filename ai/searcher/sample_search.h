#pragma once
#include <vector>
#include <random>
#include "core/player.h"
#include "core/card/cardtype.h"
#include "core/card/deck.h"
#include "core/tracker/cardtracker.h"
#include "ai/searcher/search_params.h"

// Build a complete hypothesis using public rank counts only. The tracker does
// not retain suits, so played cards use equivalent suit representatives.
Deck buildSampledDeck(const CardTracker& tracker,
                      const std::vector<Card>& myHand,
                      const std::vector<Card>& sampledHand,
                      std::mt19937& rng);

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
