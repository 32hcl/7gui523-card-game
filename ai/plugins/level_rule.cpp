#include "level_rule.h"
#include "ai/ai.h"
#include "core/card/rank.h"
#include <algorithm>

LevelRule createLevelRule(int level) {
    LevelRule r;

    switch (level) {
    case 1:
        r.levelName = "新手";
        break;

    case 2:
        r.levelName = "换牌虫";
        break;

    case 3:
        r.levelName = "急眼雀";
        break;

    case 4:
        r.levelName = "疯狗";
        r.bossDamage = [](const std::vector<Card>& bossPlay) -> int {
            return (int)bossPlay.size() * 3;
        };
        r.bossDamageMsg = "疯狗撕咬！每张牌咬掉 3 点血（%2 张 × 3 = %1 点伤害）";
        break;

    case 5:
        r.levelName = "赖账鬼";
        r.tryForcePlay = [](const Player& boss, const CardTypeResult& lastPlay,
                             std::mt19937& /*rng*/) -> std::vector<Card> {
            if (lastPlay.type != CardType::Single || lastPlay.cards.empty())
                return {};
            if (getCardRank(lastPlay.cards[0].point) >= getCardRank("J"))
                return {};
            auto it = std::find_if(boss.hand.begin(), boss.hand.end(),
                [&](const Card& c) { return c.suit == lastPlay.cards[0].suit; });
            if (it != boss.hand.end())
                return {*it};
            return {};
        };
        r.canBeatOverride = [](const CardTypeResult& candidate, const CardTypeResult& lastPlay,
                                std::mt19937& /*rng*/) -> bool {
            return lastPlay.type == CardType::Single && candidate.type == CardType::Single &&
                   !lastPlay.cards.empty() && !candidate.cards.empty() &&
                   getCardRank(lastPlay.cards[0].point) < getCardRank("J") &&
                   lastPlay.cards[0].suit == candidate.cards[0].suit;
        };
        r.blockPlayer = [](const CardTypeResult& playerPlay, const CardTypeResult& lastPlay) -> bool {
            return playerPlay.keyPoint == lastPlay.keyPoint;
        };
        r.forcePlayMsg  = "赖账鬼耍赖！用同花色牌强行压过（仅对 J 以下的单张有效）";
        r.canBeatOverrideMsg = "赖账鬼用同花色牌压过（无视点数，仅对J以下单张）";
        r.blockPlayerMsg = "赖账鬼禁止你出相同点数的牌！试试换一种牌型或更大点数";
        break;

    case 6:
        r.levelName = "不识数";
        r.tryForcePlay = [](const Player& boss, const CardTypeResult& lastPlay,
                             std::mt19937& rng) -> std::vector<Card> {
            if (rng() % 2 != 0) return {};
            auto allPlays = enumerateLegalPlays(boss);
            std::vector<std::vector<Card>> sameType;
            for (const auto& play : allPlays) {
                CardTypeResult parsed = parseCardType(play);
                if (parsed.type == CardType::Invalid)
                    parsed = parseFiveOfAKind(play);
                if (parsed.type == lastPlay.type)
                    sameType.push_back(play);
            }
            if (sameType.empty()) return {};
            std::sort(sameType.begin(), sameType.end(),
                [](const std::vector<Card>& a, const std::vector<Card>& b) {
                    return getCardRank(parseCardType(a).keyPoint) <
                           getCardRank(parseCardType(b).keyPoint);
                });
            return sameType[0];
        };
        r.canBeatOverride = [](const CardTypeResult& candidate, const CardTypeResult& lastPlay,
                                std::mt19937& rng) -> bool {
            return candidate.type == lastPlay.type && (rng() % 2 == 0);
        };
        r.forcePlayMsg = "不识数蒙混过关！Pass前50%概率强行出同牌型最小牌";
        r.canBeatOverrideMsg = "不识数蒙混过关！50%概率牌型相同即可压（无视大小）";
        break;

    case 7:
        r.levelName = "黑老大";
        r.onBossFinish = [](std::vector<Card>& playerHand,
                             std::vector<Card>& playerDeck,
                             std::mt19937& rng) -> std::vector<Card> {
            if (playerHand.empty()) return {};
            int maxRank = -1;
            for (const auto& c : playerHand) {
                int rk = getCardRank(c.point);
                if (rk > maxRank) maxRank = rk;
            }
            std::vector<Card*> maxCards;
            for (auto& c : playerHand) {
                if (getCardRank(c.point) == maxRank)
                    maxCards.push_back(&c);
            }
            Card* picked = maxCards[rng() % maxCards.size()];
            std::vector<Card> result = {*picked};
            auto it = std::find_if(playerHand.begin(), playerHand.end(),
                [&](const Card& h) { return h.point == picked->point && h.suit == picked->suit; });
            if (it != playerHand.end()) playerHand.erase(it);
            if (!playerDeck.empty()) {
                playerHand.push_back(playerDeck.back());
                playerDeck.pop_back();
            }
            return result;
        };
        r.onBossFinishMsg = "黑老大先手出完！强制弃你最大的牌(%1)，从牌堆摸一张";
        break;

    case 8:
        r.levelName = "千里眼";
        break;

    case 9:
        r.levelName = "赌场老板";
        break;

    default:
        break;
    }

    return r;
}