#pragma once
#include "card.h"
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

private:
    std::map<std::string, int> m_totalCount;
    std::map<std::string, int> m_playedCount;
};
