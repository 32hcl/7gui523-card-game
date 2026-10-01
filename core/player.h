#pragma once

#include <string>
#include <vector>
#include "core/card/card.h"

constexpr int kMaxHandSize = 5;

struct Deck;

enum class AILevel {
    AI1_Simple,  // 保守
    AI2_Rule,    // 贪心
    AI4_Expert   // 搜索
};

struct Player {
    std::string name;
    std::vector<Card> hand;
    int totalScore = 0;
    std::vector<Card> collected;
    bool isHuman = false;
    AILevel aiLevel = AILevel::AI1_Simple;
};

Player createPlayer(const std::string& name);
void refillToFive(Player& player, Deck& deck);
void removeCardsFromHand(Player& player, const std::vector<Card>& cardsToRemove);
void printHand(const Player& player);