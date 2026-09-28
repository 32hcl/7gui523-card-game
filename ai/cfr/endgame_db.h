#pragma once

#include "cfr_types.h"
#include <unordered_map>
#include <vector>
#include <string>

struct EndgameEntry {
    std::vector<double> strategy;
    std::vector<double> counterfactualValues;
};

class EndgameDB {
public:
    struct CompleteState {
        std::vector<std::string> p0Hand;
        std::vector<std::string> p1Hand;
        int tableScore;
        LastPlayEncoding lastPlay;
        uint8_t historyHash;
        int currentPlayer;
    };

    void build(const CFRParams& params);

    bool lookup(const InfoSetKey& key, std::vector<double>& outStrategy) const;

    bool save(const std::string& filepath) const;
    bool load(const std::string& filepath);

    size_t size() const { return table_.size(); }

private:
    struct DCFRSubgameNode {
        std::vector<double> regretSum;
        std::vector<double> strategySum;
    };

    std::vector<CompleteState> enumerateStates();
    InfoSetKey makeKey(const CompleteState& s, int player) const;
    std::vector<int> legalActions(const CompleteState& s) const;
    CompleteState applyAction(const CompleteState& s, int action, int player) const;
    double dcfrSolve(const InfoSetKey& rootKey, const CFRParams& params);

    std::unordered_map<InfoSetKey, EndgameEntry, InfoSetKeyHash> table_;
};

struct ActionDef {
    std::vector<std::string> cards;
    int id;
};

extern const std::vector<ActionDef> kEndgameActionTable;

void buildEndgameActionTable();

void collectEndgameActions(const std::vector<std::string>& hand,
                           const LastPlayEncoding& lastPlay,
                           std::vector<std::vector<std::string>>& outActions);