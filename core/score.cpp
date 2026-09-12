#include "score.h"

int calculateScore(const std::vector<Card>& cards) {
    int total = 0;
    for (const auto& card : cards) total += card.score;
    return total;
}

int calculateTableScore(const std::vector<Card>& cards, int bonus) {
    return calculateScore(cards) + bonus;
}

void settleScoreCards(Player& player, const std::vector<Card>& cards) {
    for (const auto& card : cards) {
        if (card.score > 0) {
            player.totalScore += card.score;
            player.collected.push_back(card);
        }
    }
}