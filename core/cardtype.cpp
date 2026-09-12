#include "cardtype.h"
#include "score.h"

bool isJoker(const Card& card) {
    return card.point == "大鬼" || card.point == "小鬼";
}

bool isSamePoint(const std::vector<Card>& cards) {
    if (cards.empty()) return false;
    const std::string& firstPoint = cards[0].point;
    for (size_t i = 1; i < cards.size(); ++i) {
        if (cards[i].point != firstPoint) return false;
    }
    return true;
}

bool isRocket(const std::vector<Card>& cards) {
    if (cards.size() != 2) return false;
    bool hasBigJoker = false, hasSmallJoker = false;
    for (const auto& card : cards) {
        if (card.point == "大鬼") hasBigJoker = true;
        if (card.point == "小鬼") hasSmallJoker = true;
    }
    return hasBigJoker && hasSmallJoker;
}

bool hasTriple(const std::vector<Card>& cards, std::string& outTriplePoint) {
    std::map<std::string, int> countMap;
    for (const auto& card : cards) countMap[card.point]++;
    for (const auto& entry : countMap) {
        if (entry.second >= 3 && entry.first != "大鬼" && entry.first != "小鬼") {
            outTriplePoint = entry.first;
            return true;
        }
    }
    return false;
}

bool hasPair(const std::vector<Card>& cards, std::string& outPairPoint,
             const std::string& excludePoint) {
    std::map<std::string, int> countMap;
    for (const auto& card : cards) countMap[card.point]++;
    for (const auto& entry : countMap) {
        if (entry.second >= 2 && entry.first != "大鬼" && entry.first != "小鬼"
            && entry.first != excludePoint) {
            outPairPoint = entry.first;
            return true;
        }
    }
    return false;
}

CardTypeResult parseCardType(const std::vector<Card>& cards) {
    CardTypeResult result;
    result.type = CardType::Invalid;
    result.keyPoint = "";
    result.cards = cards;
    size_t size = cards.size();

    if (size == 1) {
        result.type = CardType::Single;
        result.keyPoint = cards[0].point;
    } else if (size == 2) {
        if (isRocket(cards)) {
            result.type = CardType::Rocket;
            result.keyPoint = "王炸";
        } else if (isSamePoint(cards) && !isJoker(cards[0])) {
            result.type = CardType::Pair;
            result.keyPoint = cards[0].point;
        }
    } else if (size == 3) {
        if (isSamePoint(cards) && !isJoker(cards[0])) {
            result.type = CardType::Triple;
            result.keyPoint = cards[0].point;
        }
    } else if (size == 4) {
        if (isSamePoint(cards) && !isJoker(cards[0])) {
            result.type = CardType::Bomb;
            result.keyPoint = cards[0].point;
        } else {
            std::string triplePoint;
            if (hasTriple(cards, triplePoint)) {
                result.type = CardType::TripleWithOne;
                result.keyPoint = triplePoint;
            }
        }
    } else if (size == 5) {
        std::string triplePoint;
        std::string pairPoint;
        if (hasTriple(cards, triplePoint) && hasPair(cards, pairPoint, triplePoint)) {
            result.type = CardType::TripleWithTwo;
            result.keyPoint = triplePoint;
        }
    }
    return result;
}

bool canBeat(const CardTypeResult& candidate, const CardTypeResult& previous) {
    if (previous.cards.empty()) return true;
    if (candidate.type == CardType::Invalid) return false;
    if (candidate.type == CardType::Rocket) {
        return previous.type != CardType::Rocket;
    }
    if (previous.type == CardType::Rocket) return false;
    if (candidate.type == CardType::Bomb) {
        if (previous.type == CardType::Bomb) {
            return RANK_MAP.at(candidate.keyPoint) >= RANK_MAP.at(previous.keyPoint);
        }
        return true;
    }
    if (previous.type == CardType::Bomb) return false;
    if (candidate.type != previous.type) return false;
    return RANK_MAP.at(candidate.keyPoint) >= RANK_MAP.at(previous.keyPoint);
}

int calculatePressureBonus(const CardTypeResult& candidate,
                           const CardTypeResult& previous) {
    if (previous.cards.empty()) return 0;
    if (candidate.type != CardType::Single && candidate.type != CardType::Pair)
        return 0;
    if (candidate.type != previous.type) return 0;
    if (candidate.keyPoint != previous.keyPoint) return 0;
    for (const Card& c : candidate.cards) {
        if (c.score == 0) return 0;
    }
    int bonus = 0;
    for (const Card& c : candidate.cards) {
        bonus += c.score;
    }
    return bonus;
}

void printCardTypeResult(const CardTypeResult& result) {
    std::cout << cardTypeToString(result.type);
    if (!result.keyPoint.empty()) std::cout << " " << result.keyPoint;
    std::cout << " (";
    for (size_t i = 0; i < result.cards.size(); ++i) {
        printCard(result.cards[i]);
        if (i < result.cards.size() - 1) std::cout << " ";
    }
    std::cout << ")";
}