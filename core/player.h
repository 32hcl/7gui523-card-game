#pragma once

#include <string>
#include <vector>
#include "core/card/card.h"

constexpr int kMaxHandSize = 5;

struct Deck;

enum class AILevel {
    AI1_Simple,
    AI2_Rule,
    AI3_Tracker,
    AI4_Expert,
    AI_Fair_Lv1,
    AI_Fair_Lv2
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