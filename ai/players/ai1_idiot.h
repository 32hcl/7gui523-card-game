#pragma once

#include <vector>
#include "core/card/card.h"
#include "core/card/cardtype.h"
#include "core/player.h"
#include "core/card/deck.h"

// AI1 傻子：不出炸弹/火箭、50% 概率拆牌、不做任何推理
std::vector<Card> ai1_idiot_choose(
    const Player& me,
    const Player& opp,
    const CardTypeResult& lastPlay,
    const Deck& myDeck,
    int tableScore
);