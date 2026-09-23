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
}

void Campaign::startLevel(int level) {
    currentLevel_ = level;
    bossHp_ = cfg_.bossMaxHp;
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

    int prevPlayerHp = playerHp_;
    int prevBossHp = bossHp_;

    bossHp_ -= player.totalScore;
    playerHp_ -= boss.totalScore;

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
    } else {
        playerHp_ -= healAmount;
    }

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