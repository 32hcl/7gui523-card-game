#pragma once
#include "core/card/card.h"
#include <vector>
#include <map>
#include <string>

enum class DeckSide { PlayerA, Boss };

class CardTracker {
public:
    CardTracker();
    void reset();

    void recordPlayed(const std::vector<Card>& cards);
    int remainingCount(const std::string& point) const;
    bool isExhausted(const std::string& point) const;
    int totalCount(const std::string& point) const;
    int playedCount(const std::string& point) const;
    double expectedOpponentCount(const std::string& point, int oppHandSize, int deckSize) const;
    double expectedDeckCount(const std::string& point, int oppHandSize, int deckSize) const;

    void recordPlayed(const std::vector<Card>& cards, DeckSide side);
    int playedCount(const std::string& point, DeckSide side) const;
    int remainingCount(const std::string& point, DeckSide side) const;
    bool isExhausted(const std::string& point, DeckSide side) const;
    double expectedOpponentCount(const std::string& point, int oppHandSize, int oppDeckSize, DeckSide mySide) const;
    double expectedDeckCount(const std::string& point, int oppHandSize, int deckSize, DeckSide side) const;

private:
    std::map<std::string, int> m_totalCount;
    std::map<std::string, int> m_playedCountPlayerA;
    std::map<std::string, int> m_playedCountBoss;

    const std::map<std::string, int>& getPlayedMap(DeckSide side) const;
    std::map<std::string, int>& getPlayedMap(DeckSide side);
};