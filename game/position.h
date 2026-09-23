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
    bool finalPhase = false;
    int firstEmpty = 0; // first player to empty a hand in this round: +1/-1
};
GamePosition advancePosition(const GamePosition&, const std::vector<Card>&);
