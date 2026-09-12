#pragma once

#include <string>
#include <vector>
#include "card.h"
#include "deck.h"
#include "player.h"
#include "cardtype.h"

struct RoundResult {
    std::string winnerName;
    std::string finishedPlayerName;
    std::vector<Card> tableCards;
};

RoundResult playRound(Player& first, Player& second, Deck& deck);
void finalSettlement(Player& finisher, Player& opponent, std::vector<Card>& tableCards);
void compareAndAnnounce(const Player& a, const Player& b);
void printPlayerStatus(const Player& player);
void runBasicDataTest();
void runCardTypeTest();
void runAIVsAI();
void runHumanVsAI();