#pragma once
#include <string>
#include "card.h"

inline int getCardRank(const std::string& point) {
    auto it = RANK_MAP.find(point);
    if (it != RANK_MAP.end()) return it->second;
    if (point == "王炸")       return 100;
    if (point == "Special523") return 999;
    return 0;
}