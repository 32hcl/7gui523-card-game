#include "ai_types.h"
#include "core/card/rank.h"
#include "ai.h"
#include "engine/play_selector.h"
#include "plugins/counter_risk.h"
#include "plugins/minimax.h"
#include "core/card/cardtype.h"
#include "core/player.h"
#include <sstream>
#include <iostream>
#include <random>
#include <map>
#include <set>
#include <climits>



std::vector<std::vector<Card>> enumerateLegalPlays(const Player& player) {
    std::vector<std::vector<Card>> result;
    size_t n = player.hand.size();
    for (size_t mask = 1; mask < (1ULL << n); ++mask) {
        std::vector<Card> subset;
        for (size_t i = 0; i < n; ++i) {
            if (mask & (1ULL << i)) subset.push_back(player.hand[i]);
        }
        if (parseCardType(subset).type != CardType::Invalid) {
            result.push_back(subset);
        }
    }
    return result;
}

std::vector<Card> aiChoosePlayAI1(const Player& player, const CardTypeResult& previous) {
    auto allPlays = enumerateLegalPlays(player);
    if (allPlays.empty()) return {};

    if (previous.cards.empty()) {
        std::vector<std::vector<Card>> singles, pairs, triples, triplesWithOne, triplesWithTwo, bombs, rockets;
        for (const auto& play : allPlays) {
            auto parsed = parseCardType(play);
            switch (parsed.type) {
                case CardType::Single:        singles.push_back(play); break;
                case CardType::Pair:          pairs.push_back(play); break;
                case CardType::Triple:        triples.push_back(play); break;
                case CardType::TripleWithOne: triplesWithOne.push_back(play); break;
                case CardType::TripleWithTwo: triplesWithTwo.push_back(play); break;
                case CardType::Bomb:          bombs.push_back(play); break;
                case CardType::Rocket:        rockets.push_back(play); break;
                default: break;
            }
        }
        std::vector<std::vector<std::vector<Card>>*> availableTypes;
        if (!rockets.empty())        availableTypes.push_back(&rockets);
        if (!bombs.empty())          availableTypes.push_back(&bombs);
        if (!triplesWithOne.empty()) availableTypes.push_back(&triplesWithOne);
        if (!triplesWithTwo.empty()) availableTypes.push_back(&triplesWithTwo);
        if (!triples.empty())        availableTypes.push_back(&triples);
        if (!pairs.empty())          availableTypes.push_back(&pairs);
        if (!singles.empty())        availableTypes.push_back(&singles);
        if (availableTypes.empty()) return {};

        std::random_device rd;
        std::mt19937 g(rd());
        std::uniform_int_distribution<> typeDist(0, (int)availableTypes.size() - 1);
        auto& chosenVec = *availableTypes[typeDist(g)];
        std::uniform_int_distribution<> cardDist(0, (int)chosenVec.size() - 1);
        return chosenVec[cardDist(g)];
    } else {
        std::vector<std::vector<Card>> sameTypePlays, bombPlays, rocketPlays;
        for (const auto& play : allPlays) {
            auto parsed = parseCardType(play);
            if (!canBeat(parsed, previous)) continue;
            if (parsed.type == CardType::Bomb) bombPlays.push_back(play);
            else if (parsed.type == CardType::Rocket) rocketPlays.push_back(play);
            else sameTypePlays.push_back(play);
        }
        if (!sameTypePlays.empty()) {
            auto best = sameTypePlays[0];
            auto bestParsed = parseCardType(best);
            for (const auto& play : sameTypePlays) {
                auto parsed = parseCardType(play);
                if (getCardRank(parsed.keyPoint) < getCardRank(bestParsed.keyPoint)) {
                    best = play; bestParsed = parsed;
                }
            }
            return best;
        }
        if (!bombPlays.empty()) {
            auto best = bombPlays[0];
            int bestRank = getCardRank(parseCardType(best).keyPoint);
            for (const auto& play : bombPlays) {
                int rank = getCardRank(parseCardType(play).keyPoint);
                if (rank < bestRank) { bestRank = rank; best = play; }
            }
            return best;
        }
        if (!rocketPlays.empty()) return rocketPlays[0];
    }
    return {};
}

