#pragma once

#include <vector>
#include "card.h"
#include "player.h"

int calculateScore(const std::vector<Card>& cards);
int calculateTableScore(const std::vector<Card>& cards, int bonus);
void settleScoreCards(Player& player, const std::vector<Card>& cards);