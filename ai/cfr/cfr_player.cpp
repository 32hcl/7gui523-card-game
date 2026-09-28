#include "cfr_player.h"
#include "hand_encoder.h"
#include "infoset.h"
#include "ai/ai.h"
#include "core/rule/score.h"
#include <algorithm>
#include <iostream>

CFRPlayer::CFRPlayer() {
    std::random_device rd;
    rng_.seed(rd());
}

bool CFRPlayer::loadDB(const std::string& path) {
    return db_.load(path);
}

bool CFRPlayer::loadStrategy(const std::string& path) {
    return strategy_.load(path);
}

bool CFRPlayer::isInEndgame(const Player& player, const Player& opponent,
                            const Deck& deck) const {
    if (!deck.cards.empty()) return false;
    if ((int)player.hand.size() > kEndgameMaxHandSize) return false;
    if ((int)opponent.hand.size() > kEndgameMaxHandSize) return false;
    return true;
}

std::vector<Card> CFRPlayer::choosePlay(const Player& player,
                                         const Player& opponent,
                                         const CardTypeResult& lastPlay,
                                         const Deck& deck,
                                         int tableScore) {
    if (isInEndgame(player, opponent, deck)) {
        return choosePlayEndgame(player, opponent, lastPlay, tableScore);
    }
    return choosePlayMidgame(player, opponent, lastPlay, deck, tableScore);
}

std::vector<Card> CFRPlayer::choosePlayEndgame(const Player& player,
                                                const Player& opponent,
                                                const CardTypeResult& lastPlay,
                                                int tableScore) {
    if (db_.size() == 0) return {};

    std::vector<std::string> myHandPoints;
    for (const auto& c : player.hand) myHandPoints.push_back(c.point);

    LastPlayEncoding lpEnc = emptyLastPlay();
    if (!lastPlay.cards.empty()) {
        lpEnc = makeLastPlayEncoding(lastPlay);
    }

    InfoSetKey key = buildInfoSetKey(
        myHandPoints,
        (int)opponent.hand.size(),
        0, 0,
        tableScore,
        lpEnc,
        0
    );

    std::vector<std::vector<std::string>> actions;
    collectEndgameActions(myHandPoints, lpEnc, actions);

    if (actions.empty()) return {};

    std::vector<double> strategy;
    bool found = db_.lookup(key, strategy);

    if (found && (int)strategy.size() == (int)actions.size()) {
        double total = 0.0;
        double maxP = 0.0;
        for (double p : strategy) {
            total += p;
            if (p > maxP) maxP = p;
        }
        if (total > 0.0) {
            double uniformP = 1.0 / (double)strategy.size();
            if (maxP / total < 3.0 * uniformP) {
                return aiChoosePlayAI1(player, lastPlay);
            }

            std::uniform_real_distribution<double> dist(0.0, total);
            double r = dist(rng_);
            double cumulative = 0.0;
            for (int i = 0; i < (int)strategy.size(); ++i) {
                cumulative += strategy[i];
                if (r <= cumulative) {
                    if (actions[i].empty()) return aiChoosePlayAI1(player, lastPlay);

                    std::vector<Card> chosen;
                    for (const auto& pt : actions[i]) {
                        for (const auto& c : player.hand) {
                            if (c.point == pt) {
                                chosen.push_back(c);
                                break;
                            }
                        }
                    }
                    return chosen;
                }
            }
        }
    }

    return aiChoosePlayAI1(player, lastPlay);
}

std::vector<Card> CFRPlayer::choosePlayMidgame(const Player& player,
                                                 const Player& opponent,
                                                 const CardTypeResult& lastPlay,
                                                 const Deck& deck,
                                                 int tableScore) {
    if (strategy_.size() == 0) {
        return choosePlayHeuristic(player, opponent, lastPlay, deck, tableScore);
    }

    std::vector<std::string> myHandPoints;
    for (const auto& c : player.hand) myHandPoints.push_back(c.point);

    int myDeckCount = (int)deck.cards.size() / 2;
    int oppDeckCount = (int)deck.cards.size() - myDeckCount;

    LastPlayEncoding lpEnc = emptyLastPlay();
    if (!lastPlay.cards.empty()) {
        lpEnc = makeLastPlayEncoding(lastPlay);
    }

    InfoSetKey key = buildInfoSetKey(
        myHandPoints,
        (int)opponent.hand.size(),
        myDeckCount,
        oppDeckCount,
        tableScore,
        lpEnc,
        0
    );

    int n = (int)myHandPoints.size();

    std::vector<CFRAction> actions;
    if (!lpEnc.empty()) {
        actions.push_back({{}, -1});
    }

    for (int mask = 1; mask < (1 << n); ++mask) {
        std::vector<std::string> subset;
        for (int b = 0; b < n; ++b) {
            if (mask & (1 << b)) subset.push_back(myHandPoints[b]);
        }

        std::vector<Card> cards;
        for (const auto& pt : subset) cards.push_back({pt, "", 0, 0});
        CardTypeResult parsed = parseCardType(cards);
        if (parsed.type == CardType::Invalid) continue;

        if (!lpEnc.empty()) {
            CardTypeResult prev = lastPlay;
            if (!canBeat(parsed, prev)) continue;
        }

        int actionId = encodeHand(subset);
        actions.push_back({subset, actionId});
    }

    if (actions.empty()) return {};

    auto strat = strategy_.query(key);
    if (strat.empty() || (int)strat.size() != (int)actions.size()) {
        return choosePlayHeuristic(player, opponent, lastPlay, deck, tableScore);
    }

    double total = 0.0;
    double maxP = 0.0;
    for (double p : strat) {
        total += p;
        if (p > maxP) maxP = p;
    }
    if (total <= 0.0) {
        return choosePlayHeuristic(player, opponent, lastPlay, deck, tableScore);
    }

    double uniformP = 1.0 / (double)strat.size();
    if (maxP / total < 2.5 * uniformP) {
        return choosePlayHeuristic(player, opponent, lastPlay, deck, tableScore);
    }

    std::uniform_real_distribution<double> dist(0.0, total);
    double r = dist(rng_);
    double cumulative = 0.0;
    for (int i = 0; i < (int)strat.size(); ++i) {
        cumulative += strat[i];
        if (r <= cumulative) {
            const auto& subset = actions[i].cards;
            if (subset.empty()) return {};

            std::vector<Card> chosen;
            for (const auto& pt : subset) {
                for (const auto& c : player.hand) {
                    if (c.point == pt) {
                        chosen.push_back(c);
                        break;
                    }
                }
            }
            return chosen;
        }
    }

    return {};
}

std::vector<Card> CFRPlayer::choosePlayHeuristic(const Player& player,
                                                   const Player& opponent,
                                                   const CardTypeResult& lastPlay,
                                                   const Deck& deck,
                                                   int tableScore) {
    return aiChoosePlayAI1(player, lastPlay);
}