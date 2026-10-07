#include "game/campaign.h"
#include <algorithm>

static const std::vector<std::string> g_removeTables[] = {
    {},
    {"4"},
    {"4", "4"},
    {"4", "4", "6"},
    {"4", "4", "6", "6"},
    {"4", "4", "4", "6", "6"},
    {"4", "4", "4", "6", "6", "8"},
    {"4", "4", "4", "6", "6", "8", "8"},
    {"4", "4", "4", "4", "6", "6", "8", "8"},
    {"4", "4", "4", "4", "6", "6", "6", "8", "8", "8"},
};

const std::vector<std::string>& getLevelRemoveTable(int level) {
    if (level < 1 || level > 9) {
        static const std::vector<std::string> empty;
        return empty;
    }
    return g_removeTables[level];
}

// ── 关卡筹码配置 ──
// 入场筹码 = 基础递增
// 奖励筹码 ≈ 入场 × 2.5（胜利获得，失败则损失入场）
static const int g_entryChips[] = {
    0,    // 0 占位
    1,    // 1 傻子/新手
    5,    // 2 换牌虫
    10,   // 3 急眼雀
    20,   // 4 疯狗
    50,   // 5 赖账鬼
    100,  // 6 不识数
    200,  // 7 (预留)
    400,  // 8 (预留)
    800,  // 9 (最终关)
};

static const int g_rewardChips[] = {
    0,    // 0 占位
    3,    // 1
    12,   // 2
    25,   // 3
    50,   // 4
    120,  // 5
    250,  // 6
    500,  // 7
    1000, // 8
    2000, // 9
};

int getLevelEntryChips(int level) {
    if (level < 1 || level > 9) return 0;
    return g_entryChips[level];
}

int getLevelRewardChips(int level) {
    if (level < 1 || level > 9) return 0;
    return g_rewardChips[level];
}

Campaign::Campaign() {
    startNew();
}

Campaign::Campaign(const CampaignConfig& cfg) : cfg_(cfg) {
    startNew();
}

void Campaign::startNew() {
    playerHp_ = cfg_.playerInitHp;
    bossHp_ = cfg_.bossMaxHp;
    currentLevel_ = 1;
    playerChips_ = 100;
}

void Campaign::startLevel(int level) {
    currentLevel_ = level;
    bossHp_ = cfg_.bossMaxHp;
}

void Campaign::applyRoundDamage(int playerScore, int bossScore) {
    bossHp_ -= playerScore;
    playerHp_ -= bossScore;
}

LevelResult Campaign::finishLevel(const RoundResult& rr,
                                  const Player& player,
                                  const Player& boss,
                                  int playerDeckRemain,
                                  int bossDeckRemain) {
    LevelResult lr;
    lr.playerScore = player.totalScore;
    lr.bossScore = boss.totalScore;
    lr.playerRemainCards = static_cast<int>(player.hand.size()) + playerDeckRemain;
    lr.bossRemainCards = static_cast<int>(boss.hand.size()) + bossDeckRemain;
    lr.chipsDelta = 0;
    lr.chipsAfter = playerChips_;

    int prevPlayerHp = playerHp_;
    int prevBossHp = bossHp_;

    if (rr.specialVictory) {
        lr.playerWon = true;
        lr.bossCleared = true;
    } else if (bossHp_ <= 0) {
        lr.playerWon = true;
        lr.bossCleared = true;
    } else {
        lr.playerWon = (player.totalScore >= boss.totalScore);
        lr.bossCleared = false;
    }

    int totalRemain = lr.playerRemainCards + lr.bossRemainCards;
    int healAmount = totalRemain * cfg_.healPerCard;

    if (lr.playerWon) {
        playerHp_ += healAmount;
        // 胜利：获得奖励筹码
        int reward = getLevelRewardChips(currentLevel_);
        playerChips_ += reward;
        lr.chipsDelta += reward;
    } else {
        playerHp_ -= healAmount;
        // 失败：扣除入场筹码（已在开局扣除，这里不重复扣）
    }
    lr.chipsAfter = playerChips_;

    playerHp_ = std::min(playerHp_, cfg_.playerMaxHp);

    lr.playerHpDelta = playerHp_ - prevPlayerHp;
    lr.bossHpDelta = bossHp_ - prevBossHp;

    return lr;
}

bool Campaign::isGameOver() const {
    return playerHp_ <= 0;
}

bool Campaign::isVictory() const {
    return currentLevel_ >= cfg_.totalLevels && bossHp_ <= 0;
}