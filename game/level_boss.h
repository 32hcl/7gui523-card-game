#pragma once

#include <vector>
#include "core/card/card.h"
#include "core/card/cardtype.h"
#include "core/player.h"
#include "core/card/deck.h"

void ai2_variant_deck(std::vector<Card>& deck);

std::vector<Card> ai2_liar_choose(
    const Player& me,
    const Player& opp,
    const CardTypeResult& lastPlay,
    const Deck& myDeck,
    int tableScore
);