#include "searcher.h"
#include "play_selector.h"
#include "minimax_evaluator_adapter.h"
#include "ai/ai.h"
#include "ai/searcher/minimax.h"
#include "core/card/rank.h"
#include <climits>
#include <algorithm>
#include <stdexcept>

namespace {
    int minimaxWithEvaluator(GamePosition& state, int depth, int alpha, int beta, bool isMax, const StateEvaluator* stateEvaluator) {
        if (state.terminal) {
            return state.winner > 0 ? 10000 : state.winner < 0 ? -10000 : 0;
        }
        if (depth == 0) {
            return evaluateLeafWithStateEvaluator(state, stateEvaluator);
        }

        auto moves = genLegalMoves(state);
        if (moves.empty()) {
            return evaluateLeafWithStateEvaluator(state, stateEvaluator);
        }

        if (isMax) {
            int bestVal = INT_MIN + 1;
            for (const auto& m : moves) {
                GamePosition ns;
                try { ns = applyMove(state, m); }
                catch (const std::invalid_argument&) { continue; }
                int val = minimaxWithEvaluator(ns, depth - 1, alpha, beta, false, stateEvaluator);
                if (val > bestVal) bestVal = val;
                if (val > alpha) alpha = val;
                if (beta <= alpha) break;
            }
            return bestVal;
        } else {
            int bestVal = INT_MAX - 1;
            for (const auto& m : moves) {
                GamePosition ns;
                try { ns = applyMove(state, m); }
                catch (const std::invalid_argument&) { continue; }
                int val = minimaxWithEvaluator(ns, depth - 1, alpha, beta, true, stateEvaluator);
                if (val < bestVal) bestVal = val;
                if (val < beta) beta = val;
                if (beta <= alpha) break;
            }
            return bestVal;
        }
    }
}

std::vector<Card> NoSearcher::search(const Player& player,
                                      const Player& opponent,
                                      const CardTypeResult& previous,
                                      const Deck& deck,
                                      int tableScore,
                                      const CardTracker* tracker,
                                      const Evaluator* evaluator,
                                      DeckSide mySide) {
    auto allPlays = enumerateLegalPlays(player);
    if (allPlays.empty()) return {};

    std::vector<Card> best;
    int bestGain = INT_MIN;
    for (const auto& play : allPlays) {
        auto parsed = parseCardType(play);
        if (!previous.cards.empty() && !canBeat(parsed, previous)) continue;
        int gain = evaluator->evaluate(play, player, opponent, deck, previous, tableScore, tracker, mySide);
        if (gain > bestGain) { bestGain = gain; best = play; }
    }
    return best;
}

MinimaxSearcher::MinimaxSearcher(int depth, const StateEvaluator* stateEvaluator)
    : depth_(depth), stateEvaluator_(stateEvaluator) {}

std::vector<Card> MinimaxSearcher::search(const Player& player,
                                           const Player& opponent,
                                           const CardTypeResult& previous,
                                           const Deck& deck,
                                           int tableScore,
                                           const CardTracker* tracker,
                                           const Evaluator* evaluator,
                                           DeckSide /*mySide*/) {
    GamePosition state;
    state.myHand = player.hand;
    state.oppHand = opponent.hand;
    state.deckCards = deck.cards;
    state.finalPhase = deck.cards.empty();
    state.lastPlay = previous;
    state.myTurn = true;
    state.tableScore = tableScore;
    state.myScore = player.totalScore;
    state.oppScore = opponent.totalScore;
    state.terminal = false;
    state.winner = 0;

    auto allPlays = enumerateLegalPlays(player);
    if (allPlays.empty()) return {};

    if (previous.type == CardType::Invalid) {
        if (allPlays.size() == 1) return allPlays[0];

        std::vector<Card> bestMove;
        int bestScore = INT_MIN + 1;
        for (const auto& m : allPlays) {
            GamePosition ns;
            try { ns = advancePosition(state, m); }
            catch (const std::invalid_argument&) { continue; }
            int val = minimaxWithEvaluator(ns, depth_ - 1, INT_MIN + 1, INT_MAX - 1, false, stateEvaluator_);
            if (val > bestScore) { bestScore = val; bestMove = m; }
        }
        return bestMove;
    }

    std::vector<std::vector<Card>> candidates;
    for (const auto& play : allPlays) {
        auto parsed = parseCardType(play);
        if (canBeat(parsed, previous)) candidates.push_back(play);
    }
    candidates.push_back({});

    if (candidates.size() == 1) return candidates[0];

    std::vector<Card> bestMove;
    int bestScore = INT_MIN + 1;
    for (const auto& m : candidates) {
        GamePosition ns;
        try { ns = advancePosition(state, m); }
        catch (const std::invalid_argument&) { continue; }
        int val = minimaxWithEvaluator(ns, depth_ - 1, INT_MIN + 1, INT_MAX - 1, false, stateEvaluator_);
        if (val > bestScore) { bestScore = val; bestMove = m; }
    }
    return bestMove;
}