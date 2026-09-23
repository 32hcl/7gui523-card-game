#include "cardtracker.h"

CardTracker::CardTracker() {
    std::vector<std::string> points = {
        "A","2","3","4","5","6","7","8","9","10","J","Q","K"
    };
    for (const auto& p : points) {
        m_totalCount[p] = 4;
        m_playedCountPlayerA[p] = 0;
        m_playedCountBoss[p] = 0;
    }
    m_totalCount["大鬼"] = 1;
    m_totalCount["小鬼"] = 1;
    m_playedCountPlayerA["大鬼"] = 0;
    m_playedCountPlayerA["小鬼"] = 0;
    m_playedCountBoss["大鬼"] = 0;
    m_playedCountBoss["小鬼"] = 0;
}

void CardTracker::reset() {
    for (auto& kv : m_playedCountPlayerA) kv.second = 0;
    for (auto& kv : m_playedCountBoss) kv.second = 0;
}

const std::map<std::string, int>& CardTracker::getPlayedMap(DeckSide side) const {
    return (side == DeckSide::PlayerA) ? m_playedCountPlayerA : m_playedCountBoss;
}

std::map<std::string, int>& CardTracker::getPlayedMap(DeckSide side) {
    return (side == DeckSide::PlayerA) ? m_playedCountPlayerA : m_playedCountBoss;
}

// --- 向后兼容（默认 PlayerA） ---

void CardTracker::recordPlayed(const std::vector<Card>& cards) {
    recordPlayed(cards, DeckSide::PlayerA);
}

int CardTracker::remainingCount(const std::string& point) const {
    return remainingCount(point, DeckSide::PlayerA);
}

bool CardTracker::isExhausted(const std::string& point) const {
    return isExhausted(point, DeckSide::PlayerA);
}

int CardTracker::playedCount(const std::string& point) const {
    return playedCount(point, DeckSide::PlayerA);
}

int CardTracker::totalCount(const std::string& point) const {
    auto it = m_totalCount.find(point);
    return (it == m_totalCount.end()) ? 0 : it->second;
}

double CardTracker::expectedOpponentCount(const std::string& point, int oppHandSize, int deckSize) const {
    return expectedOpponentCount(point, oppHandSize, deckSize, DeckSide::PlayerA);
}

double CardTracker::expectedDeckCount(const std::string& point, int oppHandSize, int deckSize) const {
    return expectedDeckCount(point, oppHandSize, deckSize, DeckSide::PlayerA);
}

// --- 分侧追踪新实现 ---

void CardTracker::recordPlayed(const std::vector<Card>& cards, DeckSide side) {
    auto& played = getPlayedMap(side);
    for (const Card& c : cards) {
        played[c.point]++;
    }
}

int CardTracker::playedCount(const std::string& point, DeckSide side) const {
    const auto& played = getPlayedMap(side);
    auto it = played.find(point);
    return (it == played.end()) ? 0 : it->second;
}

int CardTracker::remainingCount(const std::string& point, DeckSide side) const {
    auto it = m_totalCount.find(point);
    if (it == m_totalCount.end()) return 0;
    return it->second - playedCount(point, side);
}

bool CardTracker::isExhausted(const std::string& point, DeckSide side) const {
    return remainingCount(point, side) <= 0;
}

double CardTracker::expectedOpponentCount(const std::string& point, int oppHandSize, int oppDeckSize, DeckSide mySide) const {
    // 对方的侧 = 与 mySide 相反
    DeckSide oppSide = (mySide == DeckSide::PlayerA) ? DeckSide::Boss : DeckSide::PlayerA;
    int unknown = remainingCount(point, oppSide);
    int total = oppHandSize + oppDeckSize;
    if (total == 0) return 0.0;
    return (double)unknown * oppHandSize / total;
}

double CardTracker::expectedDeckCount(const std::string& point, int oppHandSize, int deckSize, DeckSide side) const {
    int unknown = remainingCount(point, side);
    int total = oppHandSize + deckSize;
    if (total == 0) return 0.0;
    return (double)unknown * deckSize / total;
}