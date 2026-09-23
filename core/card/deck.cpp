#include "deck.h"
#include "core/player.h"
#include <random>
#include <algorithm>

Deck createStandardDeck() {
    Deck deck;
    std::vector<std::string> suits = {"黑桃", "红桃", "梅花", "方块"};
    std::vector<std::string> points = {"A", "2", "3", "4", "5", "6", "7",
                                       "8", "9", "10", "J", "Q", "K"};
    for (const auto& suit : suits) {
        for (const auto& point : points) {
            Card card;
            card.point = point;
            card.suit  = suit;
            if (point == "5")       card.score = 5;
            else if (point == "10") card.score = 10;
            else if (point == "K")  card.score = 20;
            else                    card.score = 0;
            deck.cards.push_back(card);
        }
    }
    Card bigJoker;
    bigJoker.point = "大鬼"; bigJoker.suit = ""; bigJoker.score = 0;
    deck.cards.push_back(bigJoker);
    Card smallJoker;
    smallJoker.point = "小鬼"; smallJoker.suit = ""; smallJoker.score = 0;
    deck.cards.push_back(smallJoker);
    return deck;
}

void shuffleDeck(Deck& deck) {
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(deck.cards.begin(), deck.cards.end(), g);
}

std::vector<Card> drawCards(Deck& deck, int count) {
    std::vector<Card> drawn;
    int actualCount = std::min(count, static_cast<int>(deck.cards.size()));
    for (int i = 0; i < actualCount; ++i) {
        drawn.push_back(deck.cards.back());
        deck.cards.pop_back();
    }
    return drawn;
}

void dealCards(Player& player, Deck& deck, int count) {
    std::vector<Card> drawn = drawCards(deck, count);
    player.hand.insert(player.hand.end(), drawn.begin(), drawn.end());
}

std::vector<Card> removeCards(const std::vector<Card>& full,
                              const std::vector<std::string>& pointsToRemove) {
    std::vector<Card> result;
    std::vector<std::string> remaining = pointsToRemove;

    for (const auto& card : full) {
        auto it = std::find(remaining.begin(), remaining.end(), card.point);
        if (it != remaining.end()) {
            remaining.erase(it);
        } else {
            result.push_back(card);
        }
    }
    return result;
}

std::vector<Card> drawRandom(std::vector<Card>& pool, int n) {
    int actual = std::min(n, static_cast<int>(pool.size()));
    if (actual <= 0) return {};

    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(pool.begin(), pool.end(), g);

    std::vector<Card> drawn;
    for (int i = 0; i < actual; ++i) {
        drawn.push_back(pool.back());
        pool.pop_back();
    }
    return drawn;
}

void splitDeck(std::vector<Card>& deck,
               std::vector<Card>& outPlayer,
               std::vector<Card>& outAI) {
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(deck.begin(), deck.end(), g);

    int half = static_cast<int>(deck.size()) / 2;
    outPlayer.clear();
    outAI.clear();

    for (int i = 0; i < half && !deck.empty(); ++i) {
        outPlayer.push_back(deck.back());
        deck.pop_back();
    }
    for (int i = 0; i < half && !deck.empty(); ++i) {
        outAI.push_back(deck.back());
        deck.pop_back();
    }
}