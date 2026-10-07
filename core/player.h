#pragma once

#include <string>
#include <vector>
#include "core/card/card.h"

constexpr int kMaxHandSize = 5;

struct Deck;

// ─── 两大基础AI ──────────────────────────────────────────────────────────────
enum class AILevel {
    AI1_Rule,    // 规则：最小单张→对子→同牌型最小→最小炸弹→王炸
    AI2_Greedy,  // 贪心：playGain单步评分最大化
    AI3_Search,  // 搜索：minimax作弊搜索，深度8（非基础AI）
    AI4_Search   // 搜索：AI3的可调深度变体，默认深度6（非基础AI）
};

struct Player {
    std::string name;
    std::vector<Card> hand;
    int totalScore = 0;
    std::vector<Card> collected;
    bool isHuman = false;
    AILevel aiLevel = AILevel::AI1_Rule;
};

Player createPlayer(const std::string& name);
void refillToFive(Player& player, Deck& deck);
void removeCardsFromHand(Player& player, const std::vector<Card>& cardsToRemove);
void printHand(const Player& player);