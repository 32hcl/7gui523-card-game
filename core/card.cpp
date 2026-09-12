#include "card.h"

// 牌面顺序映射（从大到小），值越大牌越大
const std::map<std::string, int> RANK_MAP = {
    {"7", 15},
    {"大鬼", 14},
    {"小鬼", 13},
    {"5", 12},
    {"2", 11},
    {"3", 10},
    {"A", 9},
    {"K", 8},
    {"Q", 7},
    {"J", 6},
    {"10", 5},
    {"9", 4},
    {"8", 3},
    {"6", 2},
    {"4", 1}
};

// 将 CardType 枚举转换为可读的中文字符串
std::string cardTypeToString(CardType type) {
    switch (type) {
        case CardType::Single:        return "单张";
        case CardType::Pair:          return "对子";
        case CardType::Triple:        return "三张";
        case CardType::TripleWithOne: return "三带一";
        case CardType::TripleWithTwo: return "三带二";
        case CardType::Bomb:          return "炸弹";
        case CardType::Rocket:        return "王炸";
        case CardType::Invalid:       return "非法";
        default:                      return "未知";
    }
}

// 辅助函数：打印一张牌
void printCard(const Card& card) {
    if (card.suit.empty()) {
        // 鬼牌没有花色，直接打印点数
        std::cout << card.point;
    } else {
        // 普通牌打印"花色点数"
        std::cout << card.suit << card.point;
    }
}
