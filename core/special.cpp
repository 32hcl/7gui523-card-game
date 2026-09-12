#include "special.h"
#include "card.h"

bool checkSpecialVictory(const Player& player) {
    bool has7 = false, hasJoker = false, has5 = false, has2 = false, has3 = false;
    for (const auto& card : player.hand) {
        if (card.point == "7")       has7 = true;
        if (card.point == "大鬼" || card.point == "小鬼") hasJoker = true;
        if (card.point == "5")       has5 = true;
        if (card.point == "2")       has2 = true;
        if (card.point == "3")       has3 = true;
    }
    return has7 && hasJoker && has5 && has2 && has3;
}