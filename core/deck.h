#pragma once

#include <vector>
#include <random>
#include <algorithm>
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