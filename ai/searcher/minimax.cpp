#include "minimax.h"
#include "search_params.h"
#include "search_state.h"
#include "core/card/cardtype.h"
#include "core/rule/score.h"
#include "core/rule/special.h"
#include <algorithm>
#include <climits>

// ── 1. 枚举所有合法走法 ──
std::vector<std::vector<Card>> genLegalMoves(const SearchState& state) {
    const auto& hand = state.myTurn ? state.myHand : state.oppHand;
    std::vector<std::vector<Card>> moves;

    if (hand.empty()) {
        // 手牌为空，只能 pass
        if (state.lastPlay.type != CardType::Invalid)
            moves.push_back({});
        return moves;
    }

    // 枚举所有合法牌型
    size_t n = hand.size();
    for (size_t mask = 1; mask < (1ULL << n); ++mask) {
        std::vector<Card> subset;
        for (size_t i = 0; i < n; ++i) {
            if (mask & (1ULL << i)) subset.push_back(hand[i]);
        }
        auto parsed = parseCardType(subset);
        if (parsed.type == CardType::Invalid) continue;

        if (state.lastPlay.type == CardType::Invalid) {
            // 先手：所有合法牌型均可
            moves.push_back(subset);
        } else {
            // 后手：必须能压过
            if (canBeat(parsed, state.lastPlay)) {
                moves.push_back(subset);
            }
        }
    }

    // 后手额外加 pass（空 vector）
    if (state.lastPlay.type != CardType::Invalid) {
        moves.push_back({});
    }

    return moves;
}

// ── 2. 执行走法，返回新状态 ──
SearchState applyMove(const SearchState& state, const std::vector<Card>& move) {
    return advancePosition(state, move);
}

// ── 3. 叶节点估值 ──
static int evaluateLeaf(const SearchState& state, const SearchParams& p) {
    const auto& my = state.myHand;
    const auto& opp = state.oppHand;

    int score = (state.myScore - state.oppScore) * p.scoreDiffWeight;

    if (state.lastPlay.type != CardType::Invalid) {
        int dir = state.myTurn ? -1 : 1;
        score += (state.tableScore + state.tableBonus) * dir * p.tableScoreWeight;
    }

    int myHandValue = 0, oppHandValue = 0;
    for (const Card& c : my) {
        auto it = RANK_MAP.find(c.point);
        int rank = (it != RANK_MAP.end()) ? it->second : 0;
        myHandValue += rank * p.handRankWeight + c.score * p.handScoreWeight;
    }
    for (const Card& c : opp) {
        auto it = RANK_MAP.find(c.point);
        int rank = (it != RANK_MAP.end()) ? it->second : 0;
        oppHandValue += rank * p.handRankWeight + c.score * p.handScoreWeight;
    }
    score += myHandValue - oppHandValue;

    if (state.myFinalPhase) {
        score += (5 - (int)my.size()) * p.handSizeWeight;
    }
    if (state.oppFinalPhase) {
        score -= (5 - (int)opp.size()) * p.handSizeWeight;
    }

    return score;
}

// ── 4. Minimax + Alpha-Beta ──
int minimax(SearchState& state, int depth, int alpha, int beta, bool isMax, const SearchParams& p) {
    if (state.terminal) {
        return state.winner > 0 ? 10000 : state.winner < 0 ? -10000 : 0;
    }
    if (depth == 0) {
        return evaluateLeaf(state, p);
    }

    auto moves = genLegalMoves(state);
    if (moves.empty()) {
        return evaluateLeaf(state, p);
    }

    if (isMax) {
        int bestVal = INT_MIN + 1;
        for (const auto& m : moves) {
            SearchState ns = applyMove(state, m);
            int val = minimax(ns, depth - 1, alpha, beta, false, p);
            if (val > bestVal) bestVal = val;
            if (val > alpha) alpha = val;
            if (beta <= alpha) break;
        }
        return bestVal;
    } else {
        int bestVal = INT_MAX - 1;
        for (const auto& m : moves) {
            SearchState ns = applyMove(state, m);
            int val = minimax(ns, depth - 1, alpha, beta, true, p);
            if (val < bestVal) bestVal = val;
            if (val < beta) beta = val;
            if (beta <= alpha) break;
        }
        return bestVal;
    }
}

// ── 5. 主入口：作弊版搜索 ──
std::vector<Card> searchBestPlayCheat(
    const Player& me,
    const Player& opp,
    const CardTypeResult& previous,
    const Deck& deck,
    int tableScore,
    int maxDepth,
    const SearchParams& params)
{
    // 构造初始 SearchState
    SearchState state;
    state.myHand = me.hand;
    state.oppHand = opp.hand;
    state.deckCards = deck.cards;
    state.myFinalPhase = (deck.myRemaining == 0);
    state.oppFinalPhase = (deck.oppRemaining == 0);
    state.myDeckRemaining = deck.myRemaining;
    state.oppDeckRemaining = deck.oppRemaining;
    state.lastPlay = previous;
    state.myTurn = true;
    state.tableScore = tableScore;
    state.myScore = me.totalScore;
    state.oppScore = opp.totalScore;
    state.terminal = false;
    state.winner = 0;

    auto moves = genLegalMoves(state);
    if (moves.empty()) return {};
    if (moves.size() == 1) return moves[0];

    std::vector<Card> bestMove;
    int bestScore = INT_MIN + 1;

    for (const auto& m : moves) {
        SearchState ns = applyMove(state, m);
        int val = minimax(ns, maxDepth - 1, INT_MIN + 1, INT_MAX - 1, false, params);
        if (val > bestScore) {
            bestScore = val;
            bestMove = m;
        }
    }

    return bestMove;
}