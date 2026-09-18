#pragma once

#include "game/game.h"
#include "core/card/deck.h"
#include "core/tracker/cardtracker.h"

RoundResult playRound(Player& first, Player& second, Deck& deck,
                      CardTracker* tracker = nullptr);