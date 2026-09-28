#include "terminal.h"
#include "core/rule/score.h"
#include "cfr_types.h"

double computeTerminalUtility(
    const std::vector<Card>& myHand,
    const std::vector<Card>& oppHand,
    int myCollectedScore,
    int oppCollectedScore,
    int myDeckRemaining,
    int oppDeckRemaining,
    int tableScore,
    bool specialWin,
    bool isMyPerspective)
{
    if (specialWin) {
        return isMyPerspective ? 1.0 : -1.0;
    }

    int oppScoreCards = 0;
    for (const auto& c : oppHand) {
        if (c.score > 0) oppScoreCards += c.score;
    }

    int oppHandCount = (int)oppHand.size();

    int extraBonus = oppScoreCards + (oppHandCount + oppDeckRemaining) * 3;

    int myTotal = myCollectedScore + tableScore + extraBonus;

    int oppTotal = oppCollectedScore;

    double utility = (double)(myTotal - oppTotal) / kUtilityDenom;

    if (!isMyPerspective) utility = -utility;

    return utility;
}