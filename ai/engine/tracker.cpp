#include "tracker.h"
#include "core/card/deck.h"

void NoTracker::recordPlayed(const std::vector<Card>& cards) {
    (void)cards;
}

bool NoTracker::isExhausted(const std::string& point) const {
    (void)point;
    return false;
}

int NoTracker::playedCount(const std::string& point) const {
    (void)point;
    return 0;
}

double NoTracker::expectedOpponentCount(const std::string& point,
                                         int opponentHandSize,
                                         int deckSize) const {
    (void)point;
    (void)opponentHandSize;
    (void)deckSize;
    return 0.0;
}

const CardTracker* NoTracker::getImpl() const {
    return nullptr;
}

BasicTracker::BasicTracker() : impl_() {}

void BasicTracker::recordPlayed(const std::vector<Card>& cards) {
    impl_.recordPlayed(cards);
}

bool BasicTracker::isExhausted(const std::string& point) const {
    return impl_.isExhausted(point);
}

int BasicTracker::playedCount(const std::string& point) const {
    return impl_.playedCount(point);
}

double BasicTracker::expectedOpponentCount(const std::string& point,
                                            int opponentHandSize,
                                            int deckSize) const {
    int remaining = impl_.remainingCount(point);
    int totalUnknown = opponentHandSize + deckSize;
    if (totalUnknown == 0) return 0.0;
    return (double)remaining * opponentHandSize / totalUnknown;
}

const CardTracker* BasicTracker::getImpl() const {
    return &impl_;
}