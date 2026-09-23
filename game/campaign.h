#pragma once

#include <string>
#include <vector>
#include "game/game.h"

struct CampaignConfig {
    int playerInitHp = 200;
    int playerMaxHp  = 300;
    int bossMaxHp    = 110;
    int totalLevels  = 9;
    int healPerCard  = 2;
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
};

const std::vector<std::string>& getLevelRemoveTable(int level);

class Campaign {
public:
    Campaign();
    explicit Campaign(const CampaignConfig& cfg);

    void startNew();
    void startLevel(int level);
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

private:
    int playerHp_;
    int bossHp_;
    int currentLevel_;
    CampaignConfig cfg_;
};