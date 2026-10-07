#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <string>
#include <string_view>
#include <random>
#include <functional>
#include <algorithm>
#include <memory>
#include <sstream>
#include <cstdio>

#include "core/card/deck.h"
#include "core/player.h"
#include "core/card/cardtype.h"
#include "core/card/rank.h"
#include "core/rule/score.h"
#include "core/rule/special.h"
#include "ai/ai.h"
#include "ai/ai_types.h"
#include "ai/ai_levels.h"
#include "game/level_boss.h"
#include "ai/plugins/level_rule.h"
#include "tests/test_common.h"

struct CampaignResult {
    bool playerWon;
    int hpRemaining;
    bool abnormal = false;
};

static std::string fmtCards(const std::vector<Card>& cards) {
    std::ostringstream ss;
    for (size_t i = 0; i < cards.size(); ++i) {
        if (i > 0) ss << ' ';
        if (cards[i].suit.empty())
            ss << cards[i].point;
        else
            ss << cards[i].suit << cards[i].point;
    }
    return ss.str();
}

static std::string levelTitle(int level) {
    const char* names[] = {"","新手","换牌虫","急眼雀","疯狗","赖账鬼","不识数","黑老大","千里眼","赌场老板"};
    char buf[64];
    snprintf(buf, sizeof(buf), "关卡%d-%s", level, names[level]);
    return buf;
}

static std::string_view levelRulesIntro(int level) {
    switch (level) {
    case 1: return "无特殊规则，Boss AI=新手(永远出最小单张)。通关=打空Boss血量(110)。";
    case 2: return "Boss换牌虫：开局将手中所有4和6替换为A~10中其他点数(排除4/6)，可能产生五同点。AI=新手。";
    case 3: return "Boss急眼雀：每轮必定先手出牌。AI=新手。";
    case 4: return "⚠ 疯狗：Boss每出一张牌咬玩家3血(对子6血,三条9血…)，轮末结算。AI=新手。";
    case 5: return "⚠ 赖账鬼：(1)玩家禁出同点数压牌 (2)Boss可用同花色任意牌压J以下单张。AI=新手。";
    case 6: return "⚠ 不识数：(1)Pass前50%强行出同牌型最小牌 (2)压牌50%牌型相同即可(无视大小)。AI=新手。";
    case 7: return "⚠ 黑老大：Boss先手出完牌时强制弃玩家最大牌(同点随机花色)，摸一张。AI=新手。";
    case 8: return "⚠ 千里眼：Boss能看到玩家手牌，AI=贪心。";
    case 9: return "⚠ 赌场老板：所有伤害(含得分扣血)全扣玩家HP，Boss不扣血！AI=新手。";
    default: return "";
    }
}

static std::string explainScore(int cardScore, int bonusScore, const std::vector<Card>& table) {
    int total = cardScore + bonusScore;
    if (total == 0) return "";
    std::ostringstream ss;
    ss << " (";
    int tableScore = 0;
    int ks = 0, tens = 0, fives = 0;
    for (const auto& c : table) {
        tableScore += c.score;
        if (c.score == 20) ks++;
        else if (c.score == 10) tens++;
        else if (c.score == 5) fives++;
    }
    bool mismatch = (tableScore != cardScore);
    if (mismatch) {
        ss << "⚠TABLE_SCORE_MISMATCH[" << cardScore << "≠" << tableScore << "]⚠ ";
    }
    bool first = true;
    if (ks > 0)   { if (!first) ss << '+'; ss << ks << "张K"; first = false; }
    if (tens > 0) { if (!first) ss << '+'; ss << tens << "张10"; first = false; }
    if (fives > 0){ if (!first) ss << '+'; ss << fives << "张5"; first = false; }
    if (tableScore > 0) ss << "=" << tableScore << "分";
    if (bonusScore > 0) {
        if (tableScore > 0) ss << "+";
        ss << "压分奖励" << bonusScore << "分";
    }
    ss << "=共" << (tableScore + bonusScore) << "分)";
    return ss.str();
}

static const char* cardTypeRulesIntro() {
    return "【牌型(大到小)】王炸 > Special523(7+鬼+5+2+3) > 五同点 > 炸弹 > 三带二 > 三带一 > 对子 > 单张\n"
           "  同牌型比点数:7>大鬼>小鬼>5>2>3>A>K>Q>J>10>9>8>6>4 | 三带一/二同三条比带的牌\n"
           "  得分牌:K=20分, 10=10分, 5=5分 | 压分规则:压住有分牌时额外得等量分";
}

