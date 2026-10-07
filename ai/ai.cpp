#include "ai_types.h"
#include "core/card/rank.h"
#include "ai.h"
#include "engine/play_selector.h"
#include "plugins/counter_risk.h"
#include "plugins/minimax.h"
#include "core/card/cardtype.h"
#include "core/player.h"
#include <algorithm>
#include <sstream>
#include <iostream>
#include <random>
#include <map>
#include <set>
#include <climits>



std::vector<std::vector<Card>> enumerateLegalPlays(const Player& player) {
    std::vector<std::vector<Card>> result;
    size_t n = player.hand.size();
    for (size_t mask = 1; mask < (1ULL << n); ++mask) {
        std::vector<Card> subset;
        for (size_t i = 0; i < n; ++i) {
            if (mask & (1ULL << i)) subset.push_back(player.hand[i]);
        }
        if (parseCardType(subset).type != CardType::Invalid ||
            parseFiveOfAKind(subset).type != CardType::Invalid) {
            result.push_back(subset);
        }
    }
    return result;
}

// ═══════════════════════════════════════════════════════════════════════════════
// 两大基础AI: 纯手写逻辑，不依赖AIEngine插件体系
//
// 基础AI1: 规则 — 纯规则驱动，不依赖对手/桌面分/tracker
//   首出: 最小点的天然组合（对子/单张），三条及以上拆最小单张
//   跟牌: 同牌型最小 > 最小炸弹 > 最小五条 > 王炸
//
// 基础AI2: 贪心 — playGain单步评分最大化
//   评分 = 桌面分×2 + 出牌数×10 + 清空奖1000 - 大牌惩罚
//         - 拆对(-17) - 炸弹保留(-92) - 王炸保留(-459)
//         + 残局炸弹(+236) + 残局火箭(+223) + tracker安全(+30) - 反制风险
// ═══════════════════════════════════════════════════════════════════════════════

bool   g_ai1_zeroScorePass = true;

std::vector<Card> aiChoosePlayAI1(const Player& player, const CardTypeResult& prev) {
    static CardTracker dummyTracker;
    static Player dummyOpponent;
    static Deck dummyDeck;
    return aiChoosePlayAI1(player, dummyOpponent, prev, dummyDeck, 0, dummyTracker);
}

std::vector<Card> aiChoosePlayAI1(const Player& player,
                                  const Player& opponent,
                                  const CardTypeResult& previous,
                                  const Deck& deck,
                                  int tableScore,
                                  const CardTracker& tracker) {
    auto allPlays = enumerateLegalPlays(player);
    if (allPlays.empty()) return {};

    if (previous.cards.empty()) {
        if (player.hand.empty()) return {};
        // 按大小排序
        std::vector<Card> sorted = player.hand;
        std::sort(sorted.begin(), sorted.end(), [](const Card& a, const Card& b) {
            return getCardRank(a.point) < getCardRank(b.point);
        });
        // 收集不同点数，按 rank 升序
        std::vector<std::string> points;
        for (const Card& c : sorted)
            if (std::find(points.begin(), points.end(), c.point) == points.end())
                points.push_back(c.point);
        // 首出：出最小点的天然组合（对子/单张直接出，三条及以上拆最小单张）
        const std::string& p = points.front();
        std::vector<Card> combo;
        for (const Card& c : player.hand)
            if (c.point == p) combo.push_back(c);
        return combo.size() >= 3 ? std::vector<Card>{combo[0]} : combo;
    } else {
        std::vector<std::vector<Card>> sameTypePlays, bombPlays, fivePlays, rocketPlays;
        for (const auto& play : allPlays) {
            auto parsed = parseAnyCardType(play);
            if (!canBeat(parsed, previous)) continue;
            if (parsed.type == CardType::FiveOfAKind) fivePlays.push_back(play);
            else if (parsed.type == CardType::Bomb) bombPlays.push_back(play);
            else if (parsed.type == CardType::Rocket) rocketPlays.push_back(play);
            else sameTypePlays.push_back(play);
        }
        if (!sameTypePlays.empty()) {
            auto best = sameTypePlays[0];
            auto bestParsed = parseCardType(best);
            for (const auto& play : sameTypePlays) {
                auto parsed = parseCardType(play);
                if (getCardRank(parsed.keyPoint) < getCardRank(bestParsed.keyPoint)) {
                    best = play; bestParsed = parsed;
                }
            }
            return best;
        }
        if (g_ai1_zeroScorePass && tableScore == 0 && !bombPlays.empty())
            return {};
        if (!bombPlays.empty()) {
            auto best = bombPlays[0];
            int bestRank = getCardRank(parseCardType(best).keyPoint);
            for (const auto& play : bombPlays) {
                int rank = getCardRank(parseCardType(play).keyPoint);
                if (rank < bestRank) { bestRank = rank; best = play; }
            }
            return best;
        }
        if (!fivePlays.empty()) {
            auto best = fivePlays[0];
            int bestRank = getCardRank(parseCardType(best).keyPoint);
            for (const auto& play : fivePlays) {
                int rank = getCardRank(parseCardType(play).keyPoint);
                if (rank < bestRank) { bestRank = rank; best = play; }
            }
            return best;
        }
        if (!rocketPlays.empty()) return rocketPlays[0];
    }
    return {};
}

