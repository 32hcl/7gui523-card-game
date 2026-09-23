#pragma once

#include "game/game.h"
#include "core/card/deck.h"
#include "core/tracker/cardtracker.h"

RoundResult playRound(Player& first, Player& second, Deck& deck,
                      CardTracker* tracker = nullptr);

// 双牌堆版本：first / second 各自从 firstDeck / secondDeck 摸牌
// 当一方手牌+牌堆全空 → RoundResult 中 handEmptied=true, finishedPlayerName 为该方
RoundResult playRoundDualDeck(Player& first, Player& second,
                              Deck& firstDeck, Deck& secondDeck,
                              CardTracker* tracker = nullptr);