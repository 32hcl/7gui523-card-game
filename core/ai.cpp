#include "ai.h"
#include "cardtype.h"
#include "player.h"
#include "ai_params.h"
#include <sstream>
#include <random>
#include <map>
#include <set>
#include <climits>

AIParams g_ai4Params;
void setAI4Params(const AIParams& p) { g_ai4Params = p; }
AIParams getAI4Params() { return g_ai4Params; }

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
                if (RANK_MAP.at(parsed.keyPoint) < RANK_MAP.at(bestParsed.keyPoint)) {
                    best = play; bestParsed = parsed;
                }
            }
            return best;
        }
        if (!bombPlays.empty()) {
            auto best = bombPlays[0];
            int bestRank = RANK_MAP.at(parseCardType(best).keyPoint);
            for (const auto& play : bombPlays) {
                int rank = RANK_MAP.at(parseCardType(play).keyPoint);
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
        int rank = RANK_MAP.at(c.point);
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
    }

    return gain;
}

// AI4 评分：所有权重由 g_ai4Params 控制
static int playGainAI4(const std::vector<Card>& play,
                       const Player& player,
                       const Player& opponent,
                       const Deck& deck,
                       const CardTypeResult& previous,
                       int tableScore,
                       const CardTracker* tracker) {
    const AIParams& P = g_ai4Params;
    int gain = 0;
    static bool printed = false;
    if (!printed) { std::cout << "TSW=" << P.tableScoreWeight << std::endl; printed = true; }

    // 桌面分基础收益（所有人都有，鼓励抢回合）
    gain += tableScore * P.tableScoreWeight;

    // 出牌张数权重
    gain += (int)play.size() * P.cardCountWeight;

    // 出完奖励
    if (play.size() == player.hand.size()) gain += P.finishBonus;

    // 前期 — 大牌惩罚
    for (const Card& c : play) {
        int rank = RANK_MAP.at(c.point);
        if (rank >= 13)
            gain -= P.earlyBigPenalty;
        else if (rank >= 10)
            gain -= P.earlyMidPenalty;
        else
            gain -= rank;
    }

    // 中期 — 仅当桌面无分时惩罚出分牌（桌面有分时出分牌是抢分，不应惩罚）
    for (const Card& c : play) {
        if (c.score > 0 && tableScore == 0)
            gain -= P.midScorePenalty;
    }

    // 抢分
    if (tableScore >= P.stealThreshold) {
        for (const Card& c : play) {
            if (c.score > 0)
                gain += c.score * P.stealMultiplier;
        }
    } else {
        // 没把握时减分
        for (const Card& c : play) {
            if (c.score > 0)
                gain -= tableScore * P.noConfidencePenalty;
        }
    }

    // 牌堆顶预测
    if (tracker) {
        for (const Card& c : play) {
            if (tracker->isExhausted(c.point))
                gain += P.deckTopBonus;
        }
    }

    // 特殊胜利保留 — 检查 7/鬼/5/2/3
    {
        const std::set<std::string> specialPoints = {"7", "大鬼", "小鬼", "5", "2", "3"};
        int handSpecialCount = 0;
        for (const Card& c : player.hand)
            if (specialPoints.count(c.point)) handSpecialCount++;
        if (handSpecialCount >= 3) {
            // 手牌已有足够特殊牌，惩罚出掉任何一张特殊牌
            for (const Card& c : play) {
                if (specialPoints.count(c.point))
                    gain -= P.specialKeepBonus;
            }
        }
    }

    // 拆对惩罚
    {
        // 按点数去重，只对"该点数出恰好 1 张且手牌中该点数 >= 2"扣一次
        std::set<std::string> deduped;
        for (const Card& c : play) {
            if (deduped.count(c.point)) continue;
            deduped.insert(c.point);
            int inHand = 0;
            for (const Card& h : player.hand)
                if (h.point == c.point) inHand++;
            int inPlay = 0;
            for (const Card& pc : play)
                if (pc.point == c.point) inPlay++;
            if (inHand >= 2 && inPlay == 1)
                gain -= P.splitPairPenalty;
        }
    }

    // 炸弹保留
    {
        auto parsed = parseCardType(play);
        if (parsed.type == CardType::Bomb)
            gain -= P.bombKeepPenalty;
        else if (parsed.type == CardType::Rocket)
            gain -= P.rocketKeepPenalty;
    }

    // 终局 — 对方手牌少时炸弹加成分
    if ((int)opponent.hand.size() <= 2) {
        auto parsed = parseCardType(play);
        if (parsed.type == CardType::Bomb)
            gain += P.endgameBombBonus;
        else if (parsed.type == CardType::Rocket)
            gain += P.endgameRocketBonus;
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
        int rank = RANK_MAP.at(c.point);
        if (rank >= 13) rankPenalty -= rank * 3;
        else if (rank >= 10) rankPenalty -= rank * 2;
        else rankPenalty -= rank;
    }
    bd.endgame = rankPenalty;

    for (const Card& c : play) {
        if (c.score > 0 && tableScore > 0) {
            bd.scoreCard += c.score * 2;
        }
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

    if (previous.cards.empty()) {
        for (const auto& play : allPlays) {
            if (play.size() == player.hand.size()) return play;
        }
    }

    if (!previous.cards.empty()) {
        for (const auto& play : allPlays) {
            auto parsed = parseCardType(play);
            if (canBeat(parsed, previous) && play.size() == player.hand.size()) {
                return play;
            }
        }
    }

    if (!previous.cards.empty() && (int)opponent.hand.size() <= 2) {
        std::vector<Card> rocket, smallestBomb;
        int smallestBombRank = INT_MAX;
        for (const auto& play : allPlays) {
            auto parsed = parseCardType(play);
            if (!canBeat(parsed, previous)) continue;
            if (parsed.type == CardType::Rocket) {
                rocket = play;
            } else if (parsed.type == CardType::Bomb) {
                int rank = RANK_MAP.at(parsed.keyPoint);
                if (rank < smallestBombRank) {
                    smallestBombRank = rank;
                    smallestBomb = play;
                }
            }
        }
        if (!rocket.empty()) return rocket;
        if (!smallestBomb.empty()) return smallestBomb;
    }

    std::vector<Card> best;
    int bestGain = INT_MIN;
    for (const auto& play : allPlays) {
        auto parsed = parseCardType(play);
        if (!previous.cards.empty() && !canBeat(parsed, previous)) continue;
        int gain = playGain(play, player, opponent, deck, previous, tableScore, nullptr);
        if (gain > bestGain) {
            bestGain = gain;
            best = play;
        }
    }
    if (!best.empty()) return best;

    return {};
}

std::vector<Card> aiChoosePlayAI3(const Player& player,
                                  const Player& opponent,
                                  const CardTypeResult& previous,
                                  const Deck& deck,
                                  int tableScore,
                                  const CardTracker& tracker) {
    auto allPlays = enumerateLegalPlays(player);
    if (allPlays.empty()) return {};

    if (previous.cards.empty()) {
        for (const auto& play : allPlays) {
            if (play.size() == player.hand.size()) return play;
        }
    }

    if (!previous.cards.empty()) {
        for (const auto& play : allPlays) {
            auto parsed = parseCardType(play);
            if (canBeat(parsed, previous) && play.size() == player.hand.size()) {
                return play;
            }
        }
    }

    if (!previous.cards.empty() && (int)opponent.hand.size() <= 2) {
        std::vector<Card> rocket, smallestBomb;
        int smallestBombRank = INT_MAX;
        for (const auto& play : allPlays) {
            auto parsed = parseCardType(play);
            if (!canBeat(parsed, previous)) continue;
            if (parsed.type == CardType::Rocket) {
                rocket = play;
            } else if (parsed.type == CardType::Bomb) {
                int rank = RANK_MAP.at(parsed.keyPoint);
                if (rank < smallestBombRank) {
                    smallestBombRank = rank;
                    smallestBomb = play;
                }
            }
        }
        if (!rocket.empty()) return rocket;
        if (!smallestBomb.empty()) return smallestBomb;
    }

    std::vector<Card> best;
    int bestGain = INT_MIN;
    for (const auto& play : allPlays) {
        auto parsed = parseCardType(play);
        if (!previous.cards.empty() && !canBeat(parsed, previous)) continue;
        int gain = playGain(play, player, opponent, deck, previous, tableScore, &tracker);
        if (gain > bestGain) {
            bestGain = gain;
            best = play;
        }
    }
    if (!best.empty()) return best;

    return {};
}

std::vector<Card> aiChoosePlayAI4(const Player& player,
                                  const Player& opponent,
                                  const CardTypeResult& previous,
                                  const Deck& deck,
                                  int tableScore,
                                  const CardTracker& tracker) {
    auto allPlays = enumerateLegalPlays(player);
    if (allPlays.empty()) return {};

    if (previous.cards.empty()) {
        for (const auto& play : allPlays) {
            if (play.size() == player.hand.size()) return play;
        }
    }

    if (!previous.cards.empty()) {
        for (const auto& play : allPlays) {
            auto parsed = parseCardType(play);
            if (canBeat(parsed, previous) && play.size() == player.hand.size()) {
                return play;
            }
        }
    }

    if (!previous.cards.empty() && (int)opponent.hand.size() <= 2) {
        std::vector<Card> rocket, smallestBomb;
        int smallestBombRank = INT_MAX;
        for (const auto& play : allPlays) {
            auto parsed = parseCardType(play);
            if (!canBeat(parsed, previous)) continue;
            if (parsed.type == CardType::Rocket) {
                rocket = play;
            } else if (parsed.type == CardType::Bomb) {
                int rank = RANK_MAP.at(parsed.keyPoint);
                if (rank < smallestBombRank) {
                    smallestBombRank = rank;
                    smallestBomb = play;
                }
            }
        }
        if (!rocket.empty()) return rocket;
        if (!smallestBomb.empty()) return smallestBomb;
    }

    std::vector<Card> best;
    int bestGain = INT_MIN;
    for (const auto& play : allPlays) {
        auto parsed = parseCardType(play);
        if (!previous.cards.empty() && !canBeat(parsed, previous)) continue;
        int gain = playGainAI4(play, player, opponent, deck, previous, tableScore, &tracker);
        if (gain > bestGain) {
            bestGain = gain;
            best = play;
        }
    }
    if (!best.empty()) return best;

    return {};
}

std::vector<Card> aiChoosePlayWithBreakdown(const Player& player,
                                            const Player& opponent,
                                            const CardTypeResult& previous,
                                            const Deck& deck,
                                            int tableScore,
                                            const CardTracker& tracker,
                                            DecisionBreakdown* outBd) {
    if (player.aiLevel == AILevel::AI1_Simple) {
        if (outBd) *outBd = DecisionBreakdown{};
        return aiChoosePlayAI1(player, previous);
    }

    auto allPlays = enumerateLegalPlays(player);
    if (allPlays.empty()) return {};

    if (previous.cards.empty()) {
        for (const auto& play : allPlays) {
            if (play.size() == player.hand.size()) {
                if (outBd) *outBd = DecisionBreakdown{};
                return play;
            }
        }
    }
    if (!previous.cards.empty()) {
        for (const auto& play : allPlays) {
            auto parsed = parseCardType(play);
            if (canBeat(parsed, previous) && play.size() == player.hand.size()) {
                if (outBd) *outBd = DecisionBreakdown{};
                return play;
            }
        }
    }

    if (!previous.cards.empty() && (int)opponent.hand.size() <= 2) {
        std::vector<Card> rocket, smallestBomb;
        int smallestBombRank = INT_MAX;
        for (const auto& play : allPlays) {
            auto parsed = parseCardType(play);
            if (!canBeat(parsed, previous)) continue;
            if (parsed.type == CardType::Rocket) rocket = play;
            else if (parsed.type == CardType::Bomb) {
                int rank = RANK_MAP.at(parsed.keyPoint);
                if (rank < smallestBombRank) {
                    smallestBombRank = rank;
                    smallestBomb = play;
                }
            }
        }
        if (!rocket.empty()) { if (outBd) *outBd = DecisionBreakdown{}; return rocket; }
        if (!smallestBomb.empty()) { if (outBd) *outBd = DecisionBreakdown{}; return smallestBomb; }
    }

    const CardTracker* trackerPtr = (player.aiLevel == AILevel::AI3_Tracker) ? &tracker : nullptr;
    std::vector<Card> best;
    int bestGain = INT_MIN;
    DecisionBreakdown bestBd;
    for (const auto& play : allPlays) {
        auto parsed = parseCardType(play);
        if (!previous.cards.empty() && !canBeat(parsed, previous)) continue;
        DecisionBreakdown bd;
        int gain = evaluatePlayWithBreakdown(play, player, opponent, deck,
                                              previous, tableScore, trackerPtr, &bd);
        if (gain > bestGain) {
            bestGain = gain;
            best = play;
            bestBd = bd;
        }
    }
    if (!best.empty()) {
        if (outBd) *outBd = bestBd;
        return best;
    }
    return {};
}

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
        case AILevel::AI3_Tracker:
            play = aiChoosePlayAI3(player, opponent, previous, deck, tableScore, tracker);
            break;
        case AILevel::AI4_Expert:
            play = aiChoosePlayAI4(player, opponent, previous, deck, tableScore, tracker);
            break;
        default:
            play = aiChoosePlayAI1(player, previous);
            break;
    }
    // 防御：非先手不得返回压不过的牌（仅先手允许出完即走）
    if (!play.empty() && !previous.cards.empty()) {
        auto parsed = parseCardType(play);
        if (!canBeat(parsed, previous)) {
            return {};
        }
    }
    return play;
}

