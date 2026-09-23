#pragma once

#include <vector>
#include "core/card/card.h"
#include "core/card/cardtype.h"
#include "core/player.h"

std::vector<Card> tryFinishPlay(const Player& player,
                                 const std::vector<std::vector<Card>>& legalPlays,
                                 const CardTypeResult& previous);

std::vector<Card> tryEndgameIntercept(const Player& player,
                                       const std::vector<std::vector<Card>>& legalPlays,
                                       const CardTypeResult& previous,
                                       int opponentHandSize);