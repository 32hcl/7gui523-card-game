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
#include "core/card/rank.h"
#include "core/rule/score.h"
#include "core/rule/special.h"
#include "ai/ai.h"
#include "ai/ai_types.h"
#include "ai/engine/ai_engine.h"
#include "ai/ai_levels.h"
#include "ai/players/ai1_idiot.h"
#include "ai/players/ai2_liar.h"

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
    if (id == 3)   return "搜索";
    if (id == 102) return "贪心";
    if (id == 105) return "保守";
    return "?";
}

static bool runCampaignGame(AIFn fnPlayer, int level, std::mt19937& rng) {
    auto combinedDeck = createStandardDeck().cards;
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

    bool playerFirst;
    if (level == 3) {
        playerFirst = false;
    } else {
        playerFirst = (rng() % 2 == 0);
    }
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
                // Boss特殊能力：Pass前尝试非压制性出牌
                if (cur == &boss && lp.type != CardType::Invalid) {
                    // 不识数 (level 6): 50%概率挑任意单张（同牌型但不大于上家）强行出
                    if (level == 6 && lp.type == CardType::Single && (rng() % 2 == 0)) {
                        auto sorted = cur->hand;
                        if (sorted.empty()) break;
                        std::sort(sorted.begin(), sorted.end(), [](const Card& a, const Card& b) {
                            return getCardRank(a.point) < getCardRank(b.point);
                        });
                        chosen = {sorted[0]};
                    }
                    // 赖账鬼 (level 5): 上家单张<J时，Boss出同花色单张（无视点数）
                    if (level == 5 && lp.type == CardType::Single && !lp.cards.empty() &&
                        getCardRank(lp.cards[0].point) < getCardRank("J")) {
                        auto it = std::find_if(cur->hand.begin(), cur->hand.end(),
                            [&](const Card& c) { return c.suit == lp.cards[0].suit; });
                        if (it != cur->hand.end()) chosen = {*it};
                    }
                }

                if (chosen.empty()) {
                    if (lp.type == CardType::Invalid) break;
                    settleScoreCards(*lastP, table);
                    lastP->totalScore += tableBonus;
                    table.clear(); tableBonus = 0;
                    break;
                }
            }

            auto parsed = parseCardType(chosen);
            if (parsed.type == CardType::Invalid) break;

            bool canPass = canBeat(parsed, lp);

            // 不识数 (level 6): 50%概率牌型相同即可压
            if (!canPass && level == 6 && cur == &boss && parsed.type == lp.type) {
                if (rng() % 2 == 0) canPass = true;
            }

            // 赖账鬼 (level 5): 同花色单张压制（无视点数）
            if (!canPass && level == 5 && cur == &boss &&
                lp.type == CardType::Single && parsed.type == CardType::Single &&
                !lp.cards.empty() && !parsed.cards.empty() &&
                getCardRank(lp.cards[0].point) < getCardRank("J") &&
                lp.cards[0].suit == parsed.cards[0].suit) {
                canPass = true;
            }

            if (lp.type != CardType::Invalid && !canPass) break;

            // 赖账鬼 (level 5): 玩家不能用相同点数压
            if (level == 5 && cur == &player && parsed.keyPoint == lp.keyPoint) break;

            // 疯狗 (level 4): 出牌数×3伤害
            if (level == 4 && cur == &boss) {
                int dogDamage = (int)chosen.size() * 3;
                player.totalScore -= dogDamage;
            }

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

        bool pDone = pDeck.empty() && player.hand.empty();
        bool bDone = bDeck.empty() && boss.hand.empty();
        if (pDone && bDone) {
            Player* finisher = (firstEmpty == 1) ? &player : (firstEmpty == -1) ? &boss : nullptr;
            Player* other = (finisher == &player) ? &boss : &player;
            if (finisher) { settleScoreCards(*finisher, other->hand); other->hand.clear(); }
            return player.totalScore > boss.totalScore;
        }
        if (pDone) { settleScoreCards(player, boss.hand); boss.hand.clear(); return true; }
        if (bDone) { settleScoreCards(boss, player.hand); player.hand.clear(); return false; }

        if (level == 3) {
            cur = &boss;
            opp = &player;
        } else {
            cur = lastP;
            opp = (cur == &player) ? &boss : &player;
        }
    }

    return player.totalScore > boss.totalScore;
}

int main() {
    const std::vector<int> ALL_AI = { 3, 102, 105 };
    const int N = (int)ALL_AI.size();
    const int CAMPAIGN_GAMES = 100;
    const int OVERRIDE_DEPTH = 6;
    const char* levelNames[] = {"关卡1-新手", "关卡2-换牌虫", "关卡3-急眼雀", "关卡4-疯狗", "关卡5-赖账鬼", "关卡6-不识数"};

    std::mt19937 rng(12345);

    auto makeAI = [&](int id) -> AIFn {
        if (id >= 100) return makeOldAI(id - 100, OVERRIDE_DEPTH);
        return makeNewEngine(id, OVERRIDE_DEPTH);
    };

    std::cout << "=== 关卡通关率 (" << CAMPAIGN_GAMES << "局) ===\n";

    std::cout << "\n--- 关卡限制验证 ---\n";
    std::cout << "  牌堆: 三关统一标准54张, 双方各27张 ✓\n";
    std::cout << "  关卡1-Boss AI: 新手 (ai1_idiot, 永远出最小单张), 随机先手 ✓\n";
    std::cout << "  关卡2-Boss AI: 换牌虫 (ai2_liar, 可换牌), 随机先手 ✓\n";
    std::cout << "  关卡3-Boss AI: 急眼雀 (ai1_idiot, 永远出最小单张), Boss始终先手 ✓\n";
    std::cout << "  关卡4-Boss AI: 疯狗 (出牌数×3伤害 + ai1_idiot), 随机先手 ✓\n";
    std::cout << "  关卡5-Boss AI: 赖账鬼 (禁用同点数压牌 + 同花色单张压 + ai1_idiot), 随机先手 ✓\n";
    std::cout << "  关卡6-Boss AI: 不识数 (50%牌型相同即可压 + ai1_idiot), 随机先手 ✓\n";
    std::cout << "  关卡1/2 后续回合: 上一回合赢家先手 ✓\n";
    std::cout << "--- 开始测试 ---\n\n";

    // ===== 全部关卡 (全部AI) =====
    std::cout << ">>> 全部关卡 (全部AI) <<<\n";
    std::cout << "            ";
    for (int lv = 1; lv <= 6; ++lv) std::cout << std::setw(13) << levelNames[lv - 1];
    std::cout << "\n" << std::string(12 + 13 * 6, '-') << "\n" << std::flush;

    for (int aiIdx = 0; aiIdx < N; ++aiIdx) {
        int aiId = ALL_AI[aiIdx];
        std::cout << std::setw(10) << aiName(aiId) << " |";
        for (int lv = 1; lv <= 6; ++lv) {
            auto fnPlayer = makeAI(aiId);
            int wins = 0;
            for (int g = 0; g < CAMPAIGN_GAMES; ++g) {
                if (runCampaignGame(fnPlayer, lv, rng)) wins++;
            }
            double wr = 100.0 * wins / CAMPAIGN_GAMES;
            std::cout << std::fixed << std::setprecision(0) << std::setw(12) << wr << "%" << std::flush;
        }
        std::cout << "\n";
    }

    std::cout << "\n";
    return 0;
}