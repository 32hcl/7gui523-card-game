#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <random>
#include <functional>
#include <algorithm>
#include <map>

#include "core/card/deck.h"
#include "core/player.h"
#include "core/card/cardtype.h"
#include "core/rule/score.h"
#include "core/rule/special.h"
#include "ai/ai.h"
#include "ai/ai_types.h"
#include "ai/engine/ai_engine.h"
#include "ai/engine/ai_levels.h"

struct Result {
    int wins = 0;
    int losses = 0;
    int draws = 0;
    long long scoreFor = 0;
    long long scoreAgainst = 0;
};

using AIFn = std::function<std::vector<Card>(
    Player& cur, Player& opp,
    const CardTypeResult& prev,
    const Deck& deck,
    int tableScore,
    CardTracker& tracker)>;

static AIFn makeNewEngine(int level, int depthOverride) {
    return [level, depthOverride](Player& cur, Player& opp, const CardTypeResult& prev,
                   const Deck& deck, int tableScore, CardTracker&) -> std::vector<Card> {
        AIEngineConfig cfg = buildAIEngineConfig(level);
        if (depthOverride > 0) cfg.searchDepth = depthOverride;
        AIEngine engine(cfg);
        engine.setOpponentHand(opp.hand);
        return engine.choosePlay(cur, opp, prev, deck, tableScore);
    };
}

static AIFn makeOldAI(int level, int depthOverride) {
    return [level, depthOverride](Player& cur, Player& opp, const CardTypeResult& prev,
                   const Deck& deck, int tableScore, CardTracker& tracker) -> std::vector<Card> {
        if (level == 1) {
            return aiChoosePlayAI1(cur, prev);
        } else if (level == 2) {
            return aiChoosePlayAI2(cur, opp, prev, deck, tableScore);
        } else if (level == 3) {
            return aiChoosePlayAI3(cur, opp, prev, deck, tableScore, tracker);
        } else {
            // aiChoosePlay internally calls searchBestPlayCheat with depth 6
            // We can't easily override that without modifying ai.cpp,
            // but for comparison purposes the old AI serves as fixed baseline.
            return aiChoosePlay(cur, opp, prev, deck, tableScore, tracker);
        }
    };
}

static std::string aiName(int id) {
    if (id >= 100) return std::string("旧") + char('0' + (id - 99));
    return getAILevelName(id);
}

static Result runOneGame(AIFn fnA, AIFn fnB, CardTracker& tracker, std::mt19937& rng) {
    Result r;
    Deck deck = createStandardDeck();
    std::shuffle(deck.cards.begin(), deck.cards.end(), rng);

    Player pA = createPlayer("A");
    Player pB = createPlayer("B");

    dealCards(pA, deck, 5);
    dealCards(pB, deck, 5);

    if (checkSpecialVictory(pA)) { r.scoreFor += 500; r.scoreAgainst += 0; r.wins++; return r; }
    if (checkSpecialVictory(pB)) { r.scoreFor += 0; r.scoreAgainst += 500; r.losses++; return r; }

    Player* cur = &pA;
    Player* opp = &pB;
    Player* lastP = nullptr;
    std::vector<Card> table;
    CardTypeResult lp;
    lp = CardTypeResult{};

    int safety = 0;
    while (safety++ < 500) {
        while (true) {
            int ts = calculateScore(table);
            auto chosen = (cur == &pA) ? fnA(*cur, *opp, lp, deck, ts, tracker)
                                       : fnB(*cur, *opp, lp, deck, ts, tracker);
            if (chosen.empty()) {
                if (lp.type == CardType::Invalid) break;
                settleScoreCards(*lastP, table);
                table.clear();
                break;
            }

            auto parsed = parseCardType(chosen);
            if (parsed.type == CardType::Invalid) break;
            if (lp.type != CardType::Invalid && !canBeat(parsed, lp)) break;

            for (auto& c : chosen) {
                auto it = std::find_if(cur->hand.begin(), cur->hand.end(),
                    [&](const Card& h) { return h.point == c.point && h.suit == c.suit; });
                if (it != cur->hand.end()) cur->hand.erase(it);
            }
            for (auto& c : chosen) table.push_back(c);
            lp = parsed;
            lastP = cur;

            if (checkSpecialVictory(*cur)) {
                if (cur == &pA) { r.scoreFor += 500; r.scoreAgainst += pB.totalScore; r.wins++; }
                else             { r.scoreFor += pA.totalScore; r.scoreAgainst += 500; r.losses++; }
                return r;
            }

            if (cur->hand.empty()) {
                if (deck.cards.empty()) {
                    settleScoreCards(*cur, table);
                    settleScoreCards(*cur, opp->hand);
                    opp->hand.clear();
                    table.clear();
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

        if (!deck.cards.empty()) {
            Player* loser = (lastP == &pA) ? &pB : &pA;
            refillToFive(*lastP, deck);
            refillToFive(*loser, deck);
        }
        cur = lastP;
        opp = (cur == &pA) ? &pB : &pA;
        if (deck.cards.empty() && pA.hand.empty() && pB.hand.empty()) break;
    }

    r.scoreFor += pA.totalScore;
    r.scoreAgainst += pB.totalScore;
    if (pA.totalScore > pB.totalScore) r.wins++;
    else if (pB.totalScore > pA.totalScore) r.losses++;
    else r.draws++;
    return r;
}

int main() {
    const std::vector<int> ALL_AI = {
        1,2,3,4,5,6,7,8,9,
        101, 102, 103, 104
    };
    const int N = (int)ALL_AI.size();
    const int GAMES = 100;
    const int OVERRIDE_DEPTH = 2;
    g_searchBestPlayCheatDepth = (OVERRIDE_DEPTH > 0) ? OVERRIDE_DEPTH : 6;

    std::mt19937 rng(12345);

    std::cout << "            ";
    for (int j = 0; j < N; ++j) {
        std::cout << std::setw(7) << aiName(ALL_AI[j]);
    }
    std::cout << "\n";
    std::cout << std::string(12 + 7 * N, '-') << "\n" << std::flush;

    for (int i = 0; i < N; ++i) {
        std::cout << std::setw(10) << aiName(ALL_AI[i]) << " |";
        for (int j = 0; j < N; ++j) {
            if (i == j) { std::cout << std::setw(7) << "---"; continue; }
            if (i > j)  { std::cout << std::setw(7) << "";    continue; }

            auto fnA = (ALL_AI[i] >= 100) ? makeOldAI(ALL_AI[i] - 100, OVERRIDE_DEPTH) : makeNewEngine(ALL_AI[i], OVERRIDE_DEPTH);
            auto fnB = (ALL_AI[j] >= 100) ? makeOldAI(ALL_AI[j] - 100, OVERRIDE_DEPTH) : makeNewEngine(ALL_AI[j], OVERRIDE_DEPTH);

            int aWins = 0, bWins = 0, draws = 0;
            long long aScore = 0, bScore = 0;
            CardTracker tracker;

            for (int g = 0; g < GAMES; ++g) {
                Result r;
                if (g < GAMES / 2) r = runOneGame(fnA, fnB, tracker, rng);
                else               r = runOneGame(fnB, fnA, tracker, rng);
                aWins  += r.wins;
                bWins  += r.losses;
                draws  += r.draws;
                aScore += r.scoreFor;
                bScore += r.scoreAgainst;
            }

            double wr = 100.0 * aWins / GAMES;
            std::cout << std::fixed << std::setprecision(0) << std::setw(6) << wr << "%" << std::flush;
        }
        std::cout << "\n";
    }

    std::cout << "\n";
    return 0;
}