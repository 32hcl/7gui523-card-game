#pragma once

#include <vector>
#include "core/card/card.h"
#include "core/player.h"
#include "core/card/cardtype.h"
#include "core/card/deck.h"
#include "core/tracker/cardtracker.h"
#include "ai/ai_types.h"

class Evaluator {
public:
    virtual ~Evaluator() = default;
    virtual int evaluate(const std::vector<Card>& play,
                         const Player& player,
                         const Player& opponent,
                         const Deck& deck,
                         const CardTypeResult& previous,
                         int tableScore,
                         const CardTracker* tracker,
                         DeckSide mySide = DeckSide::PlayerA) const = 0;
};

class SimpleEvaluator : public Evaluator {
public:
    int evaluate(const std::vector<Card>& play,
                 const Player& player,
                 const Player& opponent,
                 const Deck& deck,
                 const CardTypeResult& previous,
                 int tableScore,
                 const CardTracker* tracker,
                 DeckSide mySide = DeckSide::PlayerA) const override;
};

class SmartEvaluator : public Evaluator {
public:
    int evaluate(const std::vector<Card>& play,
                 const Player& player,
                 const Player& opponent,
                 const Deck& deck,
                 const CardTypeResult& previous,
                 int tableScore,
                 const CardTracker* tracker,
                 DeckSide mySide = DeckSide::PlayerA) const override;
};

class AdvancedEvaluator : public Evaluator {
public:
    explicit AdvancedEvaluator(const AIParams& params = AIParams());
    int evaluate(const std::vector<Card>& play,
                 const Player& player,
                 const Player& opponent,
                 const Deck& deck,
                 const CardTypeResult& previous,
                 int tableScore,
                 const CardTracker* tracker,
                 DeckSide mySide = DeckSide::PlayerA) const override;
private:
    AIParams params_;
};