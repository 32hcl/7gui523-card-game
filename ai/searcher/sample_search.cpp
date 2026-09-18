#include "sample_search.h"
#include "minimax.h"
#include "ai/sampler/uniform_sampler.h"
#include "ai/sampler/bayesian_sampler.h"
#include "search_state.h"
#include "ai/ai.h"
#include <climits>
#include <algorithm>

std::vector<Card> searchBestPlaySampled(
    const Player& me,
    const Player& opp,
    const CardTypeResult& previous,
    const Deck& deck,
    int tableScore,
    const CardTracker& tracker,
    const SearchParams& params,
    int sampleCount,
    int topK,
    int roundsPlayed,
    bool useBayesian)
{
    SearchState state;
    state.myHand = me.hand;
    state.oppHand = opp.hand;
    state.deckCards = deck.cards;
    state.lastPlay = previous;
    state.myTurn = true;
    state.tableScore = tableScore;
    state.myScore = me.totalScore;
    state.oppScore = opp.totalScore;
    state.terminal = false;
    state.winner = 0;

    auto allMoves = genLegalMoves(state);
    if (allMoves.empty()) return {};
    if (allMoves.size() == 1) return allMoves[0];

    std::vector<std::vector<Card>> moves;
    if ((int)allMoves.size() <= topK) {
        moves = allMoves;
    } else {
        struct Scored { int idx; int score; };
        std::vector<Scored> scored;
        for (size_t i = 0; i < allMoves.size(); ++i) {
            int s = evaluatePlayWithBreakdown(
                allMoves[i], me, opp, deck, previous, tableScore, &tracker, nullptr);
            scored.push_back({(int)i, s});
        }
        std::sort(scored.begin(), scored.end(),
            [](const Scored& a, const Scored& b) { return a.score > b.score; });
        for (int i = 0; i < topK && i < (int)scored.size(); ++i) {
            moves.push_back(allMoves[scored[i].idx]);
        }
    }

    std::vector<OpponentHandSample> samples;
    if (useBayesian) {
        auto bayesSamples = sampleOpponentHandsBayesian(
            tracker, me.hand, (int)opp.hand.size(), sampleCount, roundsPlayed);
        for (const auto& bs : bayesSamples) {
            OpponentHandSample s;
            s.hand = bs.hand;
            s.weight = bs.weight;
            samples.push_back(s);
        }
    } else {
        samples = sampleOpponentHands(
            tracker, me.hand, (int)opp.hand.size(), sampleCount);
    }

    std::vector<Card> bestMove;
    double bestAvg = -1e300;

    for (const auto& move : moves) {
        double totalScore = 0.0;
        double totalWeight = 0.0;

        for (const auto& sample : samples) {
            SearchState ns = state;
            ns.oppHand = sample.hand;

            ns = applyMove(ns, move);

            int val = minimax(ns, params.searchDepth - 1,
                              INT_MIN + 1, INT_MAX - 1,
                              false, params);

            totalScore += val * sample.weight;
            totalWeight += sample.weight;
        }

        double avg = (totalWeight > 0.0) ? totalScore / totalWeight : -1e300;
        if (avg > bestAvg) {
            bestAvg = avg;
            bestMove = move;
        }
    }

    return bestMove;
}