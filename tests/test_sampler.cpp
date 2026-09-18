#include "ai/sampler/uniform_sampler.h"
#include "ai/searcher/sample_search.h"
#include "ai/searcher/minimax.h"
#include "ai/searcher/search_params.h"
#include "ai/searcher/search_state.h"
#include "core/card/deck.h"
#include "core/player.h"
#include "core/tracker/cardtracker.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <climits>

static void printCards(const std::vector<Card>& cards, std::ostream& o) {
    for (const auto& c : cards) {
        o << c.point << c.suit << " ";
    }
}

int main() {
    std::ofstream out("sample_search_verify.txt");
    auto& o = out;

    // ── 构造局面：我手牌 + 对手手牌 ──
    Player me;
    me.totalScore = 0;
    me.hand = {
        {"3","黑桃",0}, {"5","黑桃",5}, {"7","黑桃",0},
        {"A","黑桃",0}, {"K","黑桃",20}
    };

    Player opp;
    opp.totalScore = 0;
    opp.hand = {
        {"4","红桃",0}, {"6","红桃",0}, {"8","红桃",0},
        {"10","红桃",10}, {"Q","红桃",0}
    };

    // 剩余牌堆：54 - 10 = 44 张
    Deck deck;
    for (const auto& card : createStandardDeck().cards) {
        bool inMe = false, inOpp = false;
        for (const auto& c : me.hand)
            if (c.point == card.point && c.suit == card.suit) inMe = true;
        for (const auto& c : opp.hand)
            if (c.point == card.point && c.suit == card.suit) inOpp = true;
        if (!inMe && !inOpp) deck.cards.push_back(card);
    }

    CardTypeResult previous;
    previous.type = CardType::Invalid;

    CardTracker tracker;

    SearchParams params;
    params.searchDepth = 4;

    int tableScore = 0;
    int sampleCount = 10;

    // ── 1. 列出所有候选走法 ──
    SearchState initState;
    initState.myHand = me.hand;
    initState.oppHand = opp.hand;
    initState.deckCards = deck.cards;
    initState.lastPlay = previous;
    initState.myTurn = true;
    initState.tableScore = tableScore;
    initState.myScore = me.totalScore;
    initState.oppScore = opp.totalScore;

    auto allMoves = genLegalMoves(initState);

    o << "=== Candidate Moves (" << allMoves.size() << " total) ===\n";
    for (size_t i = 0; i < allMoves.size(); ++i) {
        o << "[" << i << "] ";
        printCards(allMoves[i], o);
        if (allMoves[i].empty()) o << "(pass)";
        o << "\n";
    }
    o << "\n";

    // ── 2. 每个走法的作弊版评分 ──
    o << "=== Cheat Search (knows opponent hand, depth="
      << params.searchDepth << ") ===\n";

    auto t1 = std::chrono::steady_clock::now();
    for (size_t i = 0; i < allMoves.size(); ++i) {
        SearchState ns = applyMove(initState, allMoves[i]);
        int val = minimax(ns, params.searchDepth - 1,
                          INT_MIN + 1, INT_MAX - 1, false, params);
        o << "[" << i << "] score=" << val << "  ";
        printCards(allMoves[i], o);
        o << "\n";
    }
    auto cheatMove = searchBestPlayCheat(me, opp, previous, deck, tableScore,
                                         params.searchDepth, params);
    auto t2 = std::chrono::steady_clock::now();
    auto cheatMs = std::chrono::duration<double, std::milli>(t2 - t1).count();

    o << "Cheat chose: ";
    printCards(cheatMove, o);
    o << "\n";
    o << "Cheat time: " << cheatMs << " ms\n\n";

    // ── 3. 采样版（10 个采样，所有走法共享同一批样本）──
    o << "=== Sampled Search (10 samples, depth="
      << params.searchDepth << ") ===\n";

    t1 = std::chrono::steady_clock::now();

    auto sharedSamples = sampleOpponentHands(
        tracker, me.hand, (int)opp.hand.size(), sampleCount);

    double bestAvg = -1e300;
    size_t bestIdx = 0;
    for (size_t i = 0; i < allMoves.size(); ++i) {
        const auto& move = allMoves[i];
        double totalScore = 0.0;
        double totalWeight = 0.0;

        for (const auto& s : sharedSamples) {
            SearchState ns = initState;
            ns.oppHand = s.hand;
            ns = applyMove(ns, move);
            int val = minimax(ns, params.searchDepth - 1,
                              INT_MIN + 1, INT_MAX - 1, false, params);
            totalScore += val * s.weight;
            totalWeight += s.weight;
        }

        double avg = (totalWeight > 0.0) ? totalScore / totalWeight : -1e300;
        o << "[" << i << "] avg=" << avg << "  ";
        printCards(allMoves[i], o);
        o << "\n";
        if (avg > bestAvg) { bestAvg = avg; bestIdx = i; }
    }

    o << "Per-move best (shared samples): ["
      << bestIdx << "] avg=" << bestAvg << "\n";
    auto bestFromShared = allMoves[bestIdx];

    auto sampledMove = searchBestPlaySampled(
        me, opp, previous, deck, tableScore, tracker, params, sampleCount);
    t2 = std::chrono::steady_clock::now();
    auto sampledMs = std::chrono::duration<double, std::milli>(t2 - t1).count();

    o << "searchBestPlaySampled chose: ";
    printCards(sampledMove, o);
    o << "\n";
    bool consistent = (bestFromShared.size() == sampledMove.size());
    if (consistent) {
        for (size_t j = 0; j < bestFromShared.size(); ++j) {
            if (bestFromShared[j].point != sampledMove[j].point ||
                bestFromShared[j].suit != sampledMove[j].suit) {
                consistent = false; break;
            }
        }
    }
    o << "Per-move best == function result: "
      << (consistent ? "YES" : "NO (different samples)") << "\n";
    o << "Sampled time: " << sampledMs << " ms\n\n";

    // ── 4. 对比 ──
    o << "=== Comparison ===\n";
    o << "Cheat  : ";
    printCards(cheatMove, o);
    o << "(" << cheatMs << " ms)\n";
    o << "Sampled: ";
    printCards(sampledMove, o);
    o << "(" << sampledMs << " ms)\n";

    bool same = (cheatMove.size() == sampledMove.size());
    if (same) {
        for (size_t i = 0; i < cheatMove.size(); ++i) {
            if (cheatMove[i].point != sampledMove[i].point ||
                cheatMove[i].suit != sampledMove[i].suit) {
                same = false; break;
            }
        }
    }
    o << "Same choice: " << (same ? "YES" : "NO") << "\n";
    o << "Speed ratio: " << (sampledMs / cheatMs) << "x\n";

    out.close();
    std::cout << "Done. See sample_search_verify.txt\n";
    return 0;
}