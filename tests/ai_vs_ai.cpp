#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <random>
#include <functional>
#include <algorithm>
#include <memory>

#include "core/card/deck.h"
#include "core/player.h"
#include "core/card/cardtype.h"
#include "core/rule/score.h"
#include "core/rule/special.h"
#include "ai/ai.h"
#include "ai/ai_types.h"
#include "tests/test_common.h"

struct Result {
    int wins = 0;
    int losses = 0;
    int draws = 0;
    long long scoreFor = 0;
    long long scoreAgainst = 0;
};

static Result runOneGame(const AIFn& fnA, const AIFn& fnB, CardTracker& tracker, std::mt19937& rng) {
    tracker.reset();
    Result r;
    Deck deck = createStandardDeck();
    std::shuffle(deck.cards.begin(), deck.cards.end(), rng);

    Deck myDeck, oppDeck;
    splitDeck(deck.cards, oppDeck.cards, myDeck.cards);
    myDeck.myRemaining = (int)myDeck.cards.size();
    oppDeck.oppRemaining = (int)oppDeck.cards.size();

    Player pA = createPlayer("A");
    Player pB = createPlayer("B");

    dealCards(pA, myDeck, 5);
    dealCards(pB, oppDeck, 5);
    myDeck.myRemaining = (int)myDeck.cards.size();
    oppDeck.oppRemaining = (int)oppDeck.cards.size();

    if (checkSpecialVictory(pA)) { r.scoreFor += 500; r.scoreAgainst += 0; r.wins++; return r; }
    if (checkSpecialVictory(pB)) { r.scoreFor += 0; r.scoreAgainst += 500; r.losses++; return r; }

    Player* cur = &pA;
    Player* opp = &pB;
    Player* lastP = nullptr;
    std::vector<Card> table;
    int tableBonus = 0;
    CardTypeResult lp;
    int firstEmpty = 0;

    int safety = 0;
    while (safety++ < 500) {
        while (true) {
            int ts = calculateScore(table);
            Deck mergedView;
            mergedView.cards.assign(myDeck.cards.begin(), myDeck.cards.end());
            mergedView.cards.insert(mergedView.cards.end(), oppDeck.cards.begin(), oppDeck.cards.end());
            if (cur == &pA) {
                mergedView.myRemaining = myDeck.myRemaining;
                mergedView.oppRemaining = oppDeck.oppRemaining;
            } else {
                mergedView.myRemaining = oppDeck.oppRemaining;
                mergedView.oppRemaining = myDeck.myRemaining;
            }

            auto chosen = (cur == &pA) ? fnA(*cur, *opp, lp, mergedView, ts, tracker)
                                       : fnB(*cur, *opp, lp, mergedView, ts, tracker);
            if (chosen.empty()) {
                if (lp.type == CardType::Invalid) break;
                settleScoreCards(*lastP, table);
                lastP->totalScore += tableBonus;
                table.clear();
                tableBonus = 0;
                break;
            }

            auto parsed = parseCardType(chosen);
            if (parsed.type == CardType::Invalid) break;
            if (lp.type != CardType::Invalid && !canBeat(parsed, lp)) break;

            tableBonus += calculatePressureBonus(parsed, lp);

            for (auto& c : chosen) {
                auto it = std::find_if(cur->hand.begin(), cur->hand.end(),
                    [&](const Card& h) { return h.point == c.point && h.suit == c.suit; });
                if (it != cur->hand.end()) cur->hand.erase(it);
            }
            tracker.recordPlayed(chosen);
            for (auto& c : chosen) table.push_back(c);
            lp = parsed;
            lastP = cur;

            if (checkSpecialVictory(*cur)) {
                if (cur == &pA) { r.scoreFor += 500; r.scoreAgainst += pB.totalScore; r.wins++; }
                else             { r.scoreFor += pA.totalScore; r.scoreAgainst += 500; r.losses++; }
                return r;
            }

            if (cur->hand.empty()) {
                if (!firstEmpty) firstEmpty = (cur == &pA) ? 1 : -1;
                int& remaining = (cur == &pA) ? myDeck.myRemaining : oppDeck.oppRemaining;
                if (remaining == 0) {
                    settleScoreCards(*cur, table);
                    cur->totalScore += tableBonus;
                    settleScoreCards(*cur, opp->hand);
                    opp->hand.clear();
                    table.clear();
                    tableBonus = 0;
                    if (pA.totalScore > pB.totalScore) { r.wins++; }
                    else if (pB.totalScore > pA.totalScore) { r.losses++; }
                    else { r.draws++; }
                    r.scoreFor += pA.totalScore;
                    r.scoreAgainst += pB.totalScore;
                    return r;
                }
            }

            std::swap(cur, opp);
        }

        lp = CardTypeResult{};
        if (checkSpecialVictory(pA)) { r.scoreFor += 500; r.scoreAgainst += pB.totalScore; r.wins++; return r; }
        if (checkSpecialVictory(pB)) { r.scoreFor += pA.totalScore; r.scoreAgainst += 500; r.losses++; return r; }

        refillToFive(pA, myDeck);
        refillToFive(pB, oppDeck);
        myDeck.myRemaining = (int)myDeck.cards.size();
        oppDeck.oppRemaining = (int)oppDeck.cards.size();

        bool aDone = myDeck.cards.empty() && pA.hand.empty();
        bool bDone = oppDeck.cards.empty() && pB.hand.empty();
        if (aDone && bDone) {
            Player* finisher = (firstEmpty == 1) ? &pA : (firstEmpty == -1) ? &pB : nullptr;
            Player* other = (finisher == &pA) ? &pB : &pA;
            if (finisher) {
                settleScoreCards(*finisher, other->hand);
                other->hand.clear();
            }
            if (pA.totalScore > pB.totalScore) r.wins++;
            else if (pB.totalScore > pA.totalScore) r.losses++;
            else r.draws++;
            r.scoreFor += pA.totalScore;
            r.scoreAgainst += pB.totalScore;
            return r;
        }
        if (aDone) {
            settleScoreCards(pA, pB.hand);
            pB.hand.clear();
            r.wins++; r.scoreFor += pA.totalScore; r.scoreAgainst += pB.totalScore; return r;
        }
        if (bDone) {
            settleScoreCards(pB, pA.hand);
            pA.hand.clear();
            r.losses++; r.scoreFor += pA.totalScore; r.scoreAgainst += pB.totalScore; return r;
        }

        cur = lastP;
        opp = (cur == &pA) ? &pB : &pA;
    }

    r.scoreFor += pA.totalScore;
    r.scoreAgainst += pB.totalScore;
    if (pA.totalScore > pB.totalScore) r.wins++;
    else if (pB.totalScore > pA.totalScore) r.losses++;
    else r.draws++;
    return r;
}

