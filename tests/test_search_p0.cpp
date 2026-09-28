#include "ai/searcher/minimax.h"
#include "ai/searcher/sample_search.h"
#include "ai/sampler/uniform_sampler.h"
#include "ai/sampler/bayesian_sampler.h"
#include <cassert>
#include <climits>
#include <map>
#include <set>
#include <stdexcept>

static std::string cardId(const Card& card) {
    return card.point + ":" + card.suit;
}

void test_sampledWorldConservation() {
    const auto full = createStandardDeck().cards;
    const std::vector<Card> mine(full.begin(), full.begin() + 5);
    const std::vector<Card> played(full.begin() + 10, full.begin() + 30);
    CardTracker tracker;
    tracker.recordPlayed(played);
    std::mt19937 rng(20260918);
    auto verify = [&](const std::vector<Card>& opponent) {
        const Deck world = buildSampledDeck(tracker, mine, opponent, rng);
        assert(world.cards.size() == 24);
        std::set<std::string> ids;
        std::map<std::string, int> counts;
        for (const auto* cards : {&mine, &opponent, &world.cards}) {
            for (const auto& card : *cards) {
                assert(ids.insert(cardId(card)).second);
                ++counts[card.point];
            }
        }
        for (const auto& card : full) {
            assert(counts[card.point] + tracker.playedCount(card.point)
                   == tracker.totalCount(card.point));
        }
        // The supplied RNG fully determines the hypothesis draw order.
        std::mt19937 first(123), second(123);
        const auto a = buildSampledDeck(tracker, mine, opponent, first).cards;
        const auto b = buildSampledDeck(tracker, mine, opponent, second).cards;
        for (size_t i = 0; i < a.size(); ++i) assert(cardId(a[i]) == cardId(b[i]));
    };
    for (const auto& sample : sampleOpponentHands(tracker, mine, 5, 32)) verify(sample.hand);
    for (const auto& sample : sampleOpponentHandsBayesian(tracker, mine, 5, 32, 5)) verify(sample.hand);

    // Exhausted deck: all unknown cards belong to the opponent.
    CardTracker endTracker;
    endTracker.recordPlayed(std::vector<Card>(full.begin() + 10, full.end()));
    for (const auto& sample : sampleOpponentHands(endTracker, mine, 5, 2)) {
        assert(buildSampledDeck(endTracker, mine, sample.hand, rng).cards.empty());
    }
    const auto opening = sampleOpponentHands(CardTracker{}, mine, 5, 1);
    assert(buildSampledDeck(CardTracker{}, mine, opening[0].hand, rng).cards.size() == 44);
    bool rejected = false;
    try { buildSampledDeck(tracker, mine, {mine.front()}, rng); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
}

void test_searchFinalSettlement() {
    // Finishing gains 15 table + 5 played + 20 K + 10, i.e. 50 points.
    for (bool myTurn : {false, true}) {
        for (int otherScore : {40, 50, 60}) {
            SearchState state;
            state.myTurn = myTurn;
            state.myFinalPhase = true;
            auto& finisher = myTurn ? state.myHand : state.oppHand;
            auto& other = myTurn ? state.oppHand : state.myHand;
            finisher = {{"5", "黑桃", 5}};
            other = {{"K", "红桃", 20}, {"10", "方块", 10}};
            state.tableScore = 15;
            if (myTurn) state.oppScore = otherScore;
            else state.myScore = otherScore;
            const SearchState result = applyMove(state, finisher);
            assert(result.terminal);
            assert(result.myHand.empty() && result.oppHand.empty());
            assert(result.tableScore == 0);
            assert((myTurn ? result.myScore : result.oppScore) == 50);
            assert((myTurn ? result.oppScore : result.myScore) == otherScore);
            const int finisherResult = otherScore < 50 ? 1 : otherScore > 50 ? -1 : 0;
            assert(result.winner == (myTurn ? finisherResult : -finisherResult));
            assert(state.tableScore == 15 && finisher.size() == 1 && other.size() == 2);
        }
    }
    // Preserve the existing played-special win even if ordinary scores lose.
    SearchState special;
    special.oppScore = 200;
    special.myHand = {{"7", "黑桃", 0}, {"大鬼", "", 0}, {"5", "黑桃", 5},
                      {"2", "黑桃", 0}, {"3", "黑桃", 0}};
    const auto result = applyMove(special, special.myHand);
    assert(result.terminal && result.winner == 1);
}

void test_searchTerminalUtility() {
    for (int winner : {-1, 0, 1}) {
        for (int depth : {0, 6}) {
            for (bool isMax : {false, true}) {
                SearchState state;
                state.terminal = true;
                state.winner = winner;
                state.myScore = winner < 0 ? 900 : 0;
                state.oppScore = winner > 0 ? 900 : 0;
                assert(minimax(state, depth, INT_MIN + 1, INT_MAX - 1,
                               isMax, SearchParams{}) == winner * 10000);
            }
        }
    }
    SearchState ordinary;
    ordinary.myScore = 12;
    ordinary.oppScore = 5;
    assert(minimax(ordinary, 0, INT_MIN + 1, INT_MAX - 1, true, SearchParams{}) == 70);
}