bool breaksCombo(const std::vector<Card>& play, const Player& player) {
    std::map<std::string, int> handCount;
    for (const Card& c : player.hand) handCount[c.point]++;

    std::map<std::string, int> playCount;
    for (const Card& c : play) playCount[c.point]++;

    for (const auto& kv : playCount) {
        int inHand = handCount[kv.first];
        int inPlay = kv.second;
        if (inHand >= 2 && inPlay == 1) return true;
        if (inHand >= 3 && inPlay == 2) return true;
        if (inHand >= 4 && inPlay == 3) return true;
    }
    return false;
}

static int playGain(const std::vector<Card>& play,
                    const Player& player,
                    const Player& opponent,
                    const Deck& deck,
                    const CardTypeResult& previous,
                    int tableScore,
                    const CardTracker* tracker) {
    int gain = 0;
    gain += tableScore * 2;
    gain += (int)play.size() * 10;
    if (play.size() == player.hand.size()) gain += 1000;

    for (const Card& c : play) {
        int rank = getCardRank(c.point);
        if (rank >= 13) gain -= rank * 3;
        else if (rank >= 10) gain -= rank * 2;
        else gain -= rank;
    }

    for (const Card& c : play) {
        if (c.score > 0 && tableScore > 0) gain += c.score * 2;
    }

    if (tracker) {
        for (const Card& c : play) {
            if (tracker->isExhausted(c.point)) gain += 30;
        }
        auto parsed = parseCardType(play);
        int risk = estimateCounterRisk(play, player, opponent, tracker, parsed, tableScore, deck);
        gain -= risk * kCounterRiskWeight;
    }

    return gain;
}

int evaluatePlayWithBreakdown(const std::vector<Card>& play,
                              const Player& player,
                              const Player& opponent,
                              const Deck& deck,
                              const CardTypeResult& previous,
                              int tableScore,
                              const CardTracker* tracker,
                              DecisionBreakdown* out) {
    DecisionBreakdown bd;
    bd.tableScore = tableScore * 2;

    int sizeScore = (int)play.size() * 10;
    if (play.size() == player.hand.size()) sizeScore += 1000;

    int rankPenalty = 0;
    for (const Card& c : play) {
        int rank = getCardRank(c.point);
        if (rank >= 13) rankPenalty -= rank * 3;
        else if (rank >= 10) rankPenalty -= rank * 2;
        else rankPenalty -= rank;
    }
    bd.endgame = rankPenalty;

    for (const Card& c : play) {
        if (c.score > 0 && tableScore > 0) bd.scoreCard += c.score * 2;
    }

    if (tracker) {
        for (const Card& c : play) {
            if (tracker->isExhausted(c.point)) bd.tracker += 30;
        }
    }

    bd.base = sizeScore;
    bd.special = 0;
    bd.comboBreak = 0;
    bd.defensive = 0;
    bd.total = bd.base + bd.scoreCard + bd.special + bd.endgame
             + bd.comboBreak + bd.tableScore + bd.defensive + bd.tracker;

    if (out) *out = bd;
    return bd.total;
}

std::vector<Card> aiChoosePlayAI2(const Player& player,
                                  const Player& opponent,
                                  const CardTypeResult& previous,
                                  const Deck& deck,
                                  int tableScore) {
    auto allPlays = enumerateLegalPlays(player);
    if (allPlays.empty()) return {};

    auto finish = tryFinishPlay(player, allPlays, previous);
    if (!finish.empty()) return finish;

    auto intercept = tryEndgameIntercept(player, allPlays, previous, (int)opponent.hand.size());
    if (!intercept.empty()) return intercept;

    std::vector<Card> best;
    int bestGain = INT_MIN;
    for (const auto& play : allPlays) {
        auto parsed = parseCardType(play);
        if (!previous.cards.empty() && !canBeat(parsed, previous)) continue;
        int gain = playGain(play, player, opponent, deck, previous, tableScore, nullptr);
        if (gain > bestGain) { bestGain = gain; best = play; }
    }
    return best;
}

