#include <cassert>
#include <iostream>
#include "game/campaign.h"
#include "core/player.h"

void test_campaignInit() {
    Campaign camp;
    assert(camp.playerHp() == 140);
    assert(camp.bossHp() == 110);
    assert(camp.currentLevel() == 1);
    assert(!camp.isGameOver());
    assert(!camp.isVictory());
}

void test_campaignStartLevel() {
    Campaign camp;
    camp.startNew();
    assert(camp.bossHp() == 110);

    camp.startLevel(5);
    assert(camp.currentLevel() == 5);
    assert(camp.bossHp() == 110);
}

void test_removeTable() {
    {
        auto& tbl = getLevelRemoveTable(1);
        assert(tbl.size() == 1);
        assert(tbl[0] == "4");
    }
    {
        auto& tbl = getLevelRemoveTable(2);
        assert(tbl.size() == 2);
        assert(tbl[0] == "4");
        assert(tbl[1] == "4");
    }
    {
        auto& tbl = getLevelRemoveTable(9);
        assert(tbl.size() == 10);
    }
    {
        auto& tbl = getLevelRemoveTable(0);
        assert(tbl.empty());
    }
    {
        auto& tbl = getLevelRemoveTable(10);
        assert(tbl.empty());
    }
}

void test_campaignSettlement() {
    Campaign camp;

    RoundResult rr;
    rr.winnerName = "player";
    rr.handEmptied = true;

    Player player = createPlayer("player");
    player.totalScore = 30;

    Player boss = createPlayer("boss");
    boss.totalScore = 10;

    LevelResult lr = camp.finishLevel(rr, player, boss, 2, 3);

    assert(lr.playerWon == true);
    assert(camp.bossHp() == 110 - 30);  // 80

    // playerHp = 140 - 10 + (2+3)*3 = 145
    assert(camp.playerHp() == 145);
}

void test_campaignSettlementBossWins() {
    Campaign camp;

    RoundResult rr;
    rr.winnerName = "boss";
    rr.handEmptied = true;

    Player player = createPlayer("player");
    player.totalScore = 10;

    Player boss = createPlayer("boss");
    boss.totalScore = 30;

    LevelResult lr = camp.finishLevel(rr, player, boss, 0, 5);

    assert(lr.playerWon == false);
    assert(camp.bossHp() == 110 - 10);  // 100

    // playerHp = 140 - 30 - (0+5)*3 = 95
    assert(camp.playerHp() == 95);
}

void test_campaignHpCap() {
    CampaignConfig cfg;
    cfg.playerInitHp = 200;
    cfg.playerMaxHp = 300;

    Campaign camp(cfg);

    RoundResult rr;
    rr.winnerName = "player";
    rr.handEmptied = true;

    Player player = createPlayer("player");
    player.totalScore = 30;

    Player boss = createPlayer("boss");
    boss.totalScore = 0;
    boss.hand = {};

    LevelResult lr = camp.finishLevel(rr, player, boss, 20, 20);
    // totalRemain = 40, heal = 80
    // playerHp = 200 - 0 + 80 = 280, capped at 300
    assert(camp.playerHp() == 300);
    assert(camp.playerHp() <= cfg.playerMaxHp);
}

void test_campaignGameOver() {
    CampaignConfig cfg;
    cfg.playerInitHp = 30;

    Campaign camp(cfg);

    RoundResult rr;
    rr.winnerName = "boss";
    rr.handEmptied = true;

    Player player = createPlayer("player");
    player.totalScore = 5;

    Player boss = createPlayer("boss");
    boss.totalScore = 30;
    boss.hand = {};

    camp.finishLevel(rr, player, boss, 10, 10);
    // playerHp = 30 - 30 = 0 → game over
    assert(camp.playerHp() <= 0);
    assert(camp.isGameOver());
}