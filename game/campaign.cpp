#include "game/campaign.h"
#include <cstdio>
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
    fprintf(stderr, "[startLevel] enter level=%d playerHp(before)=%d bossHp(before)=%d\n",
            level, playerHp_, bossHp_);
    currentLevel_ = level;
    bossHp_ = cfg_.bossMaxHp;
    fprintf(stderr, "[startLevel] exit, playerHp=%d bossHp=%d\n",
            playerHp_, bossHp_);
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
    fprintf(stderr, "[finishLevel] enter, playerHp=%d bossHp=%d playerScore=%d bossScore=%d\n",
            playerHp_, bossHp_, player.totalScore, boss.totalScore);
    LevelResult lr;
    lr.playerScore = player.totalScore;
    lr.bossScore = boss.totalScore;
    lr.playerRemainCards = static_cast<int>(player.hand.size()) + playerDeckRemain;
    lr.bossRemainCards = static_cast<int>(boss.hand.size()) + bossDeckRemain;

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
    } else {
        playerHp_ -= healAmount;
    }

    playerHp_ = std::min(playerHp_, cfg_.playerMaxHp);

    lr.playerHpDelta = playerHp_ - prevPlayerHp;
    lr.bossHpDelta = bossHp_ - prevBossHp;

    fprintf(stderr, "[finishLevel] exit, playerHp=%d bossHp=%d playerWon=%d bossCleared=%d\n",
            playerHp_, bossHp_, (int)lr.playerWon, (int)lr.bossCleared);

    return lr;
}

bool Campaign::isGameOver() const {
    return playerHp_ <= 0;
}

bool Campaign::isVictory() const {
    return currentLevel_ >= cfg_.totalLevels && bossHp_ <= 0;
}