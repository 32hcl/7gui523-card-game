#include "endgame_db.h"
#include "hand_encoder.h"
#include "terminal.h"
#include "infoset.h"
#include "core/card/cardtype.h"
#include "core/rule/score.h"
#include <algorithm>
#include <fstream>
#include <functional>
#include <unordered_set>
#include <random>
#include <cmath>
#include <cstring>

LastPlayEncoding decodeLastPlayFromState(const EndgameDB::CompleteState& s) {
    return s.lastPlay;
}

EndgameDB::CompleteState resolvePassEndgame(const EndgameDB::CompleteState& s) {
    EndgameDB::CompleteState next = s;
    if (s.p0Hand.empty() && s.p1Hand.empty()) {
        return next;
    }
    int winner = 1 - s.currentPlayer;
    next.lastPlay = emptyLastPlay();
    next.tableScore = 0;
    next.currentPlayer = winner;
    return next;
}

EndgameDB::CompleteState resolvePlayEndgame(const EndgameDB::CompleteState& s,
                                              const std::vector<std::string>& played) {
    EndgameDB::CompleteState next = s;
    int cp = s.currentPlayer;

    std::vector<std::string> remaining;
    auto& hand = (cp == 0) ? s.p0Hand : s.p1Hand;
    std::unordered_multiset<std::string> handSet(hand.begin(), hand.end());

    for (const auto& pt : played) {
        auto it = handSet.find(pt);
        if (it != handSet.end()) handSet.erase(it);
    }
    for (const auto& pt : handSet) remaining.push_back(pt);

    int addScore = 0;
    for (const auto& pt : played) {
        if (pt == "5") addScore += 5;
        else if (pt == "10") addScore += 10;
        else if (pt == "K") addScore += 20;
    }

    std::vector<Card> playedCards;
    for (const auto& pt : played) playedCards.push_back({pt, "", 0, 0});
    CardTypeResult parsed = parseCardType(playedCards);

    int bonus = 0;
    if (!s.lastPlay.empty()) {
        CardTypeResult prev;
        switch (s.lastPlay.typeId) {
            case 0: prev.type = CardType::Single; break;
            case 1: prev.type = CardType::Pair; break;
            case 2: prev.type = CardType::Triple; break;
            case 3: prev.type = CardType::TripleWithOne; break;
            case 4: prev.type = CardType::TripleWithTwo; break;
            case 5: prev.type = CardType::Bomb; break;
            case 6: prev.type = CardType::Rocket; break;
            case 7: prev.type = CardType::Special523; break;
            default: prev.type = CardType::Invalid; break;
        }
        prev.keyPoint = indexToPoint(s.lastPlay.keyPointIdx);
        bonus = calculatePressureBonus(parsed, prev);
    }
    next.tableScore += addScore + bonus;
    next.lastPlay = makeLastPlayEncoding(parsed);

    if (cp == 0) {
        next.p0Hand = remaining;
        if (remaining.empty() && !s.p1Hand.empty()) next.currentPlayer = 1;
        else if (remaining.empty()) next.currentPlayer = 0;
        else next.currentPlayer = 1;
    } else {
        next.p1Hand = remaining;
        if (remaining.empty() && !s.p0Hand.empty()) next.currentPlayer = 0;
        else if (remaining.empty()) next.currentPlayer = 1;
        else next.currentPlayer = 0;
    }
    return next;
}

