#pragma once
#include <functional>
#include <random>
#include <vector>
#include "core/player.h"
#include "core/card/cardtype.h"

struct LevelRule {
    const char* levelName = nullptr;

    // Boss AI pass 时尝试强制出牌（内部含 RNG）。返回空表示无强制。
    std::function<std::vector<Card>(const Player& boss, const CardTypeResult& lastPlay,
                                     std::mt19937& rng)> tryForcePlay;

    // canBeat 失败时，Boss 是否有特权放行（内部含 RNG）
    std::function<bool(const CardTypeResult& candidate, const CardTypeResult& lastPlay,
                        std::mt19937& rng)> canBeatOverride;

    // 玩家出牌限制：返回 true 则阻止玩家出牌
    std::function<bool(const CardTypeResult& playerPlay, const CardTypeResult& lastPlay)> blockPlayer;

    // Boss 出牌后对玩家的额外伤害（返回伤害值）
    std::function<int(const std::vector<Card>& bossPlay)> bossDamage;

    // 黑老大：Boss出完手牌时触发，强制弃玩家最大牌
    // 参数: player手牌, player牌堆, rng. 返回被弃的牌（空=无效果）
    std::function<std::vector<Card>(std::vector<Card>& playerHand,
                                     std::vector<Card>& playerDeck,
                                     std::mt19937& rng)> onBossFinish;

    // UI 提示文本
    const char* forcePlayMsg  = nullptr;
    const char* canBeatOverrideMsg = nullptr;
    const char* blockPlayerMsg = nullptr;
    const char* bossDamageMsg = nullptr;
    const char* onBossFinishMsg = nullptr;
};

// 工厂函数：level=0 返回空规则，level=1-6 返回对应规则
LevelRule createLevelRule(int level);