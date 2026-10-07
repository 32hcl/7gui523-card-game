#pragma once

#include <vector>
#include "core/card/card.h"
#include "core/card/cardtype.h"
#include "core/player.h"
#include "core/card/deck.h"

// AI1 规则：最小单张→对子→同牌型最小→最小炸弹→五条→王炸
std::vector<Card> ai1_idiot_choose(
    const Player& me,
    const Player& opp,
    const CardTypeResult& lastPlay,
    const Deck& myDeck,
    int tableScore
);