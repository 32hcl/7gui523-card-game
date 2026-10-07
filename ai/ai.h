#pragma once

#include <vector>
#include "core/card/card.h"
#include "core/player.h"
#include "core/card/cardtype.h"
#include "core/card/deck.h"
#include "plugins/cardtracker.h"
#include "ai_types.h"

struct DecisionBreakdown {
    int base = 0;
    int scoreCard = 0;
    int special = 0;
    int endgame = 0;
    int comboBreak = 0;
    int tableScore = 0;
    int defensive = 0;
    int tracker = 0;
    int total = 0;
};

std::vector<std::vector<Card>> enumerateLegalPlays(const Player& player);

// ═══════════════════════════════════════════════════════════════════════════════
// 两大基础AI - 纯手写逻辑，不依赖AIEngine插件
// ═══════════════════════════════════════════════════════════════════════════════

// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║ AI1 可调参数（修改后重新编译即可探索新维度）                               ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
extern bool   g_ai1_zeroScorePass;   // 零分桌面跟不起时pass省炸 (default true, 已验证有效)

// AI1 规则（旧签名 — 无tracker/对手时自动退化到纯rank, 向后兼容）
std::vector<Card> aiChoosePlayAI1(const Player& player,
                                  const CardTypeResult& previous);

// AI1 规则（完整签名 — 支持CardTracker加权跟牌、tableScore决策）
std::vector<Card> aiChoosePlayAI1(const Player& player,
                                  const Player& opponent,
                                  const CardTypeResult& previous,
                                  const Deck& deck,
                                  int tableScore,
                                  const CardTracker& tracker);

// AI2 贪心：playGain单步评分最大化（拆对/炸弹保留/残局加分）
std::vector<Card> aiChoosePlayAI2(const Player& player,
                                  const Player& opponent,
                                  const CardTypeResult& previous,
                                  const Deck& deck,
                                  int tableScore);

// ═══════════════════════════════════════════════════════════════════════════════
// 插件AI - 基于AIEngine插件体系（minimax搜索/采样等）
// ═══════════════════════════════════════════════════════════════════════════════

// AI3 搜索：minimax博弈树搜索（深度8，看穿对手手牌）
std::vector<Card> aiChoosePlayAI3(const Player& player,
                                  const Player& opponent,
                                  const CardTypeResult& previous,
                                  const Deck& deck,
                                  int tableScore,
                                  const CardTracker& tracker);

// AI3 变体：minimax搜索（可调深度，默认6）
extern int g_searchBestPlayCheatDepth;
std::vector<Card> aiChoosePlayAI4(const Player& player,
                                  const Player& opponent,
                                  const CardTypeResult& previous,
                                  const Deck& deck,
                                  int tableScore,
                                  const CardTracker& tracker);

std::vector<Card> aiChoosePlay(const Player& player,
                               const Player& opponent,
                               const CardTypeResult& previous,
                               const Deck& deck,
                               int tableScore,
                               const CardTracker& tracker);

std::vector<Card> humanChoosePlay(const Player& player,
                                  const CardTypeResult& previous);

bool breaksCombo(const std::vector<Card>& play, const Player& player);

// searchBestPlayCheat 默认搜索深度（AI3变体使用，测试时可覆盖）

/* 修改：返回 DecisionBreakdown 替代裸指针输出参数，符合规范3.4 */
DecisionBreakdown evaluatePlayWithBreakdown(const std::vector<Card>& play,
                              const Player& player,
                              const Player& opponent,
                              const Deck& deck,
                              const CardTypeResult& previous,
                              int tableScore,
                              const CardTracker* tracker);