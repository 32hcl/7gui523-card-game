#pragma once
#include "core/card/card.h"
#include <vector>
#include <map>
#include <string>

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

private:
    std::map<std::string, int> m_totalCount;
    std::map<std::string, int> m_playedCount;
};