void collectEndgameActions(
    const std::vector<std::string>& hand,
    const LastPlayEncoding& lastPlay,
    std::vector<std::vector<std::string>>& outActions)
{
    int n = (int)hand.size();

    if (!lastPlay.empty()) {
        outActions.push_back({});
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

        if (!lastPlay.empty()) {
            CardTypeResult prev;
            switch (lastPlay.typeId) {
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
            prev.keyPoint = indexToPoint(lastPlay.keyPointIdx);
            if (!canBeat(parsed, prev)) continue;
        }

        outActions.push_back(subset);
    }
}

static double terminalEndgameValue(const EndgameDB::CompleteState& s,
                                    int myCollected, int oppCollected, int myPerspective) {
    int myScore = myCollected;
    int oppScore = oppCollected;

    int myHandSize = (myPerspective == 0) ? (int)s.p0Hand.size() : (int)s.p1Hand.size();
    int oppHandSize = (myPerspective == 0) ? (int)s.p1Hand.size() : (int)s.p0Hand.size();

    auto& myHand = (myPerspective == 0) ? s.p0Hand : s.p1Hand;
    auto& oppHand = (myPerspective == 0) ? s.p1Hand : s.p0Hand;

    if (myHand.empty() && oppHand.empty()) {
        int finisherScore = myCollected + s.tableScore;
        int oppExtra = 0;
        for (const auto& pt : oppHand) {
            if (pt == "5") oppExtra += 5;
            else if (pt == "10") oppExtra += 10;
            else if (pt == "K") oppExtra += 20;
        }
        oppExtra += oppHandSize * 3;
        double util = (double)(finisherScore + oppExtra - oppCollected) / 200.0;
        return (myPerspective == 0) ? util : -util;
    }

    if (myHand.empty()) {
        int extra = 0;
        for (const auto& pt : oppHand) {
            if (pt == "5") extra += 5;
            else if (pt == "10") extra += 10;
            else if (pt == "K") extra += 20;
        }
        extra += oppHandSize * 3;
        myScore = myCollected + s.tableScore + extra;
        oppScore = oppCollected;
    } else if (oppHand.empty()) {
        int extra = 0;
        for (const auto& pt : myHand) {
            if (pt == "5") extra += 5;
            else if (pt == "10") extra += 10;
            else if (pt == "K") extra += 20;
        }
        extra += myHandSize * 3;
        oppScore = oppCollected + s.tableScore + extra;
        myScore = myCollected;
    }

    double util = (double)(myScore - oppScore) / 200.0;
    return (myPerspective == 0) ? util : -util;
}

void EndgameDB::build(const CFRParams& params) {
    auto states = enumerateStates();
    if (states.empty()) return;

    std::unordered_map<InfoSetKey, int, InfoSetKeyHash> actionCount;

    for (const auto& s : states) {
        for (int p = 0; p < 2; ++p) {
            InfoSetKey key = makeKey(s, p);
            auto pts = decodeHand(key.myHandCode);
            std::vector<std::vector<std::string>> acts;
            collectEndgameActions(pts, key.lastPlay, acts);
            actionCount[key] = (int)acts.size();
        }
    }

    std::unordered_map<InfoSetKey, DCFRSubgameNode, InfoSetKeyHash> dcfrNodes;
    for (const auto& kv : actionCount) {
        DCFRSubgameNode node;
        int na = kv.second;
        if (na == 0) na = 1;
        node.regretSum.assign(na, 0.0);
        node.strategySum.assign(na, 0.0);
        dcfrNodes[kv.first] = node;
    }

    std::mt19937 rng(42);

    for (int iter = 0; iter < params.iterations; ++iter) {
        int si = std::uniform_int_distribution<int>(0, (int)states.size() - 1)(rng);
        CompleteState root = states[si];

        std::unordered_map<InfoSetKey, double, InfoSetKeyHash> nodeValues;

        std::function<double(const CompleteState&, int, double, double)> traverse;
        traverse = [&](const CompleteState& s, int depth, double p0, double p1) -> double {
            if (depth > 20) return 0.0;

            if (s.p0Hand.empty() && s.p1Hand.empty()) {
                return 0.0;
            }

            int cp = s.currentPlayer;
            InfoSetKey ikey = makeKey(s, cp);
            auto pts = decodeHand(ikey.myHandCode);

            std::vector<std::vector<std::string>> actions;
            collectEndgameActions(pts, ikey.lastPlay, actions);
            if (actions.empty()) return 0.0;

            int numA = (int)actions.size();
            auto it = dcfrNodes.find(ikey);
            if (it == dcfrNodes.end()) return 0.0;

            auto& node = it->second;
            if ((int)node.regretSum.size() != numA) {
                node.regretSum.assign(numA, 0.0);
                node.strategySum.assign(numA, 0.0);
            }

            double posSum = 0.0;
            for (int i = 0; i < numA; ++i)
                if (node.regretSum[i] > 0) posSum += node.regretSum[i];

            std::vector<double> strategy(numA, 0.0);
            if (posSum <= 0.0) {
                double u = 1.0 / numA;
                for (int i = 0; i < numA; ++i) strategy[i] = u;
            } else {
                for (int i = 0; i < numA; ++i)
                    strategy[i] = node.regretSum[i] / posSum;
            }

            double iterWeight = (double)(iter + 1);
            for (int i = 0; i < numA; ++i)
                node.strategySum[i] += iterWeight * strategy[i];

            std::vector<double> actionValues(numA, 0.0);
            double nodeValue = 0.0;

            for (int i = 0; i < numA; ++i) {
                CompleteState next;
                if (actions[i].empty()) {
                    next = resolvePassEndgame(s);
                } else {
                    next = resolvePlayEndgame(s, actions[i]);
                }

                double childVal;
                bool singleEmpty = (next.p0Hand.empty() != next.p1Hand.empty());
                bool bothEmpty = next.p0Hand.empty() && next.p1Hand.empty();
                if (bothEmpty || singleEmpty) {
                    childVal = terminalEndgameValue(next, 0, 0, cp);
                } else {
                    childVal = traverse(next, depth + 1, p0, p1);
                }

                actionValues[i] = childVal;
                nodeValue += strategy[i] * childVal;
            }

            nodeValues[ikey] = nodeValue;

            for (int i = 0; i < numA; ++i) {
                double regret = actionValues[i] - nodeValue;
                double alpha = (regret > 0) ? params.alpha : params.beta;
                if (iter == 0) node.regretSum[i] = regret;
                else node.regretSum[i] = node.regretSum[i] * params.gamma + alpha * regret;
            }

            return nodeValue;
        };

        traverse(root, 0, 1.0, 1.0);
    }

    for (auto& kv : dcfrNodes) {
        const InfoSetKey& key = kv.first;
        auto& node = kv.second;
        int numA = (int)node.regretSum.size();
        if (numA == 0) continue;

        double sum = 0.0;
        for (double s : node.strategySum) sum += s;

        EndgameEntry entry;
        entry.strategy.resize(numA);
        if (sum > 0.0) {
            for (int i = 0; i < numA; ++i)
                entry.strategy[i] = node.strategySum[i] / sum;
        } else {
            double u = 1.0 / numA;
            for (int i = 0; i < numA; ++i)
                entry.strategy[i] = u;
        }
        table_[key] = entry;
    }
}

std::vector<EndgameDB::CompleteState> EndgameDB::enumerateStates() {
    std::vector<std::pair<uint16_t, std::vector<std::string>>> smallHands;
    for (uint16_t code = 0; code < 15504; ++code) {
        auto h = decodeHand(code);
        if ((int)h.size() <= 2) {
            smallHands.push_back({code, std::move(h)});
        }
    }

    struct StateKey {
        uint16_t p0Code, p1Code;
        int8_t cp, lpType, lpKey;
        uint16_t tableScore;
        bool operator==(const StateKey& o) const {
            return p0Code == o.p0Code && p1Code == o.p1Code &&
                   cp == o.cp && lpType == o.lpType &&
                   lpKey == o.lpKey && tableScore == o.tableScore;
        }
    };
    struct StateKeyHash {
        size_t operator()(const StateKey& k) const {
            size_t h = (size_t)k.p0Code;
            h = (h << 16) ^ (size_t)k.p1Code;
            h = (h << 16) ^ (size_t)(uint8_t)k.cp;
            h = (h << 16) ^ (size_t)(uint8_t)k.lpType;
            h = (h << 16) ^ (size_t)(uint8_t)k.lpKey;
            h = (h << 16) ^ (size_t)k.tableScore;
            return h;
        }
    };

    std::unordered_set<StateKey, StateKeyHash> visited;
    std::vector<CompleteState> result;

    for (const auto& p0 : smallHands) {
        for (const auto& p1 : smallHands) {
            CompleteState s{p0.second, p1.second, 0, emptyLastPlay(), 0, 0};
            StateKey sk{p0.first, p1.first, 0, -1, -1, 0};
            visited.insert(sk);
            result.push_back(std::move(s));
        }
    }

    for (size_t idx = 0; idx < result.size(); ++idx) {
        CompleteState s = result[idx];
        int cp = s.currentPlayer;
        auto& hand = (cp == 0) ? s.p0Hand : s.p1Hand;

        std::vector<std::vector<std::string>> actions;
        collectEndgameActions(hand, s.lastPlay, actions);

        for (const auto& act : actions) {
            CompleteState next;
            if (act.empty()) {
                next = resolvePassEndgame(s);
            } else {
                next = resolvePlayEndgame(s, act);
            }

            bool anyEmpty = next.p0Hand.empty() || next.p1Hand.empty();
            if (anyEmpty && next.p0Hand.empty() && next.p1Hand.empty()) continue;
            if (anyEmpty) continue;

            uint16_t np0 = encodeHand(next.p0Hand);
            uint16_t np1 = encodeHand(next.p1Hand);
            StateKey nk{
                np0, np1,
                (int8_t)next.currentPlayer,
                (int8_t)next.lastPlay.typeId,
                (int8_t)next.lastPlay.keyPointIdx,
                (uint16_t)next.tableScore
            };

            if (visited.insert(nk).second) {
                next.historyHash = 0;
                result.push_back(std::move(next));
            }
        }
    }

    return result;
}

InfoSetKey EndgameDB::makeKey(const CompleteState& s, int player) const {
    auto myPoints = (player == 0) ? s.p0Hand : s.p1Hand;
    int oppSize = (player == 0) ? (int)s.p1Hand.size() : (int)s.p0Hand.size();
    return buildInfoSetKey(myPoints, oppSize, 0, 0, s.tableScore, s.lastPlay, s.historyHash);
}

bool EndgameDB::lookup(const InfoSetKey& key, std::vector<double>& outStrategy) const {
    auto it = table_.find(key);
    if (it == table_.end()) return false;
    outStrategy = it->second.strategy;
    return true;
}

bool EndgameDB::save(const std::string& filepath) const {
    std::ofstream ofs(filepath, std::ios::binary);
    if (!ofs) return false;

    size_t sz = table_.size();
    ofs.write((const char*)&sz, sizeof(sz));

    for (const auto& kv : table_) {
        const InfoSetKey& key = kv.first;
        const EndgameEntry& entry = kv.second;

        ofs.write((const char*)&key, sizeof(key));

        size_t stratSize = entry.strategy.size();
        ofs.write((const char*)&stratSize, sizeof(stratSize));
        ofs.write((const char*)entry.strategy.data(), stratSize * sizeof(double));
    }
    return true;
}

bool EndgameDB::load(const std::string& filepath) {
    std::ifstream ifs(filepath, std::ios::binary);
    if (!ifs) return false;

    table_.clear();
    size_t sz = 0;
    ifs.read((char*)&sz, sizeof(sz));

    for (size_t i = 0; i < sz; ++i) {
        InfoSetKey key;
        ifs.read((char*)&key, sizeof(key));

        size_t stratSize = 0;
        ifs.read((char*)&stratSize, sizeof(stratSize));

        EndgameEntry entry;
        entry.strategy.resize(stratSize);
        ifs.read((char*)entry.strategy.data(), stratSize * sizeof(double));
        table_[key] = entry;
    }
    return true;
}