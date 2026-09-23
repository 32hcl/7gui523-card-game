#include "play_selector.h"
#include "core/card/rank.h"
#include <climits>

std::vector<Card> tryFinishPlay(const Player& player,
                                 const std::vector<std::vector<Card>>& legalPlays,
                                 const CardTypeResult& previous) {
    if (previous.cards.empty()) {
        for (const auto& play : legalPlays) {
            if (play.size() == player.hand.size()) return play;
        }
    } else {
        for (const auto& play : legalPlays) {
            auto parsed = parseCardType(play);
            if (canBeat(parsed, previous) && play.size() == player.hand.size()) return play;
        }
    }
    return {};
}

std::vector<Card> tryEndgameIntercept(const Player& /*player*/,
                                       const std::vector<std::vector<Card>>& legalPlays,
                                       const CardTypeResult& previous,
                                       int opponentHandSize) {
    if (previous.cards.empty()) return {};
    if (opponentHandSize > 2) return {};

    std::vector<Card> rocket, smallestBomb;
    int smallestBombRank = INT_MAX;

    for (const auto& play : legalPlays) {
        auto parsed = parseCardType(play);
        if (!canBeat(parsed, previous)) continue;
        if (parsed.type == CardType::Rocket) {
            rocket = play;
        } else if (parsed.type == CardType::Bomb) {
            int rank = getCardRank(parsed.keyPoint);
            if (rank < smallestBombRank) {
                smallestBombRank = rank;
                smallestBomb = play;
            }
        }
    }

    if (!rocket.empty()) return rocket;
    if (!smallestBomb.empty()) return smallestBomb;
    return {};
}