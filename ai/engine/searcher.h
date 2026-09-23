#pragma once

#include <vector>
#include <memory>
#include "core/card/card.h"
#include "core/player.h"
#include "core/card/cardtype.h"
#include "core/card/deck.h"
#include "core/tracker/cardtracker.h"
#include "evaluator.h"
#include "state_evaluator.h"

class Searcher {
public:
    virtual ~Searcher() = default;
    virtual std::vector<Card> search(const Player& player,
                                     const Player& opponent,
                                     const CardTypeResult& previous,
                                     const Deck& deck,
                                     int tableScore,
                                     const CardTracker* tracker,
                                     const Evaluator* evaluator,
                                     DeckSide mySide = DeckSide::PlayerA) = 0;
};

class NoSearcher : public Searcher {
public:
    std::vector<Card> search(const Player& player,
                             const Player& opponent,
                             const CardTypeResult& previous,
                             const Deck& deck,
                             int tableScore,
                             const CardTracker* tracker,
                             const Evaluator* evaluator,
                             DeckSide mySide = DeckSide::PlayerA) override;
};

class MinimaxSearcher : public Searcher {
public:
    explicit MinimaxSearcher(int depth = 6, const StateEvaluator* stateEvaluator = nullptr);
    std::vector<Card> search(const Player& player,
                             const Player& opponent,
                             const CardTypeResult& previous,
                             const Deck& deck,
                             int tableScore,
                             const CardTracker* tracker,
                             const Evaluator* evaluator,
                             DeckSide mySide = DeckSide::PlayerA) override;
private:
    int depth_;
    const StateEvaluator* stateEvaluator_;
};