static CampaignResult runCampaignGame(const AIFn& fnPlayer, int level, std::mt19937& rng,
                                       int gameIdx = -1, const std::string& aiLabel = "",
                                       std::ostream* log = nullptr) {
    const int kStartHp = 140;
    int playerHp = kStartHp;
    int bossHp = 110;
    int bossDamageAcc = 0;

    struct HpResult { bool done; int playerHp; int dmgDone; bool playerWon; };

    auto applyDmg = [&](int dmgToBoss, int dmgToPlayer) -> HpResult {
        if (level == 9) {
            playerHp -= dmgToBoss + dmgToPlayer;
            playerHp -= bossDamageAcc;
        } else {
            bossHp -= dmgToBoss;
            playerHp -= dmgToPlayer;
            playerHp -= bossDamageAcc;
        }
        int dmg = bossDamageAcc;
        bossDamageAcc = 0;
        bool done = (playerHp <= 0 || bossHp <= 0);
        bool playerWon = (playerHp > 0 && bossHp <= 0);
        if (done && log) {
            *log << "  >>> " << (playerHp <= 0 ? "玩家HP归零!" : "BossHP归零!") << " <<<\n";
        }
        return {done, playerHp, dmg, playerWon};
    };

    auto combinedDeck = createStandardDeck().cards;
    std::shuffle(combinedDeck.begin(), combinedDeck.end(), rng);

    std::vector<Card> pDeck(combinedDeck.begin(), combinedDeck.begin() + 27);
    std::vector<Card> bDeck(combinedDeck.begin() + 27, combinedDeck.end());

    // 关卡2: Boss换牌作弊，将4/6替换为A~10中其他点数
    if (level == 2) {
        int swappedCount = 0;
        for (auto& c : bDeck) {
            if (c.point == "4" || c.point == "6") swappedCount++;
        }
        ai2_variant_deck(bDeck);
        if (log) {
            *log << " [换牌虫作弊] Boss将" << swappedCount << "张4/6换成了A~10中其他点数的牌\n";
        }
    }

    int pRemain = (int)pDeck.size(), bRemain = (int)bDeck.size();

    Player player = createPlayer("玩家");
    Player boss   = createPlayer("Boss");
    LevelRule rule = createLevelRule(level);

    auto deal = [](Player& p, std::vector<Card>& d, int& rem, int n) {
        n = std::min(n, (int)d.size());
        p.hand.insert(p.hand.end(), d.end() - n, d.end());
        d.erase(d.end() - n, d.end());
        rem = (int)d.size();
    };
    deal(player, pDeck, pRemain, 5);
    deal(boss,   bDeck,  bRemain, 5);

    if (log) {
        *log << "========================================\n";
        *log << " " << levelTitle(level) << ", " << aiLabel << "AI, 第" << (gameIdx + 1) << "局\n";
        if (gameIdx == 0) {
            *log << " 通用规则: " << cardTypeRulesIntro() << "\n";
        }
        *log << " 关卡规则: " << levelRulesIntro(level) << "\n";
        *log << " 血量: 玩家 140 | Boss 110\n";
        *log << " 胜负: BossHP≤0→通关; 玩家HP≤0→失败\n";
        *log << "========================================\n";
    }

    if (checkSpecialVictory(player)) {
        if (log) *log << "开场特殊胜利! → 玩家\n";
        return {true, playerHp};
    }
    if (checkSpecialVictory(boss)) {
        if (log) *log << "开场特殊胜利! → Boss\n";
        return {false, 0};
    }

    bool playerFirst;
    if (level == 3) {
        playerFirst = false;
    } else {
        playerFirst = (rng() % 2 == 0);
    }
    if (log) {
        *log << "先手: " << (playerFirst ? "玩家" : "Boss") << "\n\n";
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
    int roundNum = 0;
    while (safety++ < 500) {
        ++roundNum;
        if (log) {
            *log << "[R" << roundNum << "] ";
            const char* firstLabel = (roundNum == 1)
                ? (playerFirst ? "玩家先" : "Boss先")
                : (cur == &player ? "玩家先" : "Boss先");
            *log << firstLabel;
            if (level == 3 && roundNum > 1)
                *log << " (急眼雀每轮必定抢先)";
            if (!table.empty()) *log << " (续)";
            *log << " |\n";
        }
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
                if (level == 8) {
                    Deck mergedView;
                    mergedView.cards = bDeck;
                    mergedView.myRemaining = bRemain;
                    mergedView.oppRemaining = pRemain;
                    chosen = fnPlayer(*cur, *opp, lp, mergedView, ts, tracker);
                } else if (level == 2)
                    chosen = ai2_liar_choose(*cur, *opp, lp, bossView, ts);
                else
                    chosen = ai1_idiot_choose(*cur, *opp, lp, bossView, ts);
            }

            bool forcePlayed = false;

            if (chosen.empty()) {
                // Boss特殊能力：Pass前尝试强制出牌
                if (cur == &boss && lp.type != CardType::Invalid && rule.tryForcePlay) {
                    chosen = rule.tryForcePlay(*cur, lp, rng);
                    if (!chosen.empty() && log) {
                        *log << "  [Boss触发: " << (rule.forcePlayMsg ? rule.forcePlayMsg : "强制出牌") << "]\n";
                    }
                }

                // forcePlay 返回的牌跳过 canBeat 检查
                forcePlayed = !chosen.empty();

                if (chosen.empty()) {
                    if (lp.type == CardType::Invalid) {
                        // 新回合首玩家无法出牌：交换先手，给对方机会
                        std::swap(cur, opp);
                        if (log) *log << "  (双方无法出牌, 交换先手继续)\n";
                        break;
                    }
                    int cardScore = calculateScore(table);
                    int roundScore = cardScore + tableBonus;
                    settleScoreCards(*lastP, table);
                    lastP->totalScore += tableBonus;
                    auto r = applyDmg(lastP == &player ? roundScore : 0,
                                      lastP == &player ? 0 : roundScore);
                    if (log && (roundScore > 0 || r.dmgDone > 0)) {
                        const char* whoPass = (cur == &player) ? "玩家" : "Boss";
                        const char* winner = (lastP == &player) ? "玩家" : "Boss";
                        *log << "  " << whoPass << " → PASS\n"
                             << "  === " << winner << "赢"
                             << explainScore(cardScore, tableBonus, table);
                        if (r.dmgDone > 0) *log << " [疯狗撕咬总计: -" << r.dmgDone << "血]";
                        *log << " | 玩家HP:" << playerHp
                             << ", BossHP:" << bossHp << " ===\n\n";
                    } else if (log) {
                        const char* whoPass = (cur == &player) ? "玩家" : "Boss";
                        *log << "  " << whoPass << " → PASS (桌面无分牌)\n\n";
                    }
                    if (r.done) return {r.playerWon, r.playerHp};
                    table.clear(); tableBonus = 0;
                    break;
                }
            }

            auto parsed = parseCardType(chosen);
            if (parsed.type == CardType::Invalid) break;

            bool canPass = canBeat(parsed, lp);

            // Boss特权：canBeat 放行（不识数 50%牌型相同 / 赖账鬼 同花色单张）
            if (!canPass && cur == &boss && rule.canBeatOverride) {
                canPass = rule.canBeatOverride(parsed, lp, rng);
                if (canPass && log) {
                    *log << "  [Boss特权: " << (rule.canBeatOverrideMsg ? rule.canBeatOverrideMsg : "无视规则压牌") << "]\n";
                }
            }

            // forcePlay 返回的牌：即使 canBeat 失败也强制打出
            if (!canPass && forcePlayed) {
                canPass = true;
            }

            if (lp.type != CardType::Invalid && !canPass) break;

            // 玩家限制
            if (cur == &player && rule.blockPlayer && rule.blockPlayer(parsed, lp)) {
                if (log) {
                    *log << "  [玩家受阻: " << (rule.blockPlayerMsg ? rule.blockPlayerMsg : "无法出这些牌")
                         << " | 尝试出: " << fmtCards(chosen) << " (" << cardTypeToString(parsed.type) << ")]\n";
                }
                // 受阻等同于pass：本轮结束，对方收走桌面牌
                if (lp.type != CardType::Invalid && lastP) {
                    int cardScore = calculateScore(table);
                    int roundScore = cardScore + tableBonus;
                    settleScoreCards(*lastP, table);
                    lastP->totalScore += tableBonus;
                    auto r = applyDmg(lastP == &player ? roundScore : 0,
                                      lastP == &player ? 0 : roundScore);
                    if (log) {
                        const char* winner = (lastP == &player) ? "玩家" : "Boss";
                        *log << "  [玩家受阻→本轮强制结束]\n";
                        if (roundScore > 0 || r.dmgDone > 0) {
                            *log << "  === " << winner << "赢"
                                 << explainScore(cardScore, tableBonus, table);
                            if (r.dmgDone > 0) *log << " [疯狗撕咬总计: -" << r.dmgDone << "血]";
                            *log << " | 玩家HP:" << playerHp << ", BossHP:" << bossHp << " ===\n\n";
                        } else {
                            *log << "\n";
                        }
                    }
                    if (r.done) return {r.playerWon, r.playerHp};
                }
                table.clear(); tableBonus = 0;
                break;
            }

            // Boss 出牌附加伤害 → 计入本回合 HP 伤害
            int bossDmgThisTurn = 0;
            if (cur == &boss && rule.bossDamage) {
                bossDmgThisTurn = rule.bossDamage(chosen);
                bossDamageAcc += bossDmgThisTurn;
            }

            tableBonus += calculatePressureBonus(parsed, lp);

            for (auto& c : chosen) {
                auto it = std::find_if(cur->hand.begin(), cur->hand.end(),
                    [&](const Card& h) { return h.point == c.point && h.suit == c.suit; });
                if (it != cur->hand.end()) cur->hand.erase(it);
            }
            for (auto& c : chosen) table.push_back(c);
            lp = parsed; lastP = cur;

            if (log) {
                const char* who = (cur == &player) ? "玩家" : "Boss";
                *log << "  " << who << " → " << fmtCards(chosen)
                     << " (" << cardTypeToString(parsed.type) << ")";
                if (cur == &boss && rule.bossDamage) {
                    *log << " [疯狗撕咬 +" << bossDmgThisTurn << "血(本局累计:" << bossDamageAcc << "血)]";
                }
                *log << " [手牌剩" << (int)cur->hand.size() << "张]";
                *log << "\n";
            }

            if (checkSpecialVictory(*cur)) {
                return {cur == &player, cur == &player ? playerHp : 0};
            }

            if (cur->hand.empty()) {
                if (cur == &boss && rule.onBossFinish) {
                    auto discarded = rule.onBossFinish(opp->hand, pDeck, rng);
                    if (log && !discarded.empty()) {
                        *log << "  [黑老大: 弃你最大牌(" << fmtCards(discarded)
                             << "), 摸一张, 手牌剩" << (int)opp->hand.size() << "张]\n";
                    }
                }
                if (!firstEmpty) firstEmpty = (cur == &player) ? 1 : -1;
                int& rem = (cur == &player) ? pRemain : bRemain;
                if (rem == 0) {
                    int cardScore = calculateScore(table) + calculateScore(opp->hand);
                    int finalScore = cardScore + tableBonus;
                    settleScoreCards(*cur, table);
                    cur->totalScore += tableBonus;
                    settleScoreCards(*cur, opp->hand);
                    auto r = applyDmg(cur == &player ? finalScore : 0,
                                      cur == &player ? 0 : finalScore);
                    if (log) {
                        *log << "  === 双方打完! ";
                        if (cur == &player) *log << "玩家先出完 → BossHP -" << finalScore;
                        else               *log << "Boss先出完 → 玩家HP -" << finalScore;
                        auto allCards = table;
                        allCards.insert(allCards.end(), opp->hand.begin(), opp->hand.end());
                        opp->hand.clear();
                        *log << explainScore(cardScore, tableBonus, allCards);
                        if (r.dmgDone > 0)
                            *log << ", 玩家HP -" << r.dmgDone << " (疯狗撕咬总计)";
                        *log << " | 玩家HP:" << playerHp << ", BossHP:" << bossHp << " ===\n";
                    }
                    if (r.done) {
                        if (log) *log << "  最终判定: BossHP=" << bossHp << (bossHp > 0 ? " (未归零→未通关)" : " →通关") << "\n";
                        return {r.playerWon, r.playerHp};
                    }
                    table.clear(); tableBonus = 0;
                    // 关卡胜负只看HP：BOSS血量没空=没通关
                    if (log) *log << "  最终判定: BossHP=" << bossHp << (bossHp > 0 ? " (未归零→未通关)" : " →通关") << "\n";
                    return {bossHp <= 0, playerHp};
                }
            }

            std::swap(cur, opp);
        }

        lp = CardTypeResult{};
        if (checkSpecialVictory(player)) return {true, playerHp};
        if (checkSpecialVictory(boss))   return {false, 0};

        refill(player, pDeck, pRemain);
        refill(boss,   bDeck,  bRemain);

        bool pDone = pDeck.empty() && player.hand.empty();
        bool bDone = bDeck.empty() && boss.hand.empty();
        if (pDone && bDone) {
            Player* finisher = (firstEmpty == 1) ? &player : (firstEmpty == -1) ? &boss : nullptr;
            Player* other = (finisher == &player) ? &boss : &player;
            if (finisher) {
                int finalScore = calculateScore(other->hand);
                settleScoreCards(*finisher, other->hand);
                other->hand.clear();
                auto r = applyDmg(finisher == &player ? finalScore : 0,
                                  finisher == &player ? 0 : finalScore);
                if (log) {
                    *log << "  === 终局: " << (finisher == &player ? "玩家" : "Boss")
                         << "先打完, 收走对手手牌";
                    if (finalScore > 0) {
                        if (finisher == &player) *log << " → BossHP -" << finalScore;
                        else                     *log << " → 玩家HP -" << finalScore;
                    }
                    if (r.dmgDone > 0)
                        *log << " [疯狗撕咬总计: 玩家HP -" << r.dmgDone << "]";
                    *log << " | 玩家HP:" << playerHp << ", BossHP:" << bossHp << " ===\n";
                }
                if (r.done) {
                    if (log) *log << "  最终判定: BossHP=" << bossHp << (bossHp > 0 ? " (未归零→未通关)" : " →通关") << "\n";
                    return {r.playerWon, r.playerHp};
                }
            }
            if (log) *log << "  最终判定: BossHP=" << bossHp << (bossHp > 0 ? " (未归零→未通关)" : " →通关") << "\n";
            return {bossHp <= 0, playerHp};
        }
        if (pDone) {
            int finalScore = calculateScore(boss.hand);
            settleScoreCards(player, boss.hand); boss.hand.clear();
            auto r = applyDmg(finalScore, 0);
            if (log) {
                *log << "  === 终局: 玩家先打完, 收走Boss手牌";
                if (finalScore > 0) *log << " → BossHP -" << finalScore;
                if (r.dmgDone > 0)  *log << " [疯狗撕咬总计: 玩家HP -" << r.dmgDone << "]";
                *log << " | 玩家HP:" << playerHp << ", BossHP:" << bossHp << " ===\n";
            }
            if (r.done) {
                if (log) *log << "  最终判定: BossHP=" << bossHp << (bossHp > 0 ? " (未归零→未通关)" : " →通关") << "\n";
                return {r.playerWon, r.playerHp};
            }
            if (log) *log << "  最终判定: BossHP=" << bossHp << (bossHp > 0 ? " (未归零→未通关)" : " →通关") << "\n";
            return {bossHp <= 0, playerHp};
        }
        if (bDone) {
            int finalScore = calculateScore(player.hand);
            settleScoreCards(boss, player.hand); player.hand.clear();
            auto r = applyDmg(0, finalScore);
            if (log) {
                *log << "  === 终局: Boss先打完, 收走玩家手牌";
                if (finalScore > 0) *log << " → 玩家HP -" << finalScore;
                if (r.dmgDone > 0)  *log << " [疯狗撕咬总计: 玩家HP -" << r.dmgDone << "]";
                *log << " | 玩家HP:" << playerHp << ", BossHP:" << bossHp << " ===\n";
            }
            if (r.done) {
                if (log) *log << "  最终判定: Boss先出完,BossHP=" << bossHp << (bossHp > 0 ? " (未归零→未通关)" : " →通关") << "\n";
                return {r.playerWon, r.playerHp};
            }
            if (log) *log << "  最终判定: Boss先出完,BossHP=" << bossHp << (bossHp > 0 ? " (未归零→未通关)" : " →通关") << "\n";
            return {bossHp <= 0, playerHp};
        }

        if (level == 3) {
            cur = &boss;
            opp = &player;
        } else {
            cur = lastP;
            opp = (cur == &player) ? &boss : &player;
        }
    }

    if (log) {
        *log << "  === 安全退出(第" << safety << "轮, 触发500轮上限) [异常局] "
             << "玩家HP:" << playerHp << ", BossHP:" << bossHp
             << ", 玩家得分:" << player.totalScore << ", Boss得分:" << boss.totalScore << " ===\n";
    }
    return {false, 0, true};
}

