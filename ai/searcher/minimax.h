#pragma once
#include <vector>
#include "core/player.h"
#include "core/card/cardtype.h"
#include "core/card/deck.h"
#include "search_params.h"
#include "search_state.h"

std::vector<std::vector<Card>> genLegalMoves(const SearchState& state);
SearchState applyMove(const SearchState& state, const std::vector<Card>& move);
int minimax(SearchState& state, int depth, int alpha, int beta, bool isMax, const SearchParams& p);

std::vector<Card> searchBestPlayCheat(
    const Player& me,
    const Player& opp,
    const CardTypeResult& previous,
    const Deck& deck,
    int tableScore,
    int maxDepth = 6,
    const SearchParams& params = SearchParams());