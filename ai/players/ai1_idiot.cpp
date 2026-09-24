#include "ai1_idiot.h"
#include <algorithm>
#include <random>
#include <map>

// ─── 辅助函数 ────────────────────────────────────────────────────────────────

// 从手牌中收集所有 point == p 的牌
static std::vector<Card> cardsOfPoint(const std::vector<Card>& hand,
                                       const std::string& p)
{
    std::vector<Card> out;
    for (const Card& c : hand)
        if (c.point == p) out.push_back(c);
    return out;
}

// 检查 combo 是否为炸弹（4 张同点）
static bool localIsBomb(const std::vector<Card>& combo)
{
    return combo.size() == 4;
}

// 检查 combo 是否为火箭（双鬼）
static bool localIsRocket(const std::vector<Card>& combo)
{
    if (combo.size() != 2) return false;
    return (combo[0].point == "大鬼" || combo[0].point == "小鬼") &&
           (combo[1].point == "大鬼" || combo[1].point == "小鬼");
}

// 从 hand 中扣除 subset 中的牌，返回剩余
static std::vector<Card> remainingCards(const std::vector<Card>& hand,
                                         const std::vector<Card>& subset)
{
    std::vector<Card> rem = hand;
    for (const Card& s : subset) {
        auto it = std::find_if(rem.begin(), rem.end(),
            [&](const Card& r) { return r.point == s.point && r.suit == s.suit; });
        if (it != rem.end()) rem.erase(it);
    }
    return rem;
}

// 从剩余手牌中找最小单张（按 RANK_MAP）
static bool findSmallestSingle(const std::vector<Card>& pool, Card& out)
{
    if (pool.empty()) return false;
    auto it = std::min_element(pool.begin(), pool.end(),
        [](const Card& a, const Card& b) {
            return RANK_MAP.at(a.point) < RANK_MAP.at(b.point);
        });
    out = *it;
    return true;
}

// 从剩余手牌中找最小对子（按 RANK_MAP）
static bool findSmallestPair(const std::vector<Card>& pool,
                              std::vector<Card>& out)
{
    // 按点数统计
    std::map<std::string, int> cnt;
    for (const Card& c : pool) cnt[c.point]++;

    // 按 RANK_MAP 升序遍历点数
    std::vector<std::string> pts;
    for (const auto& kv : cnt)
        if (kv.second >= 2) pts.push_back(kv.first);
    if (pts.empty()) return false;

    std::sort(pts.begin(), pts.end(),
        [](const std::string& a, const std::string& b) {
            return RANK_MAP.at(a) < RANK_MAP.at(b);
        });

    const std::string& p = pts[0]; // 最小的
    out.clear();
    for (const Card& c : pool)
        if (c.point == p && out.size() < 2) out.push_back(c);
    return out.size() == 2;
}

// 尝试分解 combo 成 lastPlay 所需子集
// 成功时返回分解后的牌；失败返回空 vector
static std::vector<Card> tryDecompose(const std::vector<Card>& combo,
                                       const std::vector<Card>& hand,
                                       CardType targetType,
                                       const CardTypeResult& lastPlay)
{
    std::vector<Card> result;
    std::vector<Card> rem = remainingCards(hand, combo);

    switch (targetType) {
    case CardType::Single: {
        // 从 combo 里拿 1 张
        result.push_back(combo[0]);
        break;
    }
    case CardType::Pair: {
        if (combo.size() < 2) return {};
        result = { combo[0], combo[1] };
        break;
    }
    case CardType::Triple: {
        if (combo.size() < 3) return {};
        result = { combo[0], combo[1], combo[2] };
        break;
    }
    case CardType::TripleWithOne: {
        if (combo.size() < 3) return {};
        std::vector<Card> triple = { combo[0], combo[1], combo[2] };
        Card single;
        if (!findSmallestSingle(rem, single)) return {};
        result = triple;
        result.push_back(single);
        break;
    }
    case CardType::TripleWithTwo: {
        if (combo.size() < 3) return {};
        std::vector<Card> triple = { combo[0], combo[1], combo[2] };
        std::vector<Card> pair;
        if (!findSmallestPair(rem, pair)) return {};
        result = triple;
        result.insert(result.end(), pair.begin(), pair.end());
        break;
    }
    default:
        return {};
    }

    // 验证合法性
    CardTypeResult parsed = parseCardType(result);
    if (parsed.type == CardType::Invalid) return {};
    if (!canBeat(parsed, lastPlay)) return {};
    return result;
}

// ─── AI1 主入口 ──────────────────────────────────────────────────────────────

std::vector<Card> ai1_idiot_choose(
    const Player& me,
    const Player& /*opp*/,
    const CardTypeResult& lastPlay,
    const Deck& /*myDeck*/,
    int /*tableScore*/
)
{
    // ── Step 1：Special523 ──
    {
        bool has7 = false, has5 = false, has2 = false, has3 = false, hasGhost = false;
        for (const Card& c : me.hand) {
            if      (c.point == "7")   has7 = true;
            else if (c.point == "5")   has5 = true;
            else if (c.point == "2")   has2 = true;
            else if (c.point == "3")   has3 = true;
            else if (c.point == "大鬼" || c.point == "小鬼") hasGhost = true;
        }
        if (has7 && has5 && has2 && has3 && hasGhost)
            return me.hand; // 直接全部出
    }

    // ── Step 2：非炸弹/火箭点数，弱→强遍历 ──
    // 收集所有不同点数，按 RANK_MAP 升序
    std::vector<std::string> points;
    for (const Card& c : me.hand)
        if (std::find(points.begin(), points.end(), c.point) == points.end())
            points.push_back(c.point);
    std::sort(points.begin(), points.end(),
        [](const std::string& a, const std::string& b) {
            return RANK_MAP.at(a) < RANK_MAP.at(b);
        });

    static std::mt19937 rng(std::random_device{}());
    static std::uniform_int_distribution<int> coin(0, 1);

    for (const std::string& p : points) {
        std::vector<Card> combo = cardsOfPoint(me.hand, p);
        if (combo.empty()) continue;

        // 跳过炸弹/火箭
        if (localIsBomb(combo) || localIsRocket(combo)) continue;

        CardTypeResult cr = parseCardType(combo);
        if (cr.type == CardType::Invalid) continue;

        bool isFirst = (lastPlay.type == CardType::Invalid);

        if (isFirst) {
            // 先手：直接出
            return combo;
        }

        // ── 后手 ──
        // a) 牌型匹配且能压过
        if (cr.type == lastPlay.type && canBeat(cr, lastPlay)) {
            return combo;
        }

        // b) 牌型不匹配，尝试拆牌
        std::vector<Card> decomposed = tryDecompose(combo, me.hand, lastPlay.type, lastPlay);
        if (!decomposed.empty()) {
            // 50% 概率返回，50% 跳过
            if (coin(rng) == 0) {
                return decomposed;
            }
            // else 继续下一个点数
        }
    }

    // ── Step 3：炸弹/火箭 ──
    for (const std::string& p : points) {
        std::vector<Card> combo = cardsOfPoint(me.hand, p);
        if (combo.empty()) continue;
        if (!localIsBomb(combo) && !localIsRocket(combo)) continue;

        // 先手：直接出
        if (lastPlay.type == CardType::Invalid)
            return combo;

        // 后手：能压过才出
        CardTypeResult cr = parseCardType(combo);
        if (cr.type != CardType::Invalid && canBeat(cr, lastPlay))
            return combo;
    }

    // ── Step 4：pass ──
    return {};
}