#include "cfr_trainer.h"
#include "hand_encoder.h"
#include "infoset.h"
#include "terminal.h"
#include "core/card/cardtype.h"
#include "core/rule/score.h"
#include "core/card/deck.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <numeric>

CFRTrainer::CFRTrainer(const CFRParams& params, const EndgameDB* endgameDB)
    : params_(params), endgameDB_(endgameDB), iterCount_(0),
      totalPositiveRegret_(0.0), regretNodeCount_(0)
{
    stats_ = {};
}

CFREndStatus CFRTrainer::checkEnd(const CFRTrainingState& state) const {
    if (state.specialWin) {
        return (state.currentPlayer == 0) ? CFREndStatus::Terminal_SpecialWin
                                           : CFREndStatus::Terminal_SpecialLose;
    }
    if (state.myHandPoints.empty() && state.myDeck.empty()) {
        return CFREndStatus::Terminal_Normal;
    }
    if (state.oppHandPoints.empty() && state.oppDeck.empty()) {
        return CFREndStatus::Terminal_Normal;
    }

    if (endgameDB_) {
        auto myPts = state.currentPlayer == 0 ? state.myHandPoints : state.oppHandPoints;
        int oppSz = state.currentPlayer == 0 ? (int)state.oppHandPoints.size()
                                              : (int)state.myHandPoints.size();
        int myDeck = state.currentPlayer == 0 ? state.myDeckCount() : state.oppDeckCount();
        int oppDeck = state.currentPlayer == 0 ? state.oppDeckCount() : state.myDeckCount();

        InfoSetKey key = buildInfoSetKey(
            myPts, oppSz, myDeck, oppDeck,
            state.tableScore + state.tableBonus,
            state.lastPlay, 0
        );

        if (isEndgameBoundary(key)) {
            return CFREndStatus::EndgameLookup;
        }
    }

    return CFREndStatus::Running;
}

std::vector<CFRAction> CFRTrainer::getLegalActions(const CFRTrainingState& state, int player) const {
    std::vector<CFRAction> actions;
    auto& hand = (player == 0) ? state.myHandPoints : state.oppHandPoints;
    int n = (int)hand.size();

    if (!state.lastPlay.empty()) {
        actions.push_back({{}, -1});
    }

    for (int mask = 1; mask < (1 << n); ++mask) {
        std::vector<std::string> subset;
        for (int b = 0; b < n; ++b) {
            if (mask & (1 << b)) subset.push_back(hand[b]);
        }

        std::vector<Card> cards;
        for (const auto& pt : subset) cards.push_back({pt, "", 0, 0});
        CardTypeResult parsed = parseCardType(cards);
        if (parsed.type == CardType::Invalid) continue;

        if (!state.lastPlay.empty()) {
            CardTypeResult prev;
            switch (state.lastPlay.typeId) {
                case 0: prev.type = CardType::Single; break;
                case 1: prev.type = CardType::Pair; break;
                case 2: prev.type = CardType::Triple; break;
                case 3: prev.type = CardType::TripleWithOne; break;
                case 4: prev.type = CardType::TripleWithTwo; break;
                case 5: prev.type = CardType::Bomb; break;
                case 6: prev.type = CardType::Rocket; break;
                case 7: prev.type = CardType::Special523; break;
                default: continue;
            }
            prev.keyPoint = indexToPoint(state.lastPlay.keyPointIdx);
            if (!canBeat(parsed, prev)) continue;
        }

        int actionId = encodeHand(subset);
        actions.push_back({subset, actionId});
    }

    return actions;
}