int main() {
    const std::vector<int> ALL_AI = { 1, 2 };
    const int N = (int)ALL_AI.size();
    const int CAMPAIGN_GAMES = 100;
    constexpr int kTableWidth = 12 + 12 * 9;
    const std::string kSeparator(kTableWidth, '-');
    const char* levelNames[] = {"关卡1-新手","关卡2-换牌虫","关卡3-急眼雀","关卡4-疯狗","关卡5-赖账鬼","关卡6-不识数","关卡7-黑老大","关卡8-千里眼","关卡9-赌场老板"};

    std::mt19937 rng(12345);

    auto makeAI = [&](int id) -> AIFn {
        return makeOldAI(id, 6);
    };

    std::cout << "=== 关卡通关率 (" << CAMPAIGN_GAMES << "局) ===\n";

    std::cout << "\n--- 关卡限制验证 ---\n";
    std::cout << "  牌堆: 三关统一标准54张, 双方各27张 ✓\n";
    std::cout << "  关卡1-Boss AI: 新手 (AI1规则，最小单张), 随机先手 ✓\n";
    std::cout << "  关卡2-Boss AI: 换牌虫 (ai2_liar, 可换牌), 随机先手 ✓\n";
    std::cout << "  关卡3-Boss AI: 急眼雀 (AI1规则), Boss始终先手 ✓\n";
    std::cout << "  关卡4-Boss AI: 疯狗 (出牌数×3伤害 + AI1规则), 随机先手 ✓\n";
    std::cout << "  关卡5-Boss AI: 赖账鬼 (禁用同点数压牌+同花色单张压+AI1规则), 随机先手 ✓\n";
    std::cout << "  关卡6-Boss AI: 不识数 (50%牌型相同即可压+AI1规则), 随机先手 ✓\n";
    std::cout << "  关卡7-Boss AI: 黑老大 (先手出完弃玩家最大牌+摸牌+AI1规则), 随机先手 ✓\n";
    std::cout << "  关卡8-Boss AI: 千里眼 (AI2贪心+对手手牌可见), 随机先手 ✓\n";
    std::cout << "  关卡9-Boss AI: 赌场老板 (所有伤害扣玩家+AI1规则), 随机先手 ✓\n";
    std::cout << "  关卡1/2 后续回合: 上一回合赢家先手 ✓\n";
    std::cout << "  通关判定: 玩家血量 > 0 (起始140, Boss伤害扣血)\n";
    std::cout << "--- 开始测试 ---\n\n";

    // ===== 通关率 + 平均通关血量 (单次遍历, 避免RNG不一致) =====
    struct CellStats { int wins = 0; int abnormal = 0; int zeroHp = 0; double sumHp = 0; double sumAllHp = 0; };
    CellStats allStats[3][9];  // [aiIdx][lv-1]

    for (int aiIdx = 0; aiIdx < N; ++aiIdx) {
        int aiId = ALL_AI[aiIdx];
        for (int lv = 1; lv <= 9; ++lv) {
            auto fnPlayer = makeAI(aiId);
            CellStats& s = allStats[aiIdx][lv - 1];
            for (int g = 0; g < CAMPAIGN_GAMES; ++g) {
                auto res = runCampaignGame(fnPlayer, lv, rng);
                    if (res.abnormal) { s.abnormal++; continue; }
                    s.sumAllHp += res.hpRemaining;
                    if (res.playerWon) { s.wins++; s.sumHp += res.hpRemaining; }
                    else if (res.hpRemaining == 0) { s.zeroHp++; }
            }
        }
    }

    // --- 通关率表格 ---
    std::cout << ">>> 通关率 (" << CAMPAIGN_GAMES << "局) <<<\n";
    std::cout << "            ";
    for (int lv = 1; lv <= 9; ++lv) std::cout << std::setw(13) << levelNames[lv - 1];
    std::cout << "\n" << kSeparator << "\n" << std::flush;


    for (int aiIdx = 0; aiIdx < N; ++aiIdx) {
        std::cout << std::setw(10) << aiName(ALL_AI[aiIdx]) << " |";
        for (int lv = 1; lv <= 9; ++lv) {
            const auto& s = allStats[aiIdx][lv - 1];
            int valid = CAMPAIGN_GAMES - s.abnormal;
            double wr = valid > 0 ? 100.0 * s.wins / valid : 0;
            std::cout << std::fixed << std::setprecision(0) << std::setw(12) << wr << "%" << std::flush;
        }
        std::cout << "\n";
    }

    // --- 平均通关血量表格 ---
    std::cout << "\n>>> 通关局平均剩余血量 (起始 140) <<<\n";
    std::cout << "            ";
    for (int lv = 1; lv <= 9; ++lv) std::cout << std::setw(13) << levelNames[lv - 1];
    std::cout << "\n" << kSeparator << "\n" << std::flush;

    for (int aiIdx = 0; aiIdx < N; ++aiIdx) {
        std::cout << std::setw(10) << aiName(ALL_AI[aiIdx]) << " |";
        for (int lv = 1; lv <= 9; ++lv) {
            const auto& s = allStats[aiIdx][lv - 1];
            double avg = (s.wins > 0) ? s.sumHp / s.wins : 0;
            std::cout << std::fixed << std::setprecision(0) << std::setw(12) << avg << std::flush;
        }
        std::cout << "\n";
    }

    // --- 所有局数HP平均 ---
    std::cout << "\n>>> 所有有效局数平均剩余HP (起始 140) <<<\n";
    std::cout << "            ";
    for (int lv = 1; lv <= 9; ++lv) std::cout << std::setw(13) << levelNames[lv - 1];
    std::cout << "\n" << kSeparator << "\n" << std::flush;

    for (int aiIdx = 0; aiIdx < N; ++aiIdx) {
        std::cout << std::setw(10) << aiName(ALL_AI[aiIdx]) << " |";
        for (int lv = 1; lv <= 9; ++lv) {
            const auto& s = allStats[aiIdx][lv - 1];
            int valid = CAMPAIGN_GAMES - s.abnormal;
            double avg = valid > 0 ? s.sumAllHp / valid : 0;
            std::cout << std::fixed << std::setprecision(0) << std::setw(12) << avg << std::flush;
        }
        std::cout << "\n";
    }

    // --- 玩家HP归0的局数 ---
    std::cout << "\n>>> 玩家HP归零局数 (" << CAMPAIGN_GAMES << "局中) <<<\n";
    std::cout << "            ";
    for (int lv = 1; lv <= 9; ++lv) std::cout << std::setw(13) << levelNames[lv - 1];
    std::cout << "\n" << kSeparator << "\n" << std::flush;

    for (int aiIdx = 0; aiIdx < N; ++aiIdx) {
        std::cout << std::setw(10) << aiName(ALL_AI[aiIdx]) << " |";
        for (int lv = 1; lv <= 9; ++lv) {
            const auto& s = allStats[aiIdx][lv - 1];
            std::cout << std::setw(12) << s.zeroHp << std::flush;
        }
        std::cout << "\n";
    }

    // --- 验证：单次遍历数据完整性 ---
    std::cout << "\n>>> 验证: 每格 wins+losses+abnormal = " << CAMPAIGN_GAMES << "\n";
    int totalAbnormal = 0;
    for (int aiIdx = 0; aiIdx < N; ++aiIdx) {
        for (int lv = 1; lv <= 9; ++lv) {
            const auto& s = allStats[aiIdx][lv - 1];
            int losses = CAMPAIGN_GAMES - s.wins - s.abnormal;
            if (s.abnormal > 0 || s.wins + losses + s.abnormal != CAMPAIGN_GAMES) {
                std::cout << "  " << aiName(ALL_AI[aiIdx]) << " " << levelNames[lv - 1]
                          << ": wins=" << s.wins << " losses=" << losses
                          << " abnormal=" << s.abnormal << "\n";
            }
            totalAbnormal += s.abnormal;
        }
    }
    if (totalAbnormal == 0) std::cout << "  全部正常(无异常局) ✓\n";

    std::cout << "\n";

    // ===== 详细日志：20局/组合 → campaign_log.txt =====
    const int LOG_GAMES = 20;
    rng.seed(12345);
    std::cout << ">>> 生成详细日志 (" << LOG_GAMES << "局×" << N << "AI×9关卡="
              << (LOG_GAMES * N * 9) << "局) → campaign_log.txt <<<\n" << std::flush;

    {
        std::ofstream logFile("campaign_log.txt", std::ios::trunc);
        if (!logFile) {
            std::cerr << "无法创建日志文件!\n";
            return 1;
        }
        // UTF-8 BOM for Notepad compatibility
        logFile << "\xEF\xBB\xBF";

        logFile << "七鬼523 关卡通关详细日志\n";
        logFile << "==========================\n";
        logFile << "AI: 搜索/贪心/保守 (深度6)\n";
        logFile << "关卡: 1-新手 2-换牌虫 3-急眼雀 4-疯狗 5-赖账鬼 6-不识数 7-黑老大 8-千里眼 9-赌场老板\n";
        logFile << "玩家初始HP: 140, Boss初始HP: 110\n";
        logFile << "每局 " << LOG_GAMES << "局, 共 " << (LOG_GAMES * N * 9) << " 局\n\n";

        int totalWins = 0, totalGames = 0, totalAbnormal = 0;
        for (int aiIdx = 0; aiIdx < N; ++aiIdx) {
            int aiId = ALL_AI[aiIdx];
            std::string label = aiName(aiId);
            logFile << "\n##################################################\n";
            logFile << "# AI: " << label;
            if (aiId == 3) logFile << " (Minimax搜索)";
            else if (aiId == 102) logFile << " (贪心评估)";
            else logFile << " (保守评估)";
            logFile << "\n##################################################\n\n";

            for (int lv = 1; lv <= 9; ++lv) {
                auto fnPlayer = makeAI(aiId);
                int wins = 0, abnormal = 0;
                double sumHp = 0;

                logFile << "--- " << levelTitle(lv) << " ---\n\n";
                for (int g = 0; g < LOG_GAMES; ++g) {
                    auto res = runCampaignGame(fnPlayer, lv, rng, g, label, &logFile);
                    if (res.abnormal) {
                        abnormal++;
                        logFile << "──────────────────────────────────\n";
                        logFile << " 结果: ⚠ 异常 (500轮安全退出, 不计入统计)\n\n" << std::flush;
                        continue;
                    }
                    if (res.playerWon) { wins++; sumHp += res.hpRemaining; }
                    const int startHp = 140;
                    int hpLost = startHp - res.hpRemaining;
                    logFile << "──────────────────────────────────\n";
                    logFile << " 结果: " << (res.playerWon ? "✓ 通关" : "✗ 失败")
                            << " | 剩余HP: " << res.hpRemaining;
                    if (res.playerWon) {
                        logFile << " (掉了 " << hpLost << " 血)";
                        if (hpLost == 0)
                            logFile << " — 完美无伤!";
                        else if (hpLost <= 20)
                            logFile << " — 轻松碾压";
                        else if (hpLost <= 60)
                            logFile << " — 中等难度";
                        else
                            logFile << " — 惊险残血";
                    } else {
                        logFile << " (HP归零或BossHP未打空)";
                        if (lv == 4) logFile << " — 疯狗撕咬太疼了";
                        else if (lv == 5) logFile << " — 赖账鬼封死了你的出牌";
                        else if (lv == 6) logFile << " — 不识数耍赖太频繁";
                        else logFile << " — Boss的规则压制太强了";
                    }
                    logFile << "\n\n" << std::flush;
                }
                totalWins += wins;
                totalGames += (LOG_GAMES - abnormal);
                totalAbnormal += abnormal;

                int valid = LOG_GAMES - abnormal;
                double wr = valid > 0 ? 100.0 * wins / valid : 0;
                double avgHp = wins > 0 ? sumHp / wins : 0;
                logFile << "--- " << levelTitle(lv) << " 统计: 通关率 "
                        << std::fixed << std::setprecision(0) << wr << "% ("
                        << wins << "/" << valid
                        << (abnormal > 0 ? ", 异常" + std::to_string(abnormal) + "局" : "")
                        << "), 平均剩余HP: " << avgHp << " ---\n\n";
            }
        }
        logFile << "\n=== 总计: " << totalWins << "/" << totalGames
                << " (" << std::fixed << std::setprecision(1)
                << (totalGames > 0 ? 100.0 * totalWins / totalGames : 0) << "%)"
                << (totalAbnormal > 0 ? " [异常" + std::to_string(totalAbnormal) + "局不计入]" : "")
                << " ===\n";
    }

    std::cout << "日志生成完毕!\n\n";
    return 0;
}