bool breaksCombo(const std::vector<Card>& play, const Player& player) {
    std::map<std::string, int> handCount;
    for (const Card& c : player.hand) handCount[c.point]++;

    std::map<std::string, int> playCount;
    for (const Card& c : play) playCount[c.point]++;

    for (const auto& kv : playCount) {
        int inHand = handCount[kv.first];
        int inPlay = kv.second;
        if (inHand >= 2 && inPlay == 1) return true;
        if (inHand >= 3 && inPlay == 2) return true;
        if (inHand >= 4 && inPlay == 3) return true;
    }
    return false;
}

static int playGain(const std::vector<Card>& play,
                    const Player& player,
                    const Player& opponent,
                    const Deck& deck,
                    const CardTypeResult& previous,
                    int tableScore,
                    const CardTracker* tracker) {
    /* 修改：复用 evaluatePlayWithBreakdown 消除重复代码
       原 playGain 与 evaluatePlayWithBreakdown 核心评分80%相同
       现在 playGain 仅为 evaluatePlayWithBreakdown 的便捷包装 */
    DecisionBreakdown bd = evaluatePlayWithBreakdown(play, player, opponent,
                                                      deck, previous, tableScore, tracker);
    return bd.total;
}

DecisionBreakdown evaluatePlayWithBreakdown(const std::vector<Card>& play,
                              const Player& player,
                              const Player& opponent,
                              const Deck& deck,
                              const CardTypeResult& previous,
                              int tableScore,
                              const CardTracker* tracker) {
    /* 修改：原函数签名中 DecisionBreakdown* out 改为返回 DecisionBreakdown 
       符合规范3.4：多返回值优先返回结构体，不应用裸指针输出参数 */
    DecisionBreakdown bd;
    bd.tableScore = tableScore * 2;
    bd.base = (int)play.size() * 10;
    if (play.size() == player.hand.size()) bd.base += 1000;

    int rankPenalty = 0;
    for (const Card& c : play) {
        int rank = getCardRank(c.point);
        if (rank >= 13) rankPenalty -= rank * 3;
        else if (rank >= 10) rankPenalty -= rank * 2;
        else rankPenalty -= rank;
    }
    bd.endgame = rankPenalty;

    for (const Card& c : play) {
        if (c.score > 0 && tableScore > 0) bd.scoreCard += c.score * 2;
    }

    bd.comboBreak = breaksCombo(play, player) ? -17 : 0;

    auto parsed = parseCardType(play);
    if (parsed.type == CardType::Bomb) bd.special -= 92;
    else if (parsed.type == CardType::Rocket) bd.special -= 459;

    if ((int)opponent.hand.size() <= 2) {
        if (parsed.type == CardType::Bomb) bd.special += 236;
        else if (parsed.type == CardType::Rocket) bd.special += 223;
    }

    if (tracker) {
        for (const Card& c : play) {
            if (tracker->isExhausted(c.point)) bd.tracker += 30;
        }
        int risk = estimateCounterRisk(play, player, opponent, tracker, parsed, tableScore, deck);
        bd.defensive = -risk * kCounterRiskWeight;
    } else {
        bd.defensive = 0;
    }

    bd.total = bd.base + bd.scoreCard + bd.special + bd.endgame
             + bd.comboBreak + bd.tableScore + bd.defensive + bd.tracker;
    return bd;
}