CFRTrainingState CFRTrainer::applyAction(const CFRTrainingState& state,
                                          const CFRAction& action, int player,
                                          std::mt19937& rng) const {
    CFRTrainingState next = state;

    if (action.cards.empty()) {
        int winner = 1 - player;
        int tbl = next.tableScore + next.tableBonus;

        if (winner == 0) next.myCollectedScore += tbl;
        else next.oppCollectedScore += tbl;

        next.tableScore = 0;
        next.tableBonus = 0;
        next.lastPlay = emptyLastPlay();
        next.currentPlayer = winner;
        return next;
    }

    auto& hand = (player == 0) ? next.myHandPoints : next.oppHandPoints;
    std::vector<std::string> remaining;
    for (const auto& pt : hand) {
        bool used = false;
        for (const auto& ap : action.cards) {
            if (pt == ap) { used = true; break; }
        }
        if (!used) remaining.push_back(pt);
    }

    int addScore = 0;
    for (const auto& pt : action.cards) {
        if (pt == "5") addScore += 5;
        else if (pt == "10") addScore += 10;
        else if (pt == "K") addScore += 20;
    }

    std::vector<Card> pcs;
    for (const auto& pt : action.cards) pcs.push_back({pt, "", 0, 0});
    CardTypeResult parsed = parseCardType(pcs);

    if (parsed.type == CardType::Special523) {
        next.specialWin = true;
        if (player == 0) {
            next.myCollectedScore += next.tableScore + next.tableBonus + addScore;
        } else {
            next.oppCollectedScore += next.tableScore + next.tableBonus + addScore;
        }
        next.tableScore = 0;
        next.tableBonus = 0;
        return next;
    }

    int bonus = 0;
    if (!next.lastPlay.empty()) {
        CardTypeResult prev;
        switch (next.lastPlay.typeId) {
            case 0: prev.type = CardType::Single; break;
            case 1: prev.type = CardType::Pair; break;
            case 2: prev.type = CardType::Triple; break;
            case 3: prev.type = CardType::TripleWithOne; break;
            case 4: prev.type = CardType::TripleWithTwo; break;
            case 5: prev.type = CardType::Bomb; break;
            case 6: prev.type = CardType::Rocket; break;
            case 7: prev.type = CardType::Special523; break;
            default: break;
        }
        prev.keyPoint = indexToPoint(next.lastPlay.keyPointIdx);
        bonus = calculatePressureBonus(parsed, prev);
    }

    next.tableScore += addScore;
    next.tableBonus += bonus;
    next.lastPlay = makeLastPlayEncoding(parsed);

    if (player == 0) {
        next.myHandPoints = remaining;
    } else {
        next.oppHandPoints = remaining;
    }

    bool handEmptied = remaining.empty();

    if (handEmptied) {
        if (player == 0 && !next.myDeck.empty()) {
            int drawCount = std::min(5, (int)next.myDeck.size());
            for (int i = 0; i < drawCount; ++i) {
                next.myHandPoints.push_back(next.myDeck.back());
                next.myDeck.pop_back();
            }
        } else if (player == 1 && !next.oppDeck.empty()) {
            int drawCount = std::min(5, (int)next.oppDeck.size());
            for (int i = 0; i < drawCount; ++i) {
                next.oppHandPoints.push_back(next.oppDeck.back());
                next.oppDeck.pop_back();
            }
        }
    }

    if (!next.specialWin) {
        next.currentPlayer = 1 - player;
    }

    return next;
}

std::vector<double> CFRTrainer::regretMatching(
    const std::vector<double>& regretSum,
    const std::vector<int>& actionIds,
    double posRegretWeight) const
{
    int n = (int)regretSum.size();
    std::vector<double> strategy(n, 0.0);
    if (n == 0) return strategy;

    double posSum = 0.0;
    for (int i = 0; i < n; ++i) {
        if (regretSum[i] > 0) posSum += regretSum[i];
    }

    if (posSum <= 0.0) {
        double u = 1.0 / n;
        for (int i = 0; i < n; ++i) strategy[i] = u;
    } else {
        for (int i = 0; i < n; ++i) {
            strategy[i] = (regretSum[i] > 0) ? regretSum[i] / posSum : 0.0;
        }
    }
    return strategy;
}

double CFRTrainer::p0Terminal(const CFRTrainingState& state) const {
    if (state.myHandPoints.empty() && state.myDeck.empty()) {
        return computeTerminalUtility(
            {}, {}, state.myCollectedScore, state.oppCollectedScore,
            0, state.oppDeckCount() + (int)state.oppHandPoints.size(),
            state.tableScore + state.tableBonus, false, true);
    }
    return computeTerminalUtility(
        {}, {}, state.myCollectedScore, state.oppCollectedScore,
        state.myDeckCount() + (int)state.myHandPoints.size(), 0,
        state.tableScore + state.tableBonus, false, true);
}

