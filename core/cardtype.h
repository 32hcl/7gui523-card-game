#pragma once

#include <string>
#include <vector>
#include <map>
#include "card.h"

struct CardTypeResult {
    CardType type = CardType::Invalid;
    std::string keyPoint;
    std::vector<Card> cards;
    int bonusScore = 0;
};

bool isJoker(const Card& card);
bool isSamePoint(const std::vector<Card>& cards);
bool isRocket(const std::vector<Card>& cards);
bool hasTriple(const std::vector<Card>& cards, std::string& outTriplePoint);
bool hasPair(const std::vector<Card>& cards, std::string& outPairPoint,
             const std::string& excludePoint = "");
CardTypeResult parseCardType(const std::vector<Card>& cards);
bool canBeat(const CardTypeResult& candidate, const CardTypeResult& previous);
int calculatePressureBonus(const CardTypeResult& candidate,
                           const CardTypeResult& previous);
void printCardTypeResult(const CardTypeResult& result);