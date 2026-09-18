#pragma once
#include <vector>
#include <string>

struct AIParams {
    // 出牌张数权重
    int cardCountWeight = 30;
    // 出完奖励
    int finishBonus = 2035;

    // 桌面分基础收益（每次出牌都加 tableScore * tableScoreWeight）
    int tableScoreWeight = 2;

    // 前期
    int earlyBigPenalty = 52;    // 出 10/K 的惩罚
    int earlyMidPenalty = 18;    // 出 5 的惩罚

    // 中期
    int midScorePenalty = 2;     // 出分牌惩罚（仅 tableScore==0 时生效）

    // 抢分
    int stealThreshold = 11;     // 桌面分达到多少才开始考虑抢
    int stealMultiplier = 4;     // 抢分收益倍数
    int noConfidencePenalty = 1; // 没把握时的惩罚系数

    // 牌堆顶
    int deckTopBonus = 9;        // 牌堆顶是分牌时的加成
    int deckTopSpecialPenalty = 41;

    // 拆牌
    int splitPairPenalty = 17;

    // 特殊胜利
    int specialKeepBonus = 93;
    int specialMinRemaining = 1; // 缺的牌至少要剩这么多张才保留

    // 炸弹保留
    int bombKeepPenalty = 92;
    int rocketKeepPenalty = 459;

    // 对方手牌少时炸弹加成
    int endgameBombBonus = 236;
    int endgameRocketBonus = 223;

    // 序列化为向量（用于遗传算法）
    std::vector<int> toVector() const;
    static AIParams fromVector(const std::vector<int>& v);
    static int paramCount();
};