double CFRTrainer::cfrTraverse(CFRTrainingState& state,
                                double p0Reach, double p1Reach,
                                int iter, std::mt19937& rng) {
    CFREndStatus status = checkEnd(state);
    if (status == CFREndStatus::Terminal_Normal) {
        return p0Terminal(state);
    }
    if (status == CFREndStatus::Terminal_SpecialWin) return 1.0;
    if (status == CFREndStatus::Terminal_SpecialLose) return -1.0;

    int cp = state.currentPlayer;
    auto actions = getLegalActions(state, cp);
    if (actions.empty()) return 0.0;

    auto myPts = (cp == 0) ? state.myHandPoints : state.oppHandPoints;
    int oppSz = (cp == 0) ? (int)state.oppHandPoints.size()
                           : (int)state.myHandPoints.size();
    int myDeck = (cp == 0) ? state.myDeckCount() : state.oppDeckCount();
    int oppDeck = (cp == 0) ? state.oppDeckCount() : state.myDeckCount();

    InfoSetKey key = buildInfoSetKey(
        myPts, oppSz, myDeck, oppDeck,
        state.tableScore + state.tableBonus,
        state.lastPlay, 0);

    if (status == CFREndStatus::EndgameLookup && endgameDB_) {
        std::vector<double> strat;
        if (endgameDB_->lookup(key, strat)) {
            double nodeVal = 0.0;
            for (size_t i = 0; i < actions.size() && i < strat.size(); ++i) {
                auto next = applyAction(state, actions[i], cp, rng);
                nodeVal += strat[i] * cfrTraverse(next, p0Reach, p1Reach, iter, rng);
            }
            return nodeVal;
        }
    }

    auto& node = nodes_[key];
    if ((int)node.regretSum.size() != (int)actions.size()) {
        node.regretSum.assign(actions.size(), 0.0);
        node.strategySum.assign(actions.size(), 0.0);
    }

    int numA = (int)actions.size();
    std::vector<int> actionIds(numA);
    for (int i = 0; i < numA; ++i) actionIds[i] = actions[i].actionId;

    auto strategy = regretMatching(node.regretSum, actionIds, 1.0);

    for (int i = 0; i < numA; ++i) {
        node.strategySum[i] += strategy[i];
    }

    std::vector<double> actionValues(numA, 0.0);
    double nodeValue = 0.0;

    for (int i = 0; i < numA; ++i) {
        auto next = applyAction(state, actions[i], cp, rng);
        actionValues[i] = cfrTraverse(next, p0Reach, p1Reach, iter, rng);
        nodeValue += strategy[i] * actionValues[i];
    }

    for (int i = 0; i < numA; ++i) {
        double regret = actionValues[i] - nodeValue;
        node.regretSum[i] += regret;
    }

    return nodeValue;
}

double CFRTrainer::dcfrTraverse(CFRTrainingState& state,
                                 double p0Reach, double p1Reach,
                                 int iter, std::mt19937& rng) {
    CFREndStatus status = checkEnd(state);
    if (status == CFREndStatus::Terminal_Normal) {
        return p0Terminal(state);
    }
    if (status == CFREndStatus::Terminal_SpecialWin) return 1.0;
    if (status == CFREndStatus::Terminal_SpecialLose) return -1.0;

    int cp = state.currentPlayer;
    auto actions = getLegalActions(state, cp);
    if (actions.empty()) return 0.0;

    auto myPts = (cp == 0) ? state.myHandPoints : state.oppHandPoints;
    int oppSz = (cp == 0) ? (int)state.oppHandPoints.size()
                           : (int)state.myHandPoints.size();
    int myDeck = (cp == 0) ? state.myDeckCount() : state.oppDeckCount();
    int oppDeck = (cp == 0) ? state.oppDeckCount() : state.myDeckCount();

    InfoSetKey key = buildInfoSetKey(
        myPts, oppSz, myDeck, oppDeck,
        state.tableScore + state.tableBonus,
        state.lastPlay, 0);

    if (status == CFREndStatus::EndgameLookup && endgameDB_) {
        std::vector<double> strat;
        if (endgameDB_->lookup(key, strat)) {
            double nodeVal = 0.0;
            for (size_t i = 0; i < actions.size() && i < strat.size(); ++i) {
                auto next = applyAction(state, actions[i], cp, rng);
                nodeVal += strat[i] * dcfrTraverse(next, p0Reach, p1Reach, iter, rng);
            }
            return nodeVal;
        }
    }

    auto& node = nodes_[key];
    if ((int)node.regretSum.size() != (int)actions.size()) {
        node.regretSum.assign(actions.size(), 0.0);
        node.strategySum.assign(actions.size(), 0.0);
    }

    int numA = (int)actions.size();
    std::vector<int> actionIds(numA);
    for (int i = 0; i < numA; ++i) actionIds[i] = actions[i].actionId;

    auto strategy = regretMatching(node.regretSum, actionIds, 1.0);

    double iterWeight = (double)(iter + 1);
    for (int i = 0; i < numA; ++i) {
        node.strategySum[i] += iterWeight * strategy[i];
    }

    std::vector<double> actionValues(numA, 0.0);
    double nodeValue = 0.0;

    for (int i = 0; i < numA; ++i) {
        auto next = applyAction(state, actions[i], cp, rng);
        actionValues[i] = dcfrTraverse(next, p0Reach, p1Reach, iter, rng);
        nodeValue += strategy[i] * actionValues[i];
    }

    for (int i = 0; i < numA; ++i) {
        double regret = actionValues[i] - nodeValue;
        double alpha = (regret > 0) ? params_.alpha : params_.beta;
        if (iter == 0) {
            node.regretSum[i] = regret;
        } else {
            node.regretSum[i] = node.regretSum[i] * params_.gamma + alpha * regret;
        }
    }

    return nodeValue;
}

