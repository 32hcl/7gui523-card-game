#pragma once

#include "game/position.h"
#include "state_evaluator.h"

static inline int evaluateLeafWithStateEvaluator(const GamePosition& state, const StateEvaluator* evaluator) {
    if (!evaluator) {
        const auto& my = state.myHand;
        const auto& opp = state.oppHand;
        int score = (state.myScore - state.oppScore) * 10;
        if (state.lastPlay.type != CardType::Invalid) {
            int dir = state.myTurn ? -1 : 1;
            score += (state.tableScore + state.tableBonus) * dir * 8;
        }
        int myHandValue = 0, oppHandValue = 0;
        for (const Card& c : my) myHandValue += c.score * 3;
        for (const Card& c : opp) oppHandValue += c.score * 3;
        score += myHandValue - oppHandValue;
        if (state.myFinalPhase) {
            score += (5 - (int)my.size()) * 20;
        }
        if (state.oppFinalPhase) {
            score -= (5 - (int)opp.size()) * 20;
        }
        return score;
    }
    return evaluator->evaluate(state);
}