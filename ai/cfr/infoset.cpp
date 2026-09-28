#include "infoset.h"
#include "hand_encoder.h"

int deckCountToBucket(int count) {
    if (count >= kDeckBucketMax) return kDeckBucketMax;
    return count;
}

int cardTypeToTypeId(CardType type) {
    switch (type) {
        case CardType::Single:        return 0;
        case CardType::Pair:          return 1;
        case CardType::Triple:        return 2;
        case CardType::TripleWithOne: return 3;
        case CardType::TripleWithTwo: return 4;
        case CardType::Bomb:          return 5;
        case CardType::Rocket:        return 6;
        case CardType::Special523:    return 7;
        default:                      return -1;
    }
}

int pointToKeyPointIdx(const std::string& point) {
    if (point.empty())    return -1;
    if (point == "王炸")    return 16;
    if (point == "Special523") return 17;
    static const char* tbl[] = {
        "4","6","8","9","10","J","Q","K","A","3","2","5","小鬼","大鬼","7"
    };
    for (int i = 0; i < 15; ++i) {
        if (point == tbl[i]) return i;
    }
    return -1;
}

InfoSetKey buildInfoSetKey(
    const std::vector<std::string>& myHandPoints,
    int oppHandSize,
    int myDeckCount,
    int oppDeckCount,
    int tableTotalScore,
    const LastPlayEncoding& lastPlay,
    uint8_t historyHash)
{
    InfoSetKey key;
    key.myHandCode    = encodeHand(myHandPoints);
    key.oppHandSize   = oppHandSize;
    key.myDeckBucket  = deckCountToBucket(myDeckCount);
    key.oppDeckBucket = deckCountToBucket(oppDeckCount);
    key.tableTotalScore = tableTotalScore;
    key.lastPlay      = lastPlay;
    key.historyHash   = historyHash;
    return key;
}

LastPlayEncoding makeLastPlayEncoding(const CardTypeResult& parsed) {
    LastPlayEncoding enc;
    if (parsed.cards.empty()) {
        enc.typeId      = -1;
        enc.keyPointIdx = -1;
    } else {
        enc.typeId      = cardTypeToTypeId(parsed.type);
        enc.keyPointIdx = pointToKeyPointIdx(parsed.keyPoint);
    }
    return enc;
}

LastPlayEncoding emptyLastPlay() {
    LastPlayEncoding enc;
    enc.clear();
    return enc;
}

bool isEndgameBoundary(const InfoSetKey& key) {
    return key.myDeckBucket == 0
        && key.oppDeckBucket == 0
        && (int)decodeHand(key.myHandCode).size() <= kEndgameMaxHandSize
        && key.oppHandSize <= kEndgameMaxHandSize;
}

uint8_t compressHistory(const std::vector<CardTypeResult>& recentPlays, int maxRecent) {
    uint8_t h = 0;
    int start = (int)recentPlays.size() - maxRecent;
    if (start < 0) start = 0;
    for (int i = start; i < (int)recentPlays.size(); ++i) {
        int tid = cardTypeToTypeId(recentPlays[i].type);
        int kpi = pointToKeyPointIdx(recentPlays[i].keyPoint);
        h = h * 31 + (uint8_t)(tid * 17 + kpi + 1);
    }
    return h;
}