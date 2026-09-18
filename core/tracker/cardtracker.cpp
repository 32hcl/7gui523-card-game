#include "cardtracker.h"

CardTracker::CardTracker() {
    std::vector<std::string> points = {
        "A","2","3","4","5","6","7","8","9","10","J","Q","K"
    };
    for (const auto& p : points) {
        m_totalCount[p] = 4;
        m_playedCount[p] = 0;
    }
    m_totalCount["大鬼"] = 1;
    m_totalCount["小鬼"] = 1;
    m_playedCount["大鬼"] = 0;
    m_playedCount["小鬼"] = 0;
}

void CardTracker::reset() {
    for (auto& kv : m_playedCount) kv.second = 0;
}

void CardTracker::recordPlayed(const std::vector<Card>& cards) {
    for (const Card& c : cards) {
        m_playedCount[c.point]++;
    }
}

int CardTracker::remainingCount(const std::string& point) const {
    auto it = m_totalCount.find(point);
    if (it == m_totalCount.end()) return 0;
    auto pit = m_playedCount.find(point);
    int played = (pit == m_playedCount.end()) ? 0 : pit->second;
    return it->second - played;
}

bool CardTracker::isExhausted(const std::string& point) const {
    return remainingCount(point) <= 0;
}

int CardTracker::totalCount(const std::string& point) const {
    auto it = m_totalCount.find(point);
    return (it == m_totalCount.end()) ? 0 : it->second;
}

int CardTracker::playedCount(const std::string& point) const {
    auto it = m_playedCount.find(point);
    return (it == m_playedCount.end()) ? 0 : it->second;
}
