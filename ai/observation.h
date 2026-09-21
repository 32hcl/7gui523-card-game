#pragma once
#include <vector>
#include "core/card/card.h"
#include "core/card/cardtype.h"

// AI 合法观测：不包含真实对手手牌、不包含未公开牌堆信息
struct AIObservation {
    std::vector<Card> myHand;           // 我的手牌（已知）
    int myScore;                        // 我的累计得分
    int oppHandCount;                   // 对手手牌剩余张数（不包含具体牌面）
    int oppScore;                       // 对手累计得分（比分可知）
    int deckCount;                      // 牌堆剩余张数（不包含具体牌面）
    CardTypeResult lastPlay;            // 上一手牌型
    int tableScore;                     // 桌面分
    bool isMyTurn;                      // 是否轮到我出
    
    // CardTracker 公开信息：每张牌点数的剩余数量
    // 通过 "总数 - 已打出 - 我手牌中" 推导，不暴露对手具体手牌
    std::map<std::string, int> unseenCount;  // 每点数未出现张数（= 牌堆 + 对手手牌）
    std::map<std::string, int> totalCount;   // 每点数总数（4 或 1）
    std::map<std::string, int> playedCount;  // 每点数已打出张数
};