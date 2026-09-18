#pragma once

#include <vector>
#include <string>
#include "core/card/card.h"
#include "core/player.h"

struct RoundResult {
    std::string winnerName;
    std::string finishedPlayerName;
    std::vector<Card> tableCards;
    bool specialVictory = false;
    bool handEmptied = false;
};

void finalSettlement(Player& finisher, Player& opponent, std::vector<Card>& tableCards);
inline void finalSettlement(Player& finisher, Player& opponent) {
    std::vector<Card> empty;
    finalSettlement(finisher, opponent, empty);
}
void compareAndAnnounce(const Player& a, const Player& b);