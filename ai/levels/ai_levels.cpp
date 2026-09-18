#include "ai/ai.h"
#include "ai/ai_types.h"
#include "core/player.h"

struct StrengthParams {
    int searchDepth = 0;
    AIParams ai4Params;
};

static StrengthParams getStrengthParams(AILevel level) {
    StrengthParams sp;
    switch (level) {
        case AILevel::AI1_Simple:
            sp.searchDepth = 0;
            break;
        case AILevel::AI2_Rule:
            sp.searchDepth = 0;
            break;
        case AILevel::AI3_Tracker:
            sp.searchDepth = 0;
            break;
        case AILevel::AI4_Expert:
            sp.searchDepth = 6;
            sp.ai4Params = getAI4Params();
            break;
    }
    return sp;
}