std::vector<Card> aiChoosePlayAI3(const Player& player,
                                  const Player& opponent,
                                  const CardTypeResult& previous,
                                  const Deck& deck,
                                  int tableScore,
                                  const CardTracker& tracker) {
    auto allPlays = enumerateLegalPlays(player);
    if (allPlays.empty()) return {};

    auto finish = tryFinishPlay(player, allPlays, previous);
    if (!finish.empty()) return finish;

    auto intercept = tryEndgameIntercept(player, allPlays, previous, (int)opponent.hand.size());
    if (!intercept.empty()) return intercept;

    std::vector<Card> best;
    int bestGain = INT_MIN;
    for (const auto& play : allPlays) {
        auto parsed = parseCardType(play);
        if (!previous.cards.empty() && !canBeat(parsed, previous)) continue;
        int gain = playGain(play, player, opponent, deck, previous, tableScore, &tracker);
        if (gain > bestGain) { bestGain = gain; best = play; }
    }
    return best;
}

std::vector<Card> aiChoosePlayAI4(const Player& player,
                                  const Player& opponent,
                                  const CardTypeResult& previous,
                                  const Deck& deck,
                                  int tableScore,
                                  const CardTracker& tracker) {
    return searchBestPlayCheat(player, opponent, previous, deck, tableScore, g_searchBestPlayCheatDepth);
}

int g_searchBestPlayCheatDepth = 6;

std::vector<Card> aiChoosePlay(const Player& player,
                               const Player& opponent,
                               const CardTypeResult& previous,
                               const Deck& deck,
                               int tableScore,
                               const CardTracker& tracker) {
    std::vector<Card> play;
    switch (player.aiLevel) {
        case AILevel::AI1_Simple:
            play = aiChoosePlayAI1(player, previous);
            break;
        case AILevel::AI2_Rule:
            play = aiChoosePlayAI2(player, opponent, previous, deck, tableScore);
            break;
        case AILevel::AI4_Expert:
            play = aiChoosePlayAI4(player, opponent, previous, deck, tableScore, tracker);
            break;
        default:
            play = aiChoosePlayAI1(player, previous);
            break;
    }
    if (!play.empty() && !previous.cards.empty()) {
        auto parsed = parseCardType(play);
        if (!canBeat(parsed, previous)) return {};
    }
    return play;
}

std::vector<Card> humanChoosePlay(const Player& player, const CardTypeResult& previous) {
    std::cout << std::endl;
    std::cout << "当前出牌者: " << player.name << "（你）" << std::endl;
    std::cout << "你的手牌:" << std::endl;
    for (size_t i = 0; i < player.hand.size(); ++i) {
        std::cout << "  " << (i + 1) << ": ";
        printCard(player.hand[i]);
        if (player.hand[i].score > 0) std::cout << " (" << player.hand[i].score << "分)";
        std::cout << std::endl;
    }
    if (!previous.cards.empty()) {
        std::cout << "上一手出牌: ";
        printCardTypeResult(previous);
        std::cout << std::endl;
    } else {
        std::cout << "这一手由你开始出牌" << std::endl;
    }
    std::cout << "输入出牌序号（空格分隔，例如: 1 3 5，输入 0 表示不要）: ";
    std::string line;
    std::getline(std::cin, line);

    if (line.empty() || line == "0") return {};

    std::istringstream iss(line);
    std::vector<Card> chosen;
    int idx;
    while (iss >> idx) {
        if (idx >= 1 && (size_t)idx <= player.hand.size()) {
            chosen.push_back(player.hand[idx - 1]);
        }
    }
    if (chosen.empty()) return {};

    CardTypeResult parsed = parseCardType(chosen);
    if (parsed.type == CardType::Invalid) {
        std::cout << "无效牌型" << std::endl;
        return {};
    }
    if (!previous.cards.empty() && !canBeat(parsed, previous)) {
        std::cout << "无法打过上一手" << std::endl;
        return {};
    }
    return chosen;
}