// ── AI2 实现: 贪心评分 + 终局/拦截检测 ────────────────────────────────────
std::vector<Card> aiChoosePlayAI2(const Player& player,
                                  const Player& opponent,
                                  const CardTypeResult& previous,
                                  const Deck& deck,
                                  int tableScore) {
    auto allPlays = enumerateLegalPlays(player);
    if (allPlays.empty()) return {};

    auto finish = tryFinishPlay(player, allPlays, previous);
    if (!finish.empty()) return finish;

    auto intercept = tryEndgameIntercept(player, allPlays, previous, (int)opponent.hand.size());
    if (!intercept.empty()) return intercept;

    std::vector<Card> best;
    int bestGain = INT_MIN;
    for (const auto& play : allPlays) {
        auto parsed = parseAnyCardType(play);
        if (!previous.cards.empty() && !canBeat(parsed, previous)) continue;
        int gain = playGain(play, player, opponent, deck, previous, tableScore, nullptr);
        if (gain > bestGain) { bestGain = gain; best = play; }
    }
    return best;
}

// ═══════════════════════════════════════════════════════════════════════════════
// 插件AI: 搜索 — 依赖 minimax/采样搜索插件（博弈树，深度8，看穿对手手牌）
// ═══════════════════════════════════════════════════════════════════════════════
std::vector<Card> aiChoosePlayAI3(const Player& player,
                                  const Player& opponent,
                                  const CardTypeResult& previous,
                                  const Deck& deck,
                                  int tableScore,
                                  const CardTracker& tracker) {
    return searchBestPlayCheat(player, opponent, previous, deck, tableScore, 8);
}

// ── AI3变体: 搜索（可调深度，默认6）────────────────────────────────────
std::vector<Card> aiChoosePlayAI4(const Player& player,
                                  const Player& opponent,
                                  const CardTypeResult& previous,
                                  const Deck& deck,
                                  int tableScore,
                                  const CardTracker& tracker) {
    return searchBestPlayCheat(player, opponent, previous, deck, tableScore, g_searchBestPlayCheatDepth);
}

int g_searchBestPlayCheatDepth = 6;

// ── AI调度入口（根据 player.aiLevel 选择）────────────────────────────────────
std::vector<Card> aiChoosePlay(const Player& player,
                               const Player& opponent,
                               const CardTypeResult& previous,
                               const Deck& deck,
                               int tableScore,
                               const CardTracker& tracker) {
    std::vector<Card> play;
    switch (player.aiLevel) {
        case AILevel::AI1_Rule:
            play = aiChoosePlayAI1(player, previous);
            break;
        case AILevel::AI2_Greedy:
            play = aiChoosePlayAI2(player, opponent, previous, deck, tableScore);
            break;
        case AILevel::AI3_Search:
            play = aiChoosePlayAI3(player, opponent, previous, deck, tableScore, tracker);
            break;
        case AILevel::AI4_Search:
            play = aiChoosePlayAI4(player, opponent, previous, deck, tableScore, tracker);
            break;
        default:
            play = aiChoosePlayAI1(player, previous);
            break;
    }
    if (!play.empty() && !previous.cards.empty()) {
        auto parsed = parseAnyCardType(play);
        if (!canBeat(parsed, previous)) return {};
    }
    return play;
}

std::vector<Card> humanChoosePlay(const Player& player, const CardTypeResult& previous) {
    std::cout << std::endl;
    std::cout << "当前出牌者: " << player.name << "（你）" << std::endl;
    std::cout << "你的手牌:" << std::endl;
    for (size_t i = 0; i < player.hand.size(); ++i) {
        std::cout << "  " << (i + 1) << ": ";
        printCard(player.hand[i]);
        if (player.hand[i].score > 0) std::cout << " (" << player.hand[i].score << "分)";
        std::cout << std::endl;
    }
    if (!previous.cards.empty()) {
        std::cout << "上一手出牌: ";
        printCardTypeResult(previous);
        std::cout << std::endl;
    } else {
        std::cout << "这一手由你开始出牌" << std::endl;
    }
    std::cout << "输入出牌序号（空格分隔，例如: 1 3 5，输入 0 表示不要）: ";
    std::string line;
    std::getline(std::cin, line);

    if (line.empty() || line == "0") return {};

    std::istringstream iss(line);
    std::vector<Card> chosen;
    int idx;
    while (iss >> idx) {
        if (idx >= 1 && (size_t)idx <= player.hand.size()) {
            chosen.push_back(player.hand[idx - 1]);
        }
    }
    if (chosen.empty()) return {};

    CardTypeResult parsed = parseCardType(chosen);
    if (parsed.type == CardType::Invalid) {
        std::cout << "无效牌型" << std::endl;
        return {};
    }
    if (!previous.cards.empty() && !canBeat(parsed, previous)) {
        std::cout << "无法打过上一手" << std::endl;
        return {};
    }
    return chosen;
}