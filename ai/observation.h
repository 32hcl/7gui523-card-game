#pragma once
#include "core/card/cardtype.h"
#include <array>
#include <vector>
struct PublicEvent {
    bool actorMe=true;
    std::vector<Card> cards, myDraws;
    int myCount=0, opponentCount=0, deckCount=0;
};
// No opponent cards or real draw order can enter the fair engine interface.
struct Observation {
    std::vector<Card> hand, initialHand;
    std::array<int,15> played{};
    int opponentCount=5, deckCount=44;
    int myScore=0, opponentScore=0, tableScore=0, tableBonus=0;
    CardTypeResult previous;
    bool finalPhase=false, initialMyTurn=true;
    int firstEmpty=0;
    std::vector<PublicEvent> history;
};
struct FairConfig {
    int particles=48, simulations=48, horizon=12, budgetMs=60;
    double exploration=1.2, tolerance=0.08;
    std::array<double,5> modelPrior{{.35,.3,.1,.15,.1}};
    unsigned seed=714025;
    int rolloutModel=1;
};
