#include "game.h"
#include "core/rule/score.h"
#include <iostream>

void finalSettlement(Player& finisher, Player& opponent, std::vector<Card>& tableCards) {
    std::cout << std::endl;
    std::cout << "--- 终局结算开始 ---" << std::endl;
    std::cout << "出完牌者: " << finisher.name << std::endl;

    int tableScore = calculateScore(tableCards);
    if (!tableCards.empty()) {
        std::cout << "桌面牌 (共 " << tableCards.size() << " 张): ";
        for (size_t i = 0; i < tableCards.size(); ++i) {
            printCard(tableCards[i]);
            std::cout << "(" << tableCards[i].score << "分)";
            if (i < tableCards.size() - 1) std::cout << " ";
        }
        std::cout << std::endl;
    }
    std::cout << "桌面分值: " << tableScore << " 分" << std::endl;
    if (tableScore > 0) std::cout << finisher.name << " 获得桌面分值 " << tableScore << " 分" << std::endl;
    settleScoreCards(finisher, tableCards);

    int opponentScore = calculateScore(opponent.hand);
    if (!opponent.hand.empty()) {
        std::cout << opponent.name << " 剩余手牌 (共 " << opponent.hand.size() << " 张): ";
        for (size_t i = 0; i < opponent.hand.size(); ++i) {
            printCard(opponent.hand[i]);
            std::cout << "(" << opponent.hand[i].score << "分)";
            if (i < opponent.hand.size() - 1) std::cout << " ";
        }
        std::cout << std::endl;
    }
    std::cout << opponent.name << " 手牌分值: " << opponentScore << " 分" << std::endl;
    if (opponentScore > 0) std::cout << finisher.name << " 获得 " << opponent.name << " 手牌分值 " << opponentScore << " 分" << std::endl;
    settleScoreCards(finisher, opponent.hand);

    int totalObtained = tableScore + opponentScore;
    std::cout << finisher.name << " 本次终局共获得 " << totalObtained << " 分";
    std::cout << " (桌面 " << tableScore << " 分 + " << opponent.name << " 手牌 " << opponentScore << " 分)" << std::endl;

    opponent.hand.clear();
    tableCards.clear();
    std::cout << "--- 终局结算结束 ---" << std::endl;
    std::cout << std::endl;
}

void compareAndAnnounce(const Player& a, const Player& b) {
    std::cout << "========== 游戏结束 ==========" << std::endl;
    std::cout << a.name << " 总分: " << a.totalScore << std::endl;
    std::cout << b.name << " 总分: " << b.totalScore << std::endl;
    if (a.totalScore > b.totalScore) std::cout << "最终胜者: " << a.name << std::endl;
    else if (b.totalScore > a.totalScore) std::cout << "最终胜者: " << b.name << std::endl;
    else std::cout << "最终结果: 平局！" << std::endl;
}