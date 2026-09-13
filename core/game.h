#pragma once

#include <string>
#include <vector>
#include "card.h"
#include "deck.h"
#include "player.h"
#include "cardtype.h"
#include "cardtracker.h"

struct RoundResult {
    std::string winnerName;
    std::string finishedPlayerName;
    std::vector<Card> tableCards;
    bool specialVictory = false;
    bool handEmptied = false;
};

RoundResult playRound(Player& first, Player& second, Deck& deck,
                      CardTracker* tracker = nullptr);
void finalSettlement(Player& finisher, Player& opponent, std::vector<Card>& tableCards);
inline void finalSettlement(Player& finisher, Player& opponent) {
    std::vector<Card> empty;
    finalSettlement(finisher, opponent, empty);
}
void compareAndAnnounce(const Player& a, const Player& b);
void printPlayerStatus(const Player& player);
void runBasicDataTest();
void runCardTypeTest();
void runAIVsAI();
void runHumanVsAI();