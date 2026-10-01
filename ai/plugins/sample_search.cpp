#include "sample_search.h"
#include "minimax.h"
#include "uniform_sampler.h"
#include "bayesian_sampler.h"
#include "search_state.h"
#include "ai/ai.h"
#include <climits>
#include <algorithm>
#include <map>
#include <stdexcept>

Deck buildSampledDeck(const CardTracker& tracker,
                      const std::vector<Card>& myHand,
                      const std::vector<Card>& sampledHand,
                      std::mt19937& rng)
{
    Deck sampledDeck = createStandardDeck();
    auto removeCard = [&](const Card& card) {
        auto it = std::find_if(sampledDeck.cards.begin(), sampledDeck.cards.end(),
            [&](const Card& candidate) {
                return candidate.point == card.point && candidate.suit == card.suit;
            });
        if (it == sampledDeck.cards.end()) {
            throw std::invalid_argument("Inconsistent sampled world: duplicate or unavailable card");
        }
        sampledDeck.cards.erase(it);
    };
    for (const auto& card : myHand) removeCard(card);

    // Match the public pool construction used by both existing hand samplers.
    std::map<std::string, int> played;
    for (const auto& card : sampledDeck.cards) {
        played[card.point] = tracker.playedCount(card.point);
    }
    sampledDeck.cards.erase(
        std::remove_if(sampledDeck.cards.begin(), sampledDeck.cards.end(),
            [&](const Card& card) {
                int& remaining = played[card.point];
                if (remaining <= 0) return false;
                --remaining;
                return true;
            }),
        sampledDeck.cards.end());
    for (const auto& card : sampledHand) removeCard(card);
    std::shuffle(sampledDeck.cards.begin(), sampledDeck.cards.end(), rng);
    return sampledDeck;
}

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
    state.myDeckRemaining = deck.myRemaining;
    state.oppDeckRemaining = deck.oppRemaining;
    state.myFinalPhase = (deck.myRemaining == 0);
    state.oppFinalPhase = (deck.oppRemaining == 0);
    // Hidden hands and draw order are supplied only by sampled hypotheses.
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
        Player publicOpponent;
        publicOpponent.totalScore = opp.totalScore;
        publicOpponent.hand.resize(opp.hand.size());
        Deck publicDeck;
        publicDeck.cards.resize(deck.cards.size());
        for (size_t i = 0; i < allMoves.size(); ++i) {
            int s = evaluatePlayWithBreakdown(
                allMoves[i], me, publicOpponent, publicDeck, previous, tableScore, &tracker, nullptr);
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

    // Reuse the same complete worlds for every candidate, avoiding additional
    // candidate-to-candidate noise from reshuffling within the move loop.
    std::mt19937 worldRng(std::random_device{}());
    std::vector<SearchState> worlds;
    worlds.reserve(samples.size());
    for (const auto& sample : samples) {
        SearchState world = state;
        world.oppHand = sample.hand;
        world.deckCards = buildSampledDeck(tracker, me.hand, sample.hand, worldRng).cards;
        worlds.push_back(std::move(world));
    }

    std::vector<Card> bestMove;
    double bestAvg = -1e300;

    for (const auto& move : moves) {
        double totalScore = 0.0;
        double totalWeight = 0.0;

        for (size_t i = 0; i < samples.size(); ++i) {
            const auto& sample = samples[i];
            SearchState ns = applyMove(worlds[i], move);

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