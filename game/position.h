#pragma once
#include <vector>
#include "core/card/cardtype.h"
struct GamePosition {
    std::vector<Card> myHand, oppHand, deckCards;
    CardTypeResult lastPlay;
    bool myTurn = true;
    int tableScore = 0, tableBonus = 0, myScore = 0, oppScore = 0;
    bool terminal = false;
    int winner = 0;
    bool myFinalPhase = false;
    bool oppFinalPhase = false;
    int firstEmpty = 0;
    int myDeckRemaining = 0;
    int oppDeckRemaining = 0;
};
GamePosition advancePosition(const GamePosition&, const std::vector<Card>&);