double CFRTrainer::mccfrTraverse(CFRTrainingState& state, int traverser,
                                  int iter, double reachProb,
                                  std::mt19937& rng) {
    CFREndStatus status = checkEnd(state);

    if (status == CFREndStatus::Terminal_Normal) {
        double p0v = p0Terminal(state);
        return (traverser == 0) ? p0v : -p0v;
    }
    if (status == CFREndStatus::Terminal_SpecialWin) {
        return (state.currentPlayer == traverser) ? 1.0 : -1.0;
    }
    if (status == CFREndStatus::Terminal_SpecialLose) {
        return (state.currentPlayer == traverser) ? -1.0 : 1.0;
    }

    int cp = state.currentPlayer;
    auto actions = getLegalActions(state, cp);
    if (actions.empty()) return 0.0;

    auto myPts = (cp == 0) ? state.myHandPoints : state.oppHandPoints;
    int oppSz = (cp == 0) ? (int)state.oppHandPoints.size()
                           : (int)state.myHandPoints.size();
    int myDeck = (cp == 0) ? state.myDeckCount() : state.oppDeckCount();
    int oppDeck = (cp == 0) ? state.oppDeckCount() : state.myDeckCount();

    InfoSetKey key = buildInfoSetKey(
        myPts, oppSz, myDeck, oppDeck,
        state.tableScore + state.tableBonus,
        state.lastPlay, 0);

    const double kExploreEps = 0.05;

    if (status == CFREndStatus::EndgameLookup && endgameDB_) {
        std::vector<double> strat;
        if (endgameDB_->lookup(key, strat)) {
            int numAeg = (int)actions.size();
            std::vector<double> exploreStrat((int)strat.size(), 0.0);
            double sum = 0.0;
            for (double s : strat) sum += s;
            for (size_t i = 0; i < strat.size(); ++i) {
                double s = (sum > 0) ? strat[i] / sum : 1.0 / numAeg;
                exploreStrat[i] = (1.0 - kExploreEps) * s + kExploreEps / numAeg;
            }
            std::discrete_distribution<int> dist(exploreStrat.begin(), exploreStrat.end());
            int idx = dist(rng);
            auto next = applyAction(state, actions[idx], cp, rng);
            double tail = mccfrTraverse(next, traverser, iter,
                                         reachProb * exploreStrat[idx], rng);
            return tail;
        }
    }

    auto& node = nodes_[key];
    int numA = (int)actions.size();
    if ((int)node.regretSum.size() != numA) {
        node.regretSum.assign(numA, 0.0);
        node.strategySum.assign(numA, 0.0);
    }

    std::vector<int> actionIds(numA);
    for (int i = 0; i < numA; ++i) actionIds[i] = actions[i].actionId;

    auto rmStrat = regretMatching(node.regretSum, actionIds, 1.0);

    std::vector<double> exploreStrat(numA);
    double uniform = 1.0 / numA;
    for (int i = 0; i < numA; ++i)
        exploreStrat[i] = (1.0 - kExploreEps) * rmStrat[i] + kExploreEps * uniform;

    double iterWeight = (double)(iter + 1);
    for (int i = 0; i < numA; ++i)
        node.strategySum[i] += iterWeight * rmStrat[i];

    std::discrete_distribution<int> dist(exploreStrat.begin(), exploreStrat.end());
    int a = dist(rng);

    auto next = applyAction(state, actions[a], cp, rng);
    double tail = mccfrTraverse(next, traverser, iter,
                                 reachProb * exploreStrat[a], rng);

    if (cp == traverser) {
        double invRp = (reachProb > 1e-12) ? 1.0 / reachProb : 1.0 / 1e-12;
        double delta = tail - node.baseline;
        double W = delta * invRp;

        for (int i = 0; i < numA; ++i) {
            double regret = (i == a)
                ? W * (1.0 - exploreStrat[i]) / (exploreStrat[i] + 1e-9)
                : -W;
            double alpha = (regret > 0) ? params_.alpha : params_.beta;
            if (iter == 0) {
                node.regretSum[i] = regret;
            } else {
                node.regretSum[i] = node.regretSum[i] * params_.gamma + alpha * regret;
            }
        }

        const double kBaselineAlpha = 0.1;
        node.baseline += kBaselineAlpha * delta;
    }

    node.visitCount++;

    return tail;
}

