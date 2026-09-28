#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <random>
#include <functional>
#include <algorithm>
#include <map>
#include <memory>

#include "core/card/deck.h"
#include "core/player.h"
#include "core/card/cardtype.h"
#include "core/rule/score.h"
#include "core/rule/special.h"
#include "ai/ai.h"
#include "ai/ai_types.h"
#include "ai/engine/ai_engine.h"
#include "ai/engine/ai_levels.h"
#include "ai/players/ai1_idiot.h"
#include "ai/players/ai2_liar.h"
#include "game/campaign.h"

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
    auto enginePtr = std::make_shared<std::unique_ptr<AIEngine>>();
    return [level, depthOverride, enginePtr, init = true]
           (Player& cur, Player& opp, const CardTypeResult& prev,
            const Deck& deck, int tableScore, CardTracker&) mutable -> std::vector<Card> {
        constexpr int kTotalCards = 54;
        constexpr int kDecksAfterFirstRound = kTotalCards - kMaxHandSize * 2 - 2;
        if (init || (prev.type == CardType::Invalid && deck.cards.size() == kDecksAfterFirstRound)) {
            AIEngineConfig cfg = buildAIEngineConfig(level);
            if (depthOverride > 0) cfg.searchDepth = depthOverride;
            *enginePtr = std::make_unique<AIEngine>(cfg);
            init = false;
        }
        (*enginePtr)->setOpponentHand(opp.hand);
        return (*enginePtr)->choosePlay(cur, opp, prev, deck, tableScore);
    };
}

static AIFn makeOldAI(int level, int /*depthOverride*/) {
    return [level](Player& cur, Player& opp, const CardTypeResult& prev,
                   const Deck& deck, int tableScore, CardTracker& tracker) -> std::vector<Card> {
        if (level == 2) {
            return aiChoosePlayAI2(cur, opp, prev, deck, tableScore);
        } else {
            return ai1_idiot_choose(cur, opp, prev, deck, tableScore);
        }
    };
}

static std::string aiName(int id) {
    if (id == 105) return "傻子";
    if (id >= 100) return std::string("旧") + char('0' + (id - 99));
    return getAILevelName(id);
}

