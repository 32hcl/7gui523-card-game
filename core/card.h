#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <map>

// 牌面顺序映射（从大到小），值越大牌越大
// 在 card.cpp 中定义
extern const std::map<std::string, int> RANK_MAP;

// 牌型枚举
enum class CardType {
    Single,         // 单张
    Pair,           // 对子
    Triple,         // 三张
    TripleWithOne,  // 三带一
    TripleWithTwo,  // 三带二
    Bomb,           // 炸弹
    Rocket,         // 王炸
    Invalid         // 非法
};

// Card 结构体
struct Card {
    std::string point;  // 点数，如 "3"、"10"、"J"、"大鬼"、"小鬼"
    std::string suit;   // 花色，如 "黑桃"、"红桃"、"梅花"、"方块"；鬼牌花色为空字符串
    int score;          // 分值，5/10/K 有分，其他为 0
};

// 辅助函数：打印一张牌
void printCard(const Card& card);

// 将 CardType 枚举转换为可读的中文字符串
std::string cardTypeToString(CardType type);