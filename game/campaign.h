#pragma once

#include <string>
#include <vector>
#include "game/game.h"

struct CampaignConfig {
    int playerInitHp = 140;
    int playerMaxHp  = 210;
    int bossMaxHp    = 110;
    int totalLevels  = 9;
    int healPerCard  = 3;
};

struct LevelResult {
    bool playerWon;
    int  playerScore;
    int  bossScore;
    int  playerRemainCards;
    int  bossRemainCards;
    int  playerHpDelta;
    int  bossHpDelta;
    bool bossCleared;
    int  chipsDelta;   // 筹码变化：正=获得，负=损失
    int  chipsAfter;   // 结算后筹码余额
};

const std::vector<std::string>& getLevelRemoveTable(int level);

/**
 * @brief 获取第 level 关的入场筹码
 */
int getLevelEntryChips(int level);

/**
 * @brief 获取第 level 关的胜利奖励筹码
 */
int getLevelRewardChips(int level);

class Campaign {
public:
    Campaign();
    explicit Campaign(const CampaignConfig& cfg);

    void startNew();
    void startLevel(int level);
    void applyRoundDamage(int playerScore, int bossScore);
    LevelResult finishLevel(const RoundResult& rr,
                            const Player& player,
                            const Player& boss,
                            int playerDeckRemain,
                            int bossDeckRemain);
    bool isGameOver() const;
    bool isVictory() const;

    int playerHp() const { return playerHp_; }
    int bossHp() const   { return bossHp_; }
    int currentLevel() const { return currentLevel_; }
    const CampaignConfig& config() const { return cfg_; }

    int  playerChips() const { return playerChips_; }
    void addChips(int amount) { playerChips_ += amount; }
    bool spendChips(int amount) {
        if (playerChips_ < amount) return false;
        playerChips_ -= amount;
        return true;
    }
    void healPlayer(int amount) {
        playerHp_ = std::min(playerHp_ + amount, cfg_.playerMaxHp);
    }

private:
    int playerHp_;
    int bossHp_;
    int currentLevel_;
    int playerChips_ = 100; // 初始筹码100
    CampaignConfig cfg_;
};