#pragma once

#include <cstdint>
#include <string>
#include <vector>

constexpr int kTotalScoreMax    = 140;
constexpr int kUtilityDenom     = 200;
constexpr int kDeckBucketMax    = 15;
constexpr int kDeckBucketCount  = 16;
constexpr int kMaxOppHandSize   = 5;
constexpr int kTableScoreRange  = kTotalScoreMax + 1;

constexpr int kEndgameMaxHandSize = 2;

struct CardTypeResult;

struct LastPlayEncoding {
    int  typeId;
    int  keyPointIdx;

    bool empty() const { return typeId == -1 && keyPointIdx == -1; }
    void clear() { typeId = -1; keyPointIdx = -1; }

    bool operator==(const LastPlayEncoding& o) const {
        return typeId == o.typeId && keyPointIdx == o.keyPointIdx;
    }
    bool operator!=(const LastPlayEncoding& o) const { return !(*this == o); }
};

struct InfoSetKey {
    uint16_t myHandCode;
    int      oppHandSize;
    int      myDeckBucket;
    int      oppDeckBucket;
    int      tableTotalScore;
    LastPlayEncoding lastPlay;
    uint8_t  historyHash;

    bool operator==(const InfoSetKey& o) const {
        return myHandCode == o.myHandCode
            && oppHandSize == o.oppHandSize
            && myDeckBucket == o.myDeckBucket
            && oppDeckBucket == o.oppDeckBucket
            && tableTotalScore == o.tableTotalScore
            && lastPlay == o.lastPlay
            && historyHash == o.historyHash;
    }
};

struct InfoSetKeyHash {
    std::size_t operator()(const InfoSetKey& k) const {
        std::size_t h = 0;
        h ^= std::hash<uint16_t>()(k.myHandCode) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<int>()(k.oppHandSize) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<int>()(k.myDeckBucket) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<int>()(k.oppDeckBucket) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<int>()(k.tableTotalScore) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<int>()(k.lastPlay.typeId * 31 + k.lastPlay.keyPointIdx)
             + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<uint8_t>()(k.historyHash) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};

struct CFRParams {
    int    iterations        = 100000;
    double alpha             = 1.5;
    double beta              = 0.5;
    double gamma             = 0.9;
    double epsilon           = 1e-6;
    int    checkpointInterval = 10000;
};

enum class CFREndStatus {
    Running,
    Terminal_Normal,
    Terminal_SpecialWin,
    Terminal_SpecialLose,
    EndgameLookup
};

struct CFRNode {
    std::vector<double> regretSum;
    std::vector<double> strategySum;
    double              baseline = 0.0;
    int                 visitCount = 0;
};

struct CFRAction {
    std::vector<std::string> cards;
    int actionId;
};