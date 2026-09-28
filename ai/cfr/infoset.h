#pragma once

#include "cfr_types.h"
#include "core/card/cardtype.h"
#include <vector>
#include <string>

int deckCountToBucket(int count);

int cardTypeToTypeId(CardType type);

int pointToKeyPointIdx(const std::string& point);

InfoSetKey buildInfoSetKey(
    const std::vector<std::string>& myHandPoints,
    int oppHandSize,
    int myDeckCount,
    int oppDeckCount,
    int tableTotalScore,
    const LastPlayEncoding& lastPlay,
    uint8_t historyHash
);

LastPlayEncoding makeLastPlayEncoding(const CardTypeResult& parsed);

LastPlayEncoding emptyLastPlay();

bool isEndgameBoundary(const InfoSetKey& key);

uint8_t compressHistory(const std::vector<CardTypeResult>& recentPlays, int maxRecent);