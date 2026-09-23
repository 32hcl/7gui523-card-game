#pragma once

#include <string>
#include "core/card/card.h"
#include "core/tracker/cardtracker.h"

class Tracker {
public:
    virtual ~Tracker() = default;
    virtual void recordPlayed(const std::vector<Card>& cards) = 0;
    virtual bool isExhausted(const std::string& point) const = 0;
    virtual int playedCount(const std::string& point) const = 0;
    virtual double expectedOpponentCount(const std::string& point,
                                          int opponentHandSize,
                                          int deckSize) const = 0;
    virtual const CardTracker* getImpl() const = 0;
};

class NoTracker : public Tracker {
public:
    void recordPlayed(const std::vector<Card>& cards) override;
    bool isExhausted(const std::string& point) const override;
    int playedCount(const std::string& point) const override;
    double expectedOpponentCount(const std::string& point,
                                  int opponentHandSize,
                                  int deckSize) const override;
    const CardTracker* getImpl() const override;
};

class BasicTracker : public Tracker {
public:
    BasicTracker();
    void recordPlayed(const std::vector<Card>& cards) override;
    bool isExhausted(const std::string& point) const override;
    int playedCount(const std::string& point) const override;
    double expectedOpponentCount(const std::string& point,
                                  int opponentHandSize,
                                  int deckSize) const override;
    const CardTracker* getImpl() const override;
private:
    CardTracker impl_;
};