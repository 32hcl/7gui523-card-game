#pragma once

#include "cfr_types.h"
#include "endgame_db.h"
#include "core/card/cardtype.h"
#include <unordered_map>
#include <vector>
#include <string>
#include <random>

struct CFRTrainingState {
    std::vector<std::string> myHandPoints;
    std::vector<std::string> oppHandPoints;
    std::vector<std::string> myDeck;
    std::vector<std::string> oppDeck;
    int myCollectedScore;
    int oppCollectedScore;
    int tableScore;
    int tableBonus;
    LastPlayEncoding lastPlay;
    std::vector<CardTypeResult> history;
    int currentPlayer;
    bool gameOver;
    bool specialWin;

    int myDeckCount() const { return (int)myDeck.size(); }
    int oppDeckCount() const { return (int)oppDeck.size(); }
};

struct TrainingStats {
    double exploitability;
    double avgStrategyEntropy;
    double avgPositiveRegret;
    double infosetCoverage;
    int    iteration;
};

class CFRTrainer {
public:
    CFRTrainer(const CFRParams& params, const EndgameDB* endgameDB = nullptr);

    void train(int iterations, std::mt19937& rng);

    const TrainingStats& stats() const { return stats_; }

    bool saveCheckpoint(const std::string& filepath) const;
    bool loadCheckpoint(const std::string& filepath);

    void exportAvgStrategy(const std::string& filepath) const;

    std::vector<double> queryStrategy(const InfoSetKey& key) const;

    const std::unordered_map<InfoSetKey, CFRNode, InfoSetKeyHash>& nodes() const { return nodes_; }

    double cfrTraverse(CFRTrainingState& state, double p0Reach, double p1Reach, int iter, std::mt19937& rng);
    double dcfrTraverse(CFRTrainingState& state, double p0Reach, double p1Reach, int iter, std::mt19937& rng);
    void computeStats();

private:
    double mccfrTraverse(CFRTrainingState& state, int traverser, int iter,
                          double reachProb, std::mt19937& rng);
    double p0Terminal(const CFRTrainingState& state) const;

    CFREndStatus checkEnd(const CFRTrainingState& state) const;
    std::vector<CFRAction> getLegalActions(const CFRTrainingState& state, int player) const;
    CFRTrainingState applyAction(const CFRTrainingState& state, const CFRAction& action, int player, std::mt19937& rng) const;

    std::vector<double> regretMatching(
        const std::vector<double>& regretSum,
        const std::vector<int>& actionIds,
        double posRegretWeight
    ) const;

    CFRParams params_;
    const EndgameDB* endgameDB_;
    std::unordered_map<InfoSetKey, CFRNode, InfoSetKeyHash> nodes_;
    TrainingStats stats_;
    int iterCount_;
    double totalPositiveRegret_;
    int    regretNodeCount_;
};