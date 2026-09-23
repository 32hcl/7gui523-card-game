#include "state_evaluator.h"
#include "core/card/rank.h"

int SimpleStateEvaluator::evaluate(const GamePosition& state) const {
    const auto& my = state.myHand;
    const auto& opp = state.oppHand;

    int score = (state.myScore - state.oppScore) * 10;

    if (state.lastPlay.type != CardType::Invalid) {
        int dir = state.myTurn ? -1 : 1;
        score += (state.tableScore + state.tableBonus) * dir * 8;
    }

    int myHandValue = 0, oppHandValue = 0;
    for (const Card& c : my) {
        int rank = getCardRank(c.point);
        myHandValue += rank * 2 + c.score * 3;
    }
    for (const Card& c : opp) {
        int rank = getCardRank(c.point);
        oppHandValue += rank * 2 + c.score * 3;
    }
    score += myHandValue - oppHandValue;

    score += (5 - (int)my.size()) * 20;
    score -= (5 - (int)opp.size()) * 20;

    return score;
}