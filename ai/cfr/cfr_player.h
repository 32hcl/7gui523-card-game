#pragma once

#include "endgame_db.h"
#include "strategy.h"
#include "cfr_types.h"
#include "core/player.h"
#include "core/card/cardtype.h"
#include "core/card/deck.h"
#include "core/tracker/cardtracker.h"
#include <vector>
#include <string>
#include <random>

class CFRPlayer {
public:
    CFRPlayer();

    bool loadDB(const std::string& path);
    bool loadStrategy(const std::string& path);
    size_t strategySize() const { return strategy_.size(); }

    bool isInEndgame(const Player& player, const Player& opponent,
                     const Deck& deck) const;

    std::vector<Card> choosePlay(const Player& player,
                                 const Player& opponent,
                                 const CardTypeResult& lastPlay,
                                 const Deck& deck,
                                 int tableScore);

    EndgameDB& db() { return db_; }
    const EndgameDB& db() const { return db_; }

private:
    std::vector<Card> choosePlayEndgame(const Player& player,
                                        const Player& opponent,
                                        const CardTypeResult& lastPlay,
                                        int tableScore);

    std::vector<Card> choosePlayMidgame(const Player& player,
                                        const Player& opponent,
                                        const CardTypeResult& lastPlay,
                                        const Deck& deck,
                                        int tableScore);

    std::vector<Card> choosePlayHeuristic(const Player& player,
                                          const Player& opponent,
                                          const CardTypeResult& lastPlay,
                                          const Deck& deck,
                                          int tableScore);

    EndgameDB     db_;
    StrategyStore strategy_;
    std::mt19937  rng_;
};