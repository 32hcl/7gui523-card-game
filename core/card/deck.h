#pragma once

#include <vector>
#include <string>
#include "card.h"

// 玩家结构体前向声明
struct Player;

// Deck 结构体
struct Deck {
    std::vector<Card> cards;
};

// 创建一副标准 54 张牌（含大鬼、小鬼）
Deck createStandardDeck();

// 洗牌
void shuffleDeck(Deck& deck);

// 从牌堆顶抽取指定数量的牌
std::vector<Card> drawCards(Deck& deck, int count);

// 给玩家发牌
void dealCards(Player& player, Deck& deck, int count);

// 从标准 54 张中按点数剔除指定牌（花色不限）
std::vector<Card> removeCards(const std::vector<Card>& full,
                              const std::vector<std::string>& pointsToRemove);

// 从牌池随机抽 n 张（不改变原池内容，返回抽出的牌）
std::vector<Card> drawRandom(std::vector<Card>& pool, int n);

// 洗牌后对半切（27+27）
void splitDeck(std::vector<Card>& deck,
               std::vector<Card>& outPlayer,
               std::vector<Card>& outAI);