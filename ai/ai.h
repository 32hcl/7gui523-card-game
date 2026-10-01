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

std::vector<Card> aiChoosePlayAI1(const Player& player,
                                  const CardTypeResult& previous);

std::vector<Card> aiChoosePlayAI2(const Player& player,
                                  const Player& opponent,
                                  const CardTypeResult& previous,
                                  const Deck& deck,
                                  int tableScore);

std::vector<Card> aiChoosePlayAI3(const Player& player,
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

// 可覆盖的搜索深度（旧AI4用searchBestPlayCheat硬编码6，现改为变量便于测试）
extern int g_searchBestPlayCheatDepth;

int evaluatePlayWithBreakdown(const std::vector<Card>& play,
                              const Player& player,
                              const Player& opponent,
                              const Deck& deck,
                              const CardTypeResult& previous,
                              int tableScore,
                              const CardTracker* tracker,
                              DecisionBreakdown* out);