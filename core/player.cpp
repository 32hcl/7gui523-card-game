#include "player.h"
#include "deck.h"

Player createPlayer(const std::string& name) {
    Player p;
    p.name = name;
    return p;
}

void refillToFive(Player& player, Deck& deck) {
    int need = 5 - static_cast<int>(player.hand.size());
    if (need > 0) {
        dealCards(player, deck, need);
    }
}

void removeCardsFromHand(Player& player, const std::vector<Card>& cardsToRemove) {
    for (const auto& card : cardsToRemove) {
        auto it = std::find_if(player.hand.begin(), player.hand.end(),
            [&card](const Card& c) {
                return c.point == card.point && c.suit == card.suit;
            });
        if (it != player.hand.end()) {
            player.hand.erase(it);
        }
    }
}

void printHand(const Player& player) {
    std::cout << player.name << " 的手牌 (" << player.hand.size() << " 张): ";
    if (player.hand.empty()) {
        std::cout << "(空)";
    } else {
        for (size_t i = 0; i < player.hand.size(); ++i) {
            std::cout << "[";
            printCard(player.hand[i]);
            std::cout << "]";
            if (i < player.hand.size() - 1) std::cout << " ";
        }
    }
    std::cout << std::endl;
}