int main() {
    const std::vector<int> ALL_AI = { 1, 2 };
    const int N = (int)ALL_AI.size();
    const std::string kSeparator(12 + 9 * N, '-');
    const int GAMES = 80;

    std::mt19937 rng(12345);

    auto makeAI = [&](int id) -> AIFn {
        return makeOldAI(id, 6);
    };

    std::cout << "            ";
    for (int j = 0; j < N; ++j) {
        std::cout << std::setw(7) << aiName(ALL_AI[j]);
    }
    std::cout << "\n";
    std::cout << kSeparator << "\n" << std::flush;

    for (int i = 0; i < N; ++i) {
        std::cout << std::setw(10) << aiName(ALL_AI[i]) << " |";
        for (int j = 0; j < N; ++j) {
            if (i == j) { std::cout << std::setw(7) << "---"; continue; }

            auto fnA = makeAI(ALL_AI[i]);
            auto fnB = makeAI(ALL_AI[j]);

            int aWins = 0;
            CardTracker tracker;

            for (int g = 0; g < GAMES; ++g) {
                Result r = runOneGame(fnA, fnB, tracker, rng);
                aWins += r.wins;
            }

            double wr = 100.0 * aWins / GAMES;
            std::cout << std::fixed << std::setprecision(0) << std::setw(6) << wr << "%" << std::flush;
        }
        std::cout << "\n";
    }

    return 0;
}