std::vector<Card> humanChoosePlay(const Player& player, const CardTypeResult& previous) {
    std::cout << std::endl;
    std::cout << "\345\275\223\345\211\215\345\207\272\347\211\214\350\200\205: " << player.name << "\357\274\210\344\275\240\357\274\211" << std::endl;
    std::cout << "\344\275\240\347\232\204\346\211\213\347\211\214:" << std::endl;
    for (size_t i = 0; i < player.hand.size(); ++i) {
        std::cout << "  " << (i + 1) << ": ";
        printCard(player.hand[i]);
        if (player.hand[i].score > 0) std::cout << " (" << player.hand[i].score << "\345\210\206)";
        std::cout << std::endl;
    }
    if (!previous.cards.empty()) {
        std::cout << "\344\270\212\344\270\200\346\211\213\345\207\272\347\211\214: ";
        printCardTypeResult(previous);
        std::cout << std::endl;
    } else {
        std::cout << "\350\277\231\344\270\200\346\211\213\347\224\261\344\275\240\345\274\200\345\247\213\345\207\272\347\211\214" << std::endl;
    }
    std::cout << "\350\276\223\345\205\245\345\207\272\347\211\214\345\272\217\345\217\267\357\274\210\347\251\272\346\240\274\345\210\206\351\232\224\357\274\214\344\276\213\345\246\202: 1 3 5\357\274\214\350\276\223\345\205\245 0 \350\241\250\347\244\272\344\270\215\350\246\201\357\274\211: ";
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
        std::cout << "\346\227\240\346\225\210\347\211\214\345\236\213" << std::endl;
        return {};
    }
    if (!previous.cards.empty() && !canBeat(parsed, previous)) {
        std::cout << "\346\227\240\346\263\225\346\211\223\350\277\207\344\270\212\344\270\200\346\211\213" << std::endl;
        return {};
    }
    return chosen;
}