void CFRTrainer::train(int iterations, std::mt19937& rng) {
    for (int iter = 0; iter < iterations; ++iter) {
        CFRTrainingState state;

        std::vector<std::string> allPoints = {
            "A","2","3","4","5","6","7","8","9","10","J","Q","K",
            "A","2","3","4","5","6","7","8","9","10","J","Q","K",
            "A","2","3","4","5","6","7","8","9","10","J","Q","K",
            "A","2","3","4","5","6","7","8","9","10","J","Q","K",
            "大鬼","小鬼"
        };
        std::shuffle(allPoints.begin(), allPoints.end(), rng);

        state.myDeck.assign(allPoints.begin(), allPoints.begin() + 27);
        state.oppDeck.assign(allPoints.begin() + 27, allPoints.end());

        int initDraw = std::min(5, (int)state.myDeck.size());
        for (int i = 0; i < initDraw; ++i) {
            state.myHandPoints.push_back(state.myDeck.back());
            state.myDeck.pop_back();
        }

        int oppDraw = std::min(5, (int)state.oppDeck.size());
        for (int i = 0; i < oppDraw; ++i) {
            state.oppHandPoints.push_back(state.oppDeck.back());
            state.oppDeck.pop_back();
        }

        state.myCollectedScore = 0;
        state.oppCollectedScore = 0;
        state.tableScore = 0;
        state.tableBonus = 0;
        state.lastPlay = emptyLastPlay();
        state.currentPlayer = 0;
        state.specialWin = false;
        state.gameOver = false;

        int traverser = iter % 2;
        mccfrTraverse(state, traverser, iter, 1.0, rng);

        if ((iter + 1) % params_.checkpointInterval == 0) {
            computeStats();
        }

        iterCount_ = iter + 1;
    }
    computeStats();
}

void CFRTrainer::computeStats() {
    int totalNodes = (int)nodes_.size();
    double totalEntropy = 0.0;
    double maxRegret = 0.0;

    for (const auto& kv : nodes_) {
        const auto& node = kv.second;
        int n = (int)node.strategySum.size();
        if (n == 0) continue;

        std::vector<double> strat(n, 0.0);
        double sum = 0.0;
        for (double s : node.strategySum) sum += s;
        if (sum > 0.0) {
            for (int i = 0; i < n; ++i) strat[i] = node.strategySum[i] / sum;
        } else {
            double u = 1.0 / n;
            for (int i = 0; i < n; ++i) strat[i] = u;
        }

        double entropy = 0.0;
        for (double p : strat) {
            if (p > 1e-10) entropy -= p * std::log2(p);
        }
        totalEntropy += entropy;

        for (double r : node.regretSum) {
            if (r > maxRegret) maxRegret = r;
        }
    }

    stats_.infosetCoverage = (double)totalNodes;
    stats_.avgStrategyEntropy = totalNodes > 0 ? totalEntropy / totalNodes : 0.0;
    stats_.avgPositiveRegret = maxRegret;
    stats_.exploitability = stats_.avgPositiveRegret;
    stats_.iteration = iterCount_;
}