static Result runOneGame(AIFn fnA, AIFn fnB, CardTracker& tracker, std::mt19937& rng) {
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
                    // Endgame: finish immediately, collect opponent's remaining cards as bonus
                    settleScoreCards(*cur, table);
                    cur->totalScore += tableBonus;
                    // 结束补分：收对手手牌分数
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

        // Refill from own decks
        refillToFive(pA, myDeck);
        refillToFive(pB, oppDeck);
        myDeck.myRemaining = (int)myDeck.cards.size();
        oppDeck.oppRemaining = (int)oppDeck.cards.size();

        // Check endgame: both players out of deck and hand
        bool aDone = myDeck.cards.empty() && pA.hand.empty();
        bool bDone = oppDeck.cards.empty() && pB.hand.empty();
        if (aDone && bDone) {
            // Both done at same time: firstEmpty decides who gets bonus
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
            // A done, B has cards
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

static bool runCampaignGame(AIFn fnPlayer, int level, std::mt19937& rng) {
    auto combinedDeck = (level == 1)
        ? createStandardDeck().cards
        : removeCards(createStandardDeck().cards, getLevelRemoveTable(level));
    if (level == 2) ai2_variant_deck(combinedDeck);
    std::shuffle(combinedDeck.begin(), combinedDeck.end(), rng);

    std::vector<Card> pDeck(combinedDeck.begin(), combinedDeck.begin() + 27);
    std::vector<Card> bDeck(combinedDeck.begin() + 27, combinedDeck.end());
    int pRemain = (int)pDeck.size(), bRemain = (int)bDeck.size();

    Player player = createPlayer("玩家");
    Player boss   = createPlayer("Boss");

    auto deal = [](Player& p, std::vector<Card>& d, int& rem, int n) {
        n = std::min(n, (int)d.size());
        p.hand.insert(p.hand.end(), d.end() - n, d.end());
        d.erase(d.end() - n, d.end());
        rem = (int)d.size();
    };
    deal(player, pDeck, pRemain, 5);
    deal(boss,   bDeck,  bRemain, 5);

    if (checkSpecialVictory(player)) return true;
    if (checkSpecialVictory(boss))   return false;

    bool playerFirst = (level != 3);
    Player *cur  = playerFirst ? &player : &boss;
    Player *opp  = playerFirst ? &boss   : &player;
    Player *lastP = nullptr;
    std::vector<Card> table;
    int tableBonus = 0;
    CardTypeResult lp;
    int firstEmpty = 0;
    CardTracker tracker;

    auto refill = [](Player& p, std::vector<Card>& d, int& rem) {
        while (p.hand.size() < 5 && !d.empty() && rem > 0) {
            p.hand.push_back(d.back());
            d.pop_back(); rem--;
        }
    };

    int safety = 0;
    while (safety++ < 500) {
        while (true) {
            int ts = calculateScore(table);
            std::vector<Card> chosen;

            if (cur == &player) {
                Deck merged;
                merged.cards.assign(pDeck.begin(), pDeck.end());
                merged.cards.insert(merged.cards.end(), bDeck.begin(), bDeck.end());
                merged.myRemaining = pRemain;
                merged.oppRemaining = bRemain;
                chosen = fnPlayer(*cur, *opp, lp, merged, ts, tracker);
            } else {
                Deck bossView;
                bossView.cards = bDeck;
                bossView.myRemaining = bRemain;
                bossView.oppRemaining = pRemain;
                if (level == 2)
                    chosen = ai2_liar_choose(*cur, *opp, lp, bossView, ts);
                else
                    chosen = ai1_idiot_choose(*cur, *opp, lp, bossView, ts);
            }

            if (chosen.empty()) {
                if (lp.type == CardType::Invalid) break;
                settleScoreCards(*lastP, table);
                lastP->totalScore += tableBonus;
                table.clear(); tableBonus = 0;
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
            for (auto& c : chosen) table.push_back(c);
            lp = parsed; lastP = cur;

            if (checkSpecialVictory(*cur)) return cur == &player;

            if (cur->hand.empty()) {
                if (!firstEmpty) firstEmpty = (cur == &player) ? 1 : -1;
                int& rem = (cur == &player) ? pRemain : bRemain;
                if (rem == 0) {
                    settleScoreCards(*cur, table);
                    cur->totalScore += tableBonus;
                    settleScoreCards(*cur, opp->hand);
                    opp->hand.clear();
                    table.clear(); tableBonus = 0;
                    return player.totalScore > boss.totalScore;
                }
            }

            std::swap(cur, opp);
        }

        lp = CardTypeResult{};
        if (checkSpecialVictory(player)) return true;
        if (checkSpecialVictory(boss))   return false;

        refill(player, pDeck, pRemain);
        refill(boss,   bDeck,  bRemain);

        bool pDone = pDeck.empty() && pRemain == 0 && player.hand.empty();
        bool bDone = bDeck.empty() && bRemain == 0 && boss.hand.empty();
        if (pDone && bDone) {
            Player *finisher = (firstEmpty == 1) ? &player
                             : (firstEmpty == -1) ? &boss : nullptr;
            Player *other = (finisher == &player) ? &boss : &player;
            if (finisher) { settleScoreCards(*finisher, other->hand); other->hand.clear(); }
            return player.totalScore > boss.totalScore;
        }
        if (pDone) { settleScoreCards(player, boss.hand); boss.hand.clear(); return true; }
        if (bDone) { settleScoreCards(boss, player.hand); player.hand.clear(); return false; }

        cur = lastP;
        opp = (cur == &player) ? &boss : &player;
    }

    return player.totalScore > boss.totalScore;
}

int main() {
    const std::vector<int> ALL_AI = {
        3, 5, 8, 9, 102, 105
    };
    const int N = (int)ALL_AI.size();
    const int GAMES = 80;
    const int OVERRIDE_DEPTH = 6;

    std::mt19937 rng(12345);

    auto makeAI = [&](int id) -> AIFn {
        if (id >= 100) return makeOldAI(id - 100, OVERRIDE_DEPTH);
        return makeNewEngine(id, OVERRIDE_DEPTH);
    };

    std::cout << "            ";
    for (int j = 0; j < N; ++j) {
        std::cout << std::setw(7) << aiName(ALL_AI[j]);
    }
    std::cout << "\n";
    std::cout << std::string(12 + 7 * N, '-') << "\n" << std::flush;

    // 11×11 全矩阵，行先手
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

    std::cout << "\n";

    const int CAMPAIGN_GAMES = 80;
    const char* levelNames[] = {"", "关卡1-傻子", "关卡2-骗子", "关卡3-急性子"};
    std::cout << "=== 关卡通关率 (" << CAMPAIGN_GAMES << "局, 玩家先手) ===\n\n";
    std::cout << "            ";
    for (int lv = 1; lv <= 3; ++lv) std::cout << std::setw(14) << levelNames[lv];
    std::cout << "\n" << std::string(12 + 14 * 3, '-') << "\n" << std::flush;

    for (int aiIdx = 0; aiIdx < N; ++aiIdx) {
        int aiId = ALL_AI[aiIdx];
        std::cout << std::setw(10) << aiName(aiId) << " |";
        for (int lv = 1; lv <= 3; ++lv) {
            auto fnPlayer = makeAI(aiId);
            int wins = 0;
            for (int g = 0; g < CAMPAIGN_GAMES; ++g) {
                if (runCampaignGame(fnPlayer, lv, rng)) wins++;
            }
            double wr = 100.0 * wins / CAMPAIGN_GAMES;
            std::cout << std::fixed << std::setprecision(0) << std::setw(13) << wr << "%" << std::flush;
        }
        std::cout << "\n";
    }

    std::cout << "\n";
    return 0;
}