 #pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "core/card/deck.h"
#include "core/card/cardtype.h"
#include "core/player.h"
#include "ai/ai.h"
#include "ai/ai_types.h"
#include "ai/engine/ai_engine.h"
#include "ai/ai_levels.h"
#include "ai/players/ai1_idiot.h"
#include "game/level_boss.h"

using AIFn = std::function<std::vector<Card>(
    Player& cur, Player& opp,
    const CardTypeResult& prev,
    const Deck& deck,
    int tableScore,
    CardTracker& tracker)>;

inline AIFn makeNewEngine(int level, int depthOverride) {
    auto enginePtr = std::make_shared<AIEngine>(AIEngineConfig::defaultConfig());
    return [level, depthOverride, enginePtr, initialized = false]
           (Player& cur, Player& opp, const CardTypeResult& prev,
            const Deck& deck, int tableScore, CardTracker&) mutable -> std::vector<Card> {
        constexpr int kTotalCards = 54;
        constexpr int kDecksAfterFirstRound = kTotalCards - kMaxHandSize * 2 - 2;
        if (!initialized || (prev.type == CardType::Invalid && deck.cards.size() == kDecksAfterFirstRound)) {
            AIEngineConfig cfg = buildAIEngineConfig(level);
            if (depthOverride > 0) cfg.searchDepth = depthOverride;
            *enginePtr = AIEngine(cfg);
            initialized = true;
        }
        enginePtr->setOpponentHand(opp.hand);
        return enginePtr->choosePlay(cur, opp, prev, deck, tableScore);
    };
}

inline AIFn makeOldAI(int level, int depthOverride) {
    return [level, depthOverride](Player& cur, Player& opp, const CardTypeResult& prev,
                   const Deck& deck, int tableScore, CardTracker& tracker) -> std::vector<Card> {
        if (level == 1) {
            (void)tableScore;
            return aiChoosePlayAI1(cur, opp, prev, deck, tableScore, tracker);
        }
        if (level == 2) {
            (void)tracker;
            return aiChoosePlayAI2(cur, opp, prev, deck, tableScore);
        }
        if (level == 3) {
            int oldDepth = g_searchBestPlayCheatDepth;
            if (depthOverride > 0) g_searchBestPlayCheatDepth = depthOverride;
            auto result = aiChoosePlayAI3(cur, opp, prev, deck, tableScore, tracker);
            if (depthOverride > 0) g_searchBestPlayCheatDepth = oldDepth;
            return result;
        }
        return ai1_idiot_choose(cur, opp, prev, deck, tableScore);
    };
}

inline std::string aiName(int id) {
    if (id == 1)   return "AI1规则";
    if (id == 2)   return "AI2贪心";
    if (id == 3)   return "AI3搜索";
    if (id == 100) return "搜索(旧)";
    if (id == 101) return "贪心(旧)";
    if (id == 102) return "保守(旧)";
    return "?";
}