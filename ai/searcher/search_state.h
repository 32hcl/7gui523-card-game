#pragma once
#include <vector>
#include <cstdint>
#include "core/card/card.h"
#include "core/card/cardtype.h"

// 局面状态：搜索时在节点间传递
struct SearchState {
    std::vector<Card> myHand;       // 我的手牌
    std::vector<Card> oppHand;      // 对手手牌（作弊版用）
    std::vector<Card> deckCards;    // 剩余牌堆
    CardTypeResult lastPlay;        // 上一手
    bool myTurn = true;             // 现在轮到谁出
    int tableScore = 0;             // 桌面分
    int myScore = 0;                // 我累计得分
    int oppScore = 0;               // 对手累计得分
    bool terminal = false;          // 是否终局
    int winner = 0;                 // 0=未结束，1=我赢，-1=对手赢
};