bool CFRTrainer::saveCheckpoint(const std::string& filepath) const {
    std::ofstream ofs(filepath, std::ios::binary);
    if (!ofs) return false;

    size_t sz = nodes_.size();
    ofs.write((const char*)&sz, sizeof(sz));
    ofs.write((const char*)&iterCount_, sizeof(iterCount_));

    for (const auto& kv : nodes_) {
        const InfoSetKey& key = kv.first;
        const CFRNode& node = kv.second;

        ofs.write((const char*)&key, sizeof(key));

        size_t rs = node.regretSum.size();
        ofs.write((const char*)&rs, sizeof(rs));
        ofs.write((const char*)node.regretSum.data(), rs * sizeof(double));

        size_t ss = node.strategySum.size();
        ofs.write((const char*)&ss, sizeof(ss));
        ofs.write((const char*)node.strategySum.data(), ss * sizeof(double));

        ofs.write((const char*)&node.baseline, sizeof(node.baseline));
        ofs.write((const char*)&node.visitCount, sizeof(node.visitCount));
    }
    return true;
}

bool CFRTrainer::loadCheckpoint(const std::string& filepath) {
    std::ifstream ifs(filepath, std::ios::binary);
    if (!ifs) return false;

    nodes_.clear();
    size_t sz = 0;
    ifs.read((char*)&sz, sizeof(sz));
    ifs.read((char*)&iterCount_, sizeof(iterCount_));

    for (size_t i = 0; i < sz; ++i) {
        InfoSetKey key;
        ifs.read((char*)&key, sizeof(key));

        size_t rs = 0;
        ifs.read((char*)&rs, sizeof(rs));
        CFRNode node;
        node.regretSum.resize(rs);
        ifs.read((char*)node.regretSum.data(), rs * sizeof(double));

        size_t ss = 0;
        ifs.read((char*)&ss, sizeof(ss));
        node.strategySum.resize(ss);
        ifs.read((char*)node.strategySum.data(), ss * sizeof(double));

        if (ifs.peek() != EOF) {
            ifs.read((char*)&node.baseline, sizeof(node.baseline));
            ifs.read((char*)&node.visitCount, sizeof(node.visitCount));
        }

        nodes_[key] = node;
    }
    return true;
}

void CFRTrainer::exportAvgStrategy(const std::string& filepath) const {
    std::ofstream ofs(filepath, std::ios::binary);
    if (!ofs) return;

    size_t sz = nodes_.size();
    ofs.write((const char*)&sz, sizeof(sz));

    for (const auto& kv : nodes_) {
        const InfoSetKey& key = kv.first;
        const CFRNode& node = kv.second;

        int n = (int)node.strategySum.size();
        std::vector<double> avgStrat(n, 0.0);
        double sum = 0.0;
        for (double s : node.strategySum) sum += s;
        if (sum > 0.0) {
            for (int i = 0; i < n; ++i) avgStrat[i] = node.strategySum[i] / sum;
        } else {
            double u = 1.0 / n;
            for (int i = 0; i < n; ++i) avgStrat[i] = u;
        }

        ofs.write((const char*)&key, sizeof(key));
        size_t ss = avgStrat.size();
        ofs.write((const char*)&ss, sizeof(ss));
        ofs.write((const char*)avgStrat.data(), ss * sizeof(double));
    }
}

std::vector<double> CFRTrainer::queryStrategy(const InfoSetKey& key) const {
    auto it = nodes_.find(key);
    if (it == nodes_.end()) return {};

    const auto& node = it->second;
    int n = (int)node.strategySum.size();
    std::vector<double> result(n, 0.0);
    double sum = 0.0;
    for (double s : node.strategySum) sum += s;
    if (sum > 0.0) {
        for (int i = 0; i < n; ++i) result[i] = node.strategySum[i] / sum;
    } else if (n > 0) {
        double u = 1.0 / n;
        for (int i = 0; i < n; ++i) result[i] = u;
    }
    return result;
}