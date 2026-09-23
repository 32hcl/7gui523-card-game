#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <fstream>
#include <sstream>
#include <map>
#include <set>

#include "ai/engine/ai_levels.h"
#include "ai/engine/ai_engine.h"
#include "ai/ai.h"
#include "ai/ai_types.h"
#include "core/card/deck.h"
#include "core/player.h"
#include "core/card/cardtype.h"
#include "core/tracker/cardtracker.h"
#include "core/rule/score.h"
#include "core/rule/special.h"

// ── 对战统计 ──────────────────────────────────────────────────

struct BattleStats {
    int firstWins = 0;
    int secondWins = 0;
    int draws = 0;
    int specialWins = 0;
    long long firstScoreSum = 0;
    long long secondScoreSum = 0;
};

// ── 辅助函数 ──────────────────────────────────────────────────

static std::string levelName(int lv) {
    return getAILevelName(lv);
}

static std::string pointName(const std::string& p) {
    if (p == "大鬼") return "大鬼";
    if (p == "小鬼") return "小鬼";
    return p;
}

static std::string cardsToString(const std::vector<Card>& cards) {
    std::string r;
    for (size_t i = 0; i < cards.size(); ++i) {
        if (i > 0) r += " ";
        if (cards[i].suit.empty()) r += cards[i].point;
        else r += cards[i].suit + cards[i].point;
    }
    return r;
}

static int countPoint(const std::vector<Card>& hand, const std::string& point) {
    int c = 0;
    for (auto& h : hand) if (h.point == point) c++;
    return c;
}

static bool hasBombInHand(const std::vector<Card>& hand) {
    std::map<std::string,int> m;
    for (auto& c : hand) m[c.point]++;
    for (auto& kv : m) if (kv.second == 4) return true;
    return false;
}

static bool hasRocketInHand(const std::vector<Card>& hand) {
    bool big = false, small = false;
    for (auto& c : hand) {
        if (c.point == "大鬼") big = true;
        if (c.point == "小鬼") small = true;
    }
    return big && small;
}

static bool canFormSpecial523(const std::vector<Card>& hand) {
    bool h7=false,hj=false,h5=false,h2=false,h3=false;
    for (auto& c : hand) {
        if (c.point=="7") h7=true;
        if (c.point=="大鬼"||c.point=="小鬼") hj=true;
        if (c.point=="5") h5=true;
        if (c.point=="2") h2=true;
        if (c.point=="3") h3=true;
    }
    return h7&&hj&&h5&&h2&&h3;
}

// 检查手牌是否能一次出完（所有牌构成一个合法牌型）
static bool canFinishInOnePlay(const std::vector<Card>& hand) {
    if (hand.empty()) return false;
    CardTypeResult r = parseCardType(hand);
    return r.type != CardType::Invalid;
}

// 点数转 RANK 值
static int getRank(const std::string& point) {
    auto it = RANK_MAP.find(point);
    return (it != RANK_MAP.end()) ? it->second : 0;
}

// 找出手牌中比给定牌更小的单张
static std::vector<Card> findSmallerSingles(const std::vector<Card>& hand, const Card& played) {
    std::vector<Card> result;
    int playedRank = getRank(played.point);
    for (auto& c : hand) {
        if (c.point == played.point && c.suit == played.suit) continue; // skip the played card
        int r = getRank(c.point);
        if (r < playedRank) result.push_back(c);
    }
    return result;
}

// ── 点数列表（按 RANK 从大到小）──
static const char* POINT_NAMES[] = {
    "7", "大鬼", "小鬼", "5", "2", "3",
    "A", "K", "Q", "J", "10", "9", "8", "6", "4"
};
static const int NUM_POINTS = 15;

// ── 单局记录 ──────────────────────────────────────────────────

struct FirstMoveRecord {
    int gameIndex;
    int firstLevel;
    int secondLevel;
    std::vector<Card> hand;
    std::vector<Card> firstCards;
    CardType firstType;
    std::string firstKeyPoint;
    bool finishedInOnePlay;
    bool canFinishInOnePlay;
    int handSize;
    int sevenCount, jokerCount, fiveCount, twoCount, threeCount;
    int specialCardCount;
    bool hasRocket, hasBomb, hasSpecial523;
    std::vector<Card> smallerSingles;
};

// ── 按先手 AI 汇总 ──────────────────────────────────────────

struct FirstMoveStats {
    int totalGames = 0;

    // 1.1 牌型分布
    int typeCounts[7] = {0}; // Single..Rocket (7 types, excl Special523)

    // 1.2 点数分布（仅首张单张）
    int pointCounts[NUM_POINTS] = {0};
    int singleGames = 0;

    // 1.3 大牌首出
    int sevenFirst = 0;
    int jokerFirst = 0;
    int fiveFirst = 0;
    int twoFirst = 0;
    int threeFirst = 0;
    int bombFirst = 0;
    int rocketFirst = 0;

    // 1.4 出 7 详情
    int sevenSevenCount[5] = {0}; // index by number of 7s in hand (0..4)
    int sevenHasSpecial523 = 0;
    int sevenSpecialTotal[8] = {0}; // index by special card total
    int sevenTotalCount = 0;

    // 1.5 手牌结构
    int handHasRocket = 0; int rocketFirstType[7] = {0};
    int handHasBomb   = 0; int bombFirstType[7]   = {0};
    int handHasS523   = 0; int s523FirstType[7]   = {0};
    int handSpecGE3   = 0; int specGE3FirstType[7]= {0};

    // 1.6 出完即走
    int couldFinishCount = 0;
    int actualFinishCount = 0;

    // 均牌力
    long long rankSum = 0;
    int singleCount = 0;

    // 异常样本
    std::vector<FirstMoveRecord> anomalies;
};

// ── 点数名称映射 ──

static int pointIndex(const std::string& p) {
    if (p=="7") return 0;
    if (p=="大鬼") return 1;
    if (p=="小鬼") return 2;
    if (p=="5") return 3;
    if (p=="2") return 4;
    if (p=="3") return 5;
    if (p=="A") return 6;
    if (p=="K") return 7;
    if (p=="Q") return 8;
    if (p=="J") return 9;
    if (p=="10") return 10;
    if (p=="9") return 11;
    if (p=="8") return 12;
    if (p=="6") return 13;
    if (p=="4") return 14;
    return -1;
}

static int typeIndex(CardType t) {
    switch (t) {
        case CardType::Single: return 0;
        case CardType::Pair: return 1;
        case CardType::Triple: return 2;
        case CardType::TripleWithOne: return 3;
        case CardType::TripleWithTwo: return 4;
        case CardType::Bomb: return 5;
        case CardType::Rocket: return 6;
        default: return -1;
    }
}

static std::vector<Card> aiEngineChoosePlay(int level,
                                             const Player& player,
                                             const Player& opponent,
                                             const CardTypeResult& previous,
                                             const Deck& deck,
                                             int tableScore,
                                             CardTracker& tracker) {
    AIEngineConfig cfg = buildAIEngineConfig(level);
    AIEngine engine(cfg);
    engine.setOpponentHand(opponent.hand);
    return engine.choosePlay(player, opponent, previous, deck, tableScore);
}

// ── runOneGame 增强版 ──

static void runOneGame(int levelFirst, int levelSecond,
                       std::mt19937& rng, BattleStats& stats,
                       FirstMoveStats& fmStats, int gameIndex) {
    Deck deck = createStandardDeck();
    std::shuffle(deck.cards.begin(), deck.cards.end(), rng);

    Player first = createPlayer("P1");
    Player second = createPlayer("P2");

    dealCards(first, deck, 5);
    dealCards(second, deck, 5);

    // ── 记录先手方第一手 ──
    std::vector<Card> firstHand = first.hand; // snapshot

    CardTracker tracker;

    AIEngine engineFirst(buildAIEngineConfig(levelFirst));
    AIEngine engineSecond(buildAIEngineConfig(levelSecond));
    engineFirst.setOpponentHand(second.hand);
    engineSecond.setOpponentHand(first.hand);

    if (checkSpecialVictory(first)) {
        stats.firstWins++; stats.specialWins++; return;
    }
    if (checkSpecialVictory(second)) {
        stats.secondWins++; stats.specialWins++; return;
    }

    Player* current = &first;
    Player* opponent = &second;
    Player* lastPlayer = nullptr;
    std::vector<Card> tableCards;
    CardTypeResult lastPlay;
    lastPlay.type = CardType::Invalid;
    lastPlay.cards.clear();
    lastPlay.keyPoint.clear();

    bool firstMoveRecorded = false;

    int safety = 0;
    while (true) {
        safety++;
        if (safety > 500) break;

        while (true) {
            int tableScore = calculateScore(tableCards);
            std::vector<Card> play;
            if (current == &first) {
                play = engineFirst.choosePlay(*current, *opponent, lastPlay, deck, tableScore);
            } else {
                play = engineSecond.choosePlay(*current, *opponent, lastPlay, deck, tableScore);
            }

            if (play.empty()) {
                if (lastPlay.type == CardType::Invalid) break;
                settleScoreCards(*lastPlayer, tableCards);
                tableCards.clear();
                break;
            }

            CardTypeResult parsed = parseCardType(play);
            if (parsed.type == CardType::Invalid) break;
            if (lastPlay.type != CardType::Invalid && !canBeat(parsed, lastPlay)) break;

            // ── 记录先手第一次出牌 ──
            if (!firstMoveRecorded && current == &first && lastPlay.type == CardType::Invalid) {
                firstMoveRecorded = true;

                fmStats.totalGames++;

                int ti = typeIndex(parsed.type);
                if (ti >= 0) fmStats.typeCounts[ti]++;

                // 单张点数
                if (parsed.type == CardType::Single) {
                    int pi = pointIndex(parsed.keyPoint);
                    if (pi >= 0) { fmStats.pointCounts[pi]++; fmStats.singleGames++; }
                    fmStats.rankSum += getRank(parsed.keyPoint);
                    fmStats.singleCount++;
                }

                // 大牌
                if (parsed.type == CardType::Single && parsed.keyPoint == "7") fmStats.sevenFirst++;
                if (parsed.type == CardType::Single && (parsed.keyPoint=="大鬼"||parsed.keyPoint=="小鬼")) fmStats.jokerFirst++;
                if (parsed.type == CardType::Single && parsed.keyPoint == "5") fmStats.fiveFirst++;
                if (parsed.type == CardType::Single && parsed.keyPoint == "2") fmStats.twoFirst++;
                if (parsed.type == CardType::Single && parsed.keyPoint == "3") fmStats.threeFirst++;
                if (parsed.type == CardType::Bomb) fmStats.bombFirst++;
                if (parsed.type == CardType::Rocket) fmStats.rocketFirst++;

                // 手牌分析
                int sv7 = countPoint(firstHand, "7");
                int svJ = countPoint(firstHand, "大鬼") + countPoint(firstHand, "小鬼");
                int sv5 = countPoint(firstHand, "5");
                int sv2 = countPoint(firstHand, "2");
                int sv3 = countPoint(firstHand, "3");
                int specTotal = sv7 + svJ + sv5 + sv2 + sv3;
                bool hRocket = hasRocketInHand(firstHand);
                bool hBomb = hasBombInHand(firstHand);
                bool hS523 = canFormSpecial523(firstHand);
                bool canFinish = canFinishInOnePlay(firstHand);
                bool didFinish = current->hand.empty() && deck.cards.empty();

                if (canFinish) fmStats.couldFinishCount++;
                if (didFinish) fmStats.actualFinishCount++;

                // 出 7 详情
                if (parsed.type == CardType::Single && parsed.keyPoint == "7") {
                    fmStats.sevenTotalCount++;
                    if (sv7 >=1 && sv7 <=4) fmStats.sevenSevenCount[sv7]++;
                    if (hS523) fmStats.sevenHasSpecial523++;
                    int sp = std::min(specTotal, 7);
                    fmStats.sevenSpecialTotal[sp]++;
                }

                // 手牌结构
                if (hRocket) { fmStats.handHasRocket++; if (ti>=0) fmStats.rocketFirstType[ti]++; }
                if (hBomb)   { fmStats.handHasBomb++;   if (ti>=0) fmStats.bombFirstType[ti]++; }
                if (hS523)   { fmStats.handHasS523++;   if (ti>=0) fmStats.s523FirstType[ti]++; }
                if (specTotal >= 3) { fmStats.handSpecGE3++; if (ti>=0) fmStats.specGE3FirstType[ti]++; }

                // 异常检测
                if ((parsed.type == CardType::Single) &&
                    (parsed.keyPoint == "7" || parsed.keyPoint == "大鬼" || parsed.keyPoint == "小鬼")) {
                    Card playedCard;
                    if (!play.empty()) playedCard = play[0];
                    std::vector<Card> smaller = findSmallerSingles(firstHand, playedCard);
                    // 过滤：只保留也是单张的牌（忽略大小鬼的比较）
                    std::vector<Card> validSmaller;
                    for (auto& c : smaller) {
                        if (c.point != playedCard.point) validSmaller.push_back(c);
                    }
                    if (!validSmaller.empty()) {
                        FirstMoveRecord rec;
                        rec.gameIndex = gameIndex;
                        rec.firstLevel = levelFirst;
                        rec.secondLevel = levelSecond;
                        rec.hand = firstHand;
                        rec.firstCards = play;
                        rec.firstType = parsed.type;
                        rec.firstKeyPoint = parsed.keyPoint;
                        rec.finishedInOnePlay = didFinish;
                        rec.canFinishInOnePlay = canFinish;
                        rec.handSize = (int)firstHand.size();
                        rec.sevenCount = sv7;
                        rec.jokerCount = svJ;
                        rec.fiveCount = sv5;
                        rec.twoCount = sv2;
                        rec.threeCount = sv3;
                        rec.specialCardCount = specTotal;
                        rec.hasRocket = hRocket;
                        rec.hasBomb = hBomb;
                        rec.hasSpecial523 = hS523;
                        rec.smallerSingles = validSmaller;
                        if (fmStats.anomalies.size() < 20) {
                            fmStats.anomalies.push_back(rec);
                        }
                    }
                }
            }

            for (const Card& c : play) {
                auto it = std::find_if(current->hand.begin(), current->hand.end(),
                    [&](const Card& h){ return h.point == c.point && h.suit == c.suit; });
                if (it != current->hand.end()) current->hand.erase(it);
            }
            for (const Card& c : play) tableCards.push_back(c);
            tracker.recordPlayed(play);
            lastPlay = parsed;
            lastPlayer = current;

            if (checkSpecialVictory(*current)) {
                if (current == &first) stats.firstWins++;
                else stats.secondWins++;
                stats.specialWins++;
                return;
            }

            if (current->hand.empty()) {
                if (deck.cards.empty()) {
                    settleScoreCards(*current, tableCards);
                    settleScoreCards(*current, opponent->hand);
                    opponent->hand.clear();
                    tableCards.clear();
                    if (first.totalScore > second.totalScore) stats.firstWins++;
                    else if (second.totalScore > first.totalScore) stats.secondWins++;
                    else stats.draws++;
                    stats.firstScoreSum += first.totalScore;
                    stats.secondScoreSum += second.totalScore;
                    return;
                }
            }

            std::swap(current, opponent);
        }

        lastPlay.type = CardType::Invalid;
        lastPlay.cards.clear();
        lastPlay.keyPoint.clear();

        if (checkSpecialVictory(first)) {
            stats.firstWins++; stats.specialWins++; return;
        }
        if (checkSpecialVictory(second)) {
            stats.secondWins++; stats.specialWins++; return;
        }

        if (!deck.cards.empty()) {
            Player* loser = (lastPlayer == &first) ? &second : &first;
            refillToFive(*lastPlayer, deck);
            refillToFive(*loser, deck);
        }

        current = lastPlayer;
        opponent = (current == &first) ? &second : &first;

        if (deck.cards.empty() && first.hand.empty() && second.hand.empty()) break;
    }

    stats.firstScoreSum += first.totalScore;
    stats.secondScoreSum += second.totalScore;

    if (first.totalScore > second.totalScore) stats.firstWins++;
    else if (second.totalScore > first.totalScore) stats.secondWins++;
    else stats.draws++;
}

// ── 打印函数 ──────────────────────────────────────────────────

static void printTypeDistribution(const FirstMoveStats& s, std::ostream& out) {
    const char* typeNames[] = {"单张","对子","三张","三带一","三带二","炸弹","王炸"};
    out << "### 1.1 牌型分布\n\n";
    out << std::left << std::setw(12) << "牌型" << std::setw(8) << "次数" << "占比\n";
    out << std::string(30, '-') << "\n";
    for (int i = 0; i < 7; ++i) {
        double pct = s.totalGames > 0 ? 100.0 * s.typeCounts[i] / s.totalGames : 0;
        out << std::left << std::setw(12) << typeNames[i]
            << std::setw(8) << s.typeCounts[i]
            << std::fixed << std::setprecision(2) << pct << "%\n";
    }
    out << std::left << std::setw(12) << "合计"
        << std::setw(8) << s.totalGames
        << "100.00%\n\n";
}

static void printPointDistribution(const FirstMoveStats& s, std::ostream& out) {
    out << "### 1.2 点数分布（仅首张单张）\n\n";
    out << std::left << std::setw(8) << "点数" << std::setw(8) << "次数" << "占比\n";
    out << std::string(25, '-') << "\n";
    for (int i = 0; i < NUM_POINTS; ++i) {
        double pct = s.singleGames > 0 ? 100.0 * s.pointCounts[i] / s.singleGames : 0;
        out << std::left << std::setw(8) << POINT_NAMES[i]
            << std::setw(8) << s.pointCounts[i]
            << std::fixed << std::setprecision(2) << pct << "%\n";
    }
    out << std::left << std::setw(8) << "合计"
        << std::setw(8) << s.singleGames
        << "100.00%\n\n";
}

static void printBigCardFirst(const FirstMoveStats& s, std::ostream& out) {
    out << "### 1.3 大牌首出率\n\n";
    out << std::left << std::setw(16) << "指标" << std::setw(8) << "次数" << "占比\n";
    out << std::string(35, '-') << "\n";
    auto pct = [&](int n) { return s.totalGames > 0 ? 100.0*n/s.totalGames : 0; };
    out << std::left << std::setw(16) << "首张出7"
        << std::setw(8) << s.sevenFirst << std::fixed << std::setprecision(2) << pct(s.sevenFirst) << "%\n";
    out << std::left << std::setw(16) << "首张出鬼"
        << std::setw(8) << s.jokerFirst << std::fixed << std::setprecision(2) << pct(s.jokerFirst) << "%\n";
    out << std::left << std::setw(16) << "首张出5"
        << std::setw(8) << s.fiveFirst << std::fixed << std::setprecision(2) << pct(s.fiveFirst) << "%\n";
    out << std::left << std::setw(16) << "首张出2"
        << std::setw(8) << s.twoFirst << std::fixed << std::setprecision(2) << pct(s.twoFirst) << "%\n";
    out << std::left << std::setw(16) << "首张出3"
        << std::setw(8) << s.threeFirst << std::fixed << std::setprecision(2) << pct(s.threeFirst) << "%\n";
    out << std::left << std::setw(16) << "首张出炸弹"
        << std::setw(8) << s.bombFirst << std::fixed << std::setprecision(2) << pct(s.bombFirst) << "%\n";
    out << std::left << std::setw(16) << "首张出王炸"
        << std::setw(8) << s.rocketFirst << std::fixed << std::setprecision(2) << pct(s.rocketFirst) << "%\n\n";
}

static void printSevenDetail(const FirstMoveStats& s, std::ostream& out) {
    out << "### 1.4 首张出7的详细情况\n\n";
    out << "出7总次数: " << s.sevenTotalCount << "\n\n";
    out << "#### 手牌中7的张数分布\n";
    out << std::left << std::setw(10) << "7的张数" << std::setw(8) << "次数" << "占比\n";
    out << std::string(25, '-') << "\n";
    for (int i = 1; i <= 4; ++i) {
        double pct = s.sevenTotalCount > 0 ? 100.0*s.sevenSevenCount[i]/s.sevenTotalCount : 0;
        out << std::left << std::setw(10) << std::to_string(i) + "张"
            << std::setw(8) << s.sevenSevenCount[i]
            << std::fixed << std::setprecision(2) << pct << "%\n";
    }
    out << "\n出7时手牌有Special523: " << s.sevenHasSpecial523
        << " (" << (s.sevenTotalCount>0?100.0*s.sevenHasSpecial523/s.sevenTotalCount:0) << "%)\n\n";

    out << "#### 出7时特殊牌总数分布\n";
    out << std::left << std::setw(20) << "特殊牌总数" << std::setw(8) << "次数" << "占比\n";
    out << std::string(35, '-') << "\n";
    for (int i = 0; i <= 7; ++i) {
        if (s.sevenSpecialTotal[i] == 0) continue;
        double pct = s.sevenTotalCount > 0 ? 100.0*s.sevenSpecialTotal[i]/s.sevenTotalCount : 0;
        out << std::left << std::setw(20) << std::to_string(i) + "张"
            << std::setw(8) << s.sevenSpecialTotal[i]
            << std::fixed << std::setprecision(2) << pct << "%\n";
    }
    out << "\n";
}

static void printHandStructure(const FirstMoveStats& s, std::ostream& out) {
    const char* tn[] = {"单张","对子","三张","三带一","三带二","炸弹","王炸"};
    out << "### 1.5 首张选择与手牌结构\n\n";

    auto printSection = [&](const std::string& label, int total,
                            const int typeCounts[7], std::ostream& out) {
        out << "#### 手牌" << label << "（共" << total << "局）\n";
        out << std::left << std::setw(10) << "首张牌型" << std::setw(8) << "次数" << "占比\n";
        out << std::string(28, '-') << "\n";
        for (int i = 0; i < 7; ++i) {
            double p = total > 0 ? 100.0*typeCounts[i]/total : 0;
            out << std::left << std::setw(10) << tn[i]
                << std::setw(8) << typeCounts[i]
                << std::fixed << std::setprecision(2) << p << "%\n";
        }
        out << "\n";
    };

    if (s.handHasRocket > 0) printSection("有王炸", s.handHasRocket, s.rocketFirstType, out);
    if (s.handHasBomb > 0)   printSection("有炸弹", s.handHasBomb, s.bombFirstType, out);
    if (s.handHasS523 > 0)   printSection("有Special523", s.handHasS523, s.s523FirstType, out);
    if (s.handSpecGE3 > 0)   printSection("特殊牌>=3张", s.handSpecGE3, s.specGE3FirstType, out);
}

static void printFinishInOne(const FirstMoveStats& s, std::ostream& out) {
    out << "### 1.6 出完即走命中率\n\n";
    double couldPct = s.totalGames>0 ? 100.0*s.couldFinishCount/s.totalGames : 0;
    double actPct  = s.totalGames>0 ? 100.0*s.actualFinishCount/s.totalGames : 0;
    out << "首张就能一次出完的手牌局数: " << s.couldFinishCount << " (" << std::fixed << std::setprecision(2) << couldPct << "%)\n";
    out << "实际一次出完的局数:         " << s.actualFinishCount << " (" << std::fixed << std::setprecision(2) << actPct << "%)\n\n";
}

static void printCrossTable(const FirstMoveStats allStats[4], std::ostream& out) {
    const char* aiNames[] = {"AI1","AI2","AI3","AI4"};
    out << "## 二、交叉对比\n\n";
    out << std::left
        << std::setw(22) << "维度"
        << std::setw(10) << "AI1" << std::setw(10) << "AI2"
        << std::setw(10) << "AI3" << std::setw(10) << "AI4" << "\n";
    out << std::string(62, '-') << "\n";

    auto fmtCell = [&](double val) {
        std::ostringstream tmp;
        tmp << std::fixed << std::setprecision(2) << val;
        out << std::right << std::setw(9) << tmp.str() << "%";
    };

    auto pct = [&](int n, const FirstMoveStats& s) {
        return s.totalGames > 0 ? 100.0 * n / s.totalGames : 0.0;
    };

    out << std::left << std::setw(22) << "首张出7率";
    for (int i = 0; i < 4; ++i) fmtCell(pct(allStats[i].sevenFirst, allStats[i]));
    out << "\n";
    out << std::left << std::setw(22) << "首张出鬼率";
    for (int i = 0; i < 4; ++i) fmtCell(pct(allStats[i].jokerFirst, allStats[i]));
    out << "\n";
    out << std::left << std::setw(22) << "首张出特殊牌率";
    for (int i = 0; i < 4; ++i) {
        int sp = allStats[i].sevenFirst + allStats[i].jokerFirst + allStats[i].fiveFirst + allStats[i].twoFirst + allStats[i].threeFirst;
        fmtCell(pct(sp, allStats[i]));
    }
    out << "\n";
    out << std::left << std::setw(22) << "首张出单张率";
    for (int i = 0; i < 4; ++i) fmtCell(pct(allStats[i].typeCounts[0], allStats[i]));
    out << "\n";
    out << std::left << std::setw(22) << "首张出对子率";
    for (int i = 0; i < 4; ++i) fmtCell(pct(allStats[i].typeCounts[1], allStats[i]));
    out << "\n";
    out << std::left << std::setw(22) << "首张出炸弹率";
    for (int i = 0; i < 4; ++i) fmtCell(pct(allStats[i].typeCounts[5], allStats[i]));
    out << "\n";
    out << std::left << std::setw(22) << "首张出完即走率";
    for (int i = 0; i < 4; ++i) fmtCell(pct(allStats[i].actualFinishCount, allStats[i]));
    out << "\n";

    out << std::left << std::setw(22) << "平均首张牌力(RANK)";
    for (int i = 0; i < 4; ++i) {
        double avg = allStats[i].singleCount > 0
            ? (double)allStats[i].rankSum / allStats[i].singleCount : 0;
        std::ostringstream tmp;
        tmp << std::fixed << std::setprecision(1) << avg;
        out << std::right << std::setw(10) << tmp.str();
    }
    out << "\n\n";
}

static void printAnomalies(const FirstMoveStats allStats[4], std::ostream& out) {
    out << "## 三、异常样本（最多20条）\n\n";
    out << "条件：先手首张出了7或鬼，但手里有其他更小的单张可出\n\n";

    int count = 0;
    for (int ai = 0; ai < 4; ++ai) {
        for (auto& rec : allStats[ai].anomalies) {
            if (count >= 20) break;
            count++;
            out << "--- 异常 #" << count << " ---\n";
            out << "局号: " << rec.gameIndex << "\n";
            out << "先手AI: " << levelName(rec.firstLevel) << " | 后手AI: " << levelName(rec.secondLevel) << "\n";
            out << "手牌: " << cardsToString(rec.hand) << "\n";
            out << "实际出了: " << cardsToString(rec.firstCards)
                << " (" << cardTypeToString(rec.firstType);
            if (!rec.firstKeyPoint.empty()) out << " " << rec.firstKeyPoint;
            out << ")\n";
            out << "更小的牌可出: " << cardsToString(rec.smallerSingles) << "\n";
            out << "手牌特殊牌: 7×" << rec.sevenCount << " 鬼×" << rec.jokerCount
                << " 5×" << rec.fiveCount << " 2×" << rec.twoCount << " 3×" << rec.threeCount
                << " (共" << rec.specialCardCount << "张)\n";
            out << "\n";
        }
        if (count >= 20) break;
    }
    if (count == 0) out << "（无异常样本）\n\n";
}

// ── 按先手 AI 汇总输出 ──

static void printAllStats(const FirstMoveStats allStats[4], std::ostream& out) {
    const char* aiNames[] = {"AI1","AI2","AI3","AI4"};
    AILevel levels[] = {AILevel::AI1_Simple, AILevel::AI2_Rule,
                        AILevel::AI3_Tracker, AILevel::AI4_Expert};

    // 分别统计每个 AI 做先手时的数据
    FirstMoveStats byFirst[4] = {};
    for (int i = 0; i < 4; ++i) {
        byFirst[i] = allStats[i];
    }

    for (int ai = 0; ai < 4; ++ai) {
        out << "========================================\n";
        out << "先手 AI: " << aiNames[ai] << "（" << byFirst[ai].totalGames << " 局）\n";
        out << "========================================\n\n";
        printTypeDistribution(byFirst[ai], out);
        printPointDistribution(byFirst[ai], out);
        printBigCardFirst(byFirst[ai], out);
        printSevenDetail(byFirst[ai], out);
        printHandStructure(byFirst[ai], out);
        printFinishInOne(byFirst[ai], out);
    }

    printCrossTable(byFirst, out);
    printAnomalies(byFirst, out);
}

// ── 风骚对局日志 ────────────────────────────────────────────

static std::string handToString(const std::vector<Card>& hand) {
    if (hand.empty()) return "(空)";
    std::string r;
    for (size_t i = 0; i < hand.size(); ++i) {
        if (i > 0) r += " ";
        if (hand[i].suit.empty()) r += hand[i].point;
        else r += hand[i].suit + hand[i].point;
    }
    return r;
}

static int parseAILevel(const std::string& s) {
    if (s == "AI1" || s == "ai1" || s == "1") return 1;
    if (s == "AI2" || s == "ai2" || s == "2") return 2;
    if (s == "AI3" || s == "ai3" || s == "3") return 3;
    if (s == "AI4" || s == "ai4" || s == "4") return 4;
    if (s == "AI5" || s == "ai5" || s == "5") return 5;
    if (s == "AI6" || s == "ai6" || s == "6") return 6;
    if (s == "AI7" || s == "ai7" || s == "7") return 7;
    if (s == "AI8" || s == "ai8" || s == "8") return 8;
    if (s == "AI9" || s == "ai9" || s == "9") return 9;
    if (s == "AI10" || s == "ai10" || s == "10") return 10;
    if (s == "AI11" || s == "ai11" || s == "11") return 11;
    if (s == "AI12" || s == "ai12" || s == "12") return 12;
    if (s == "AI13" || s == "ai13" || s == "13") return 13;
    if (s == "AI14" || s == "ai14" || s == "14") return 14;
    if (s == "AI15" || s == "ai15" || s == "15") return 15;
    return 1;
}

static void runOneGameVerbose(int levelFirst, int levelSecond,
                              std::mt19937& rng, std::ostream& log, int gameIndex) {
    Deck deck = createStandardDeck();
    std::shuffle(deck.cards.begin(), deck.cards.end(), rng);

    Player first = createPlayer("P1");
    Player second = createPlayer("P2");

    dealCards(first, deck, 5);
    dealCards(second, deck, 5);

    CardTracker tracker;

    AIEngine engineFirst(buildAIEngineConfig(levelFirst));
    AIEngine engineSecond(buildAIEngineConfig(levelSecond));
    engineFirst.setOpponentHand(second.hand);
    engineSecond.setOpponentHand(first.hand);

    std::string fn = levelName(levelFirst);
    std::string sn = levelName(levelSecond);

    // ── 局头 ──
    log << "========================================\n";
    log << "局 " << std::right << std::setfill('0') << std::setw(3) << gameIndex << "\n";
    log << std::setfill(' ');
    log << "先手: " << fn << " | 后手: " << sn << "\n";
    log << "初始手牌:\n";
    log << "  " << fn << ": " << handToString(first.hand) << "\n";
    log << "  " << sn << ": " << handToString(second.hand) << "\n";
    log << "牌堆: " << deck.cards.size() << "\n";
    log << "========================================\n\n";

    if (checkSpecialVictory(first)) {
        log << "[" << fn << "] 天胡 Special523 获胜！\n\n";
        return;
    }
    if (checkSpecialVictory(second)) {
        log << "[" << sn << "] 天胡 Special523 获胜！\n\n";
        return;
    }

    // ── 游戏循环 ──
    Player* current = &first;
    Player* opponent = &second;
    Player* lastPlayer = nullptr;
    std::vector<Card> tableCards;
    CardTypeResult lastPlay;
    lastPlay.type = CardType::Invalid;
    lastPlay.cards.clear();
    lastPlay.keyPoint.clear();

    int roundNum = 0;
    int safety = 0;

    while (true) {
        safety++;
        if (safety > 500) {
            log << "[安全终止] 超过 500 回合\n\n";
            break;
        }

        roundNum++;
        log << "--- 回合 " << roundNum << " ---\n";

        while (true) {
            int tableScore = calculateScore(tableCards);
            std::vector<Card> play;
            if (current == &first) {
                play = engineFirst.choosePlay(*current, *opponent, lastPlay, deck, tableScore);
            } else {
                play = engineSecond.choosePlay(*current, *opponent, lastPlay, deck, tableScore);
            }

            if (play.empty()) {
                if (lastPlay.type == CardType::Invalid) break;

                // ── 不要 ──
                std::string pn = (current == &first) ? fn : sn;
                log << "[" << pn << "] 不要 (桌面分 " << tableScore << ")";
                log << "  手牌: " << handToString(current->hand) << " (" << current->hand.size() << "张)\n";

                int grabbed = tableScore;
                settleScoreCards(*lastPlayer, tableCards);
                int afterScore = calculateScore(tableCards);
                std::string winner = (lastPlayer == &first) ? fn : sn;
                log << "→ " << winner << " 赢得本回合，得 " << grabbed << " 分";

                // 补牌
                Player* loser = (lastPlayer == &first) ? &second : &first;
                int bLast = (int)lastPlayer->hand.size();
                int bLoser = (int)loser->hand.size();
                refillToFive(*lastPlayer, deck);
                refillToFive(*loser, deck);
                int dLast = (int)lastPlayer->hand.size() - bLast;
                int dLoser = (int)loser->hand.size() - bLoser;

                log << "，补牌：" << winner << " +" << dLast << ", "
                    << ((loser == &first) ? fn : sn) << " +" << dLoser << "\n";
                log << "   " << fn << " 手牌: " << handToString(first.hand)
                    << " (" << first.hand.size() << "张)\n";
                log << "   " << sn << " 手牌: " << handToString(second.hand)
                    << " (" << second.hand.size() << "张)\n";
                log << "   牌堆: " << deck.cards.size() << "\n\n";

                tableCards.clear();
                break;
            }

            CardTypeResult parsed = parseCardType(play);
            if (parsed.type == CardType::Invalid) break;
            if (lastPlay.type != CardType::Invalid && !canBeat(parsed, lastPlay)) break;

            // ── 出牌 ──
            std::string pn = (current == &first) ? fn : sn;
            int beforeScore = calculateScore(tableCards);

            // 计算压分
            int pressureBonus = 0;
            if (lastPlay.type != CardType::Invalid)
                pressureBonus = calculatePressureBonus(parsed, lastPlay);

            // 从手牌移除
            for (const Card& c : play) {
                auto it = std::find_if(current->hand.begin(), current->hand.end(),
                    [&](const Card& h) { return h.point == c.point && h.suit == c.suit; });
                if (it != current->hand.end()) current->hand.erase(it);
            }
            for (const Card& c : play) tableCards.push_back(c);

            int afterScore = calculateScore(tableCards);

            log << "[" << pn << "] 出: " << cardTypeToString(parsed.type);
            if (!parsed.keyPoint.empty()) log << " " << parsed.keyPoint;
            log << " [" << handToString(play) << "]"
                << " (桌面分 " << beforeScore << " → " << afterScore;
            if (pressureBonus > 0) log << ", 压分 +" << pressureBonus;
            log << ")"
                << "  手牌: " << current->hand.size() << "张"
                << " | 对手: " << opponent->hand.size() << "张"
                << " | 牌堆: " << deck.cards.size() << "\n";

            tracker.recordPlayed(play);
            lastPlay = parsed;
            lastPlayer = current;

            // Special523 检查
            if (checkSpecialVictory(*current)) {
                log << "[" << pn << "] Special523 获胜！\n\n";
                return;
            }

            // 手牌空 → 检查终局
            if (current->hand.empty()) {
                if (deck.cards.empty()) {
                    settleScoreCards(*current, tableCards);
                    settleScoreCards(*current, opponent->hand);
                    opponent->hand.clear();
                    tableCards.clear();
                    log << "[" << pn << "] 手牌空，终局\n";
                    log << "→ " << pn << " 赢得终局，拿桌面分 + 对手手牌分\n";
                    log << "   最终分：" << fn << " = " << first.totalScore
                        << ", " << sn << " = " << second.totalScore << "\n";
                    log << "   胜者：" << (first.totalScore > second.totalScore ? fn :
                        (second.totalScore > first.totalScore ? sn : "平局")) << "\n\n";
                    return;
                }
            }

            std::swap(current, opponent);
        }

        // ── 回合结束 ──
        lastPlay.type = CardType::Invalid;
        lastPlay.cards.clear();
        lastPlay.keyPoint.clear();

        if (checkSpecialVictory(first)) {
            log << "[" << fn << "] Special523 获胜！\n\n";
            return;
        }
        if (checkSpecialVictory(second)) {
            log << "[" << sn << "] Special523 获胜！\n\n";
            return;
        }

        if (!deck.cards.empty()) {
            Player* loser = (lastPlayer == &first) ? &second : &first;
            refillToFive(*lastPlayer, deck);
            refillToFive(*loser, deck);
        }

        current = lastPlayer;
        opponent = (current == &first) ? &second : &first;

        if (deck.cards.empty() && first.hand.empty() && second.hand.empty()) break;
    }

    // ── 终局 ──
    log << "终局：牌堆空，双方手牌空\n";
    log << "   最终分：" << fn << " = " << first.totalScore
        << ", " << sn << " = " << second.totalScore << "\n";
    log << "   胜者：" << (first.totalScore > second.totalScore ? fn :
        (second.totalScore > first.totalScore ? sn : "平局")) << "\n\n";
}

// ── 主函数 ──────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    // ── 解析命令行 ──
    bool verbose = false;
    int gamesPerCombo = 100;
    int verboseGames = 20;
    int firstAI = 7;
    int secondAI = 6;
    int maxLevel = 4;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--verbose" || arg == "-v") {
            verbose = true;
        } else if (arg == "--games" || arg == "-g") {
            if (i + 1 < argc) verboseGames = std::atoi(argv[++i]);
        } else if (arg == "--first" || arg == "-1") {
            if (i + 1 < argc) firstAI = parseAILevel(argv[++i]);
        } else if (arg == "--second" || arg == "-2") {
            if (i + 1 < argc) secondAI = parseAILevel(argv[++i]);
        } else if (arg == "--max-level" || arg == "-m") {
            if (i + 1 < argc) maxLevel = std::atoi(argv[++i]);
        } else if (arg[0] != '-') {
            // 老模式：第一个数字参数 = gamesPerCombo
            gamesPerCombo = std::atoi(argv[i]);
        }
    }

    maxLevel = std::max(1, std::min(15, maxLevel));

    // ── 风骚日志模式 ──
    if (verbose) {
        std::random_device rd;
        std::mt19937 rng(rd());

        std::string fn = levelName(firstAI);
        std::string sn = levelName(secondAI);

        // 直接写文件，避免 shell 重定向的编码问题
        std::string logFile = "verbose_log.txt";
        std::ofstream vofs(logFile);
        if (!vofs) {
            std::cerr << "无法创建日志文件: " << logFile << "\n";
            return 1;
        }

        std::cerr << "风骚日志模式: " << fn << " vs " << sn
                  << ", " << verboseGames << " 局 -> " << logFile << "\n";

        for (int i = 0; i < verboseGames; ++i) {
            runOneGameVerbose(firstAI, secondAI, rng, vofs, i + 1);
        }

        vofs.close();
        std::cerr << "日志已写入 " << logFile << "\n";
        return 0;
    }

    // ── 批量对战模式 ──
    std::vector<int> levels;
    for (int i = 1; i <= maxLevel; ++i) levels.push_back(i);

    std::random_device rd;
    std::mt19937 rng(rd());

    int numLevels = (int)levels.size();
    int totalGames = numLevels * numLevels * gamesPerCombo;

    // 同时输出到屏幕和文件
    std::ofstream ofs("battle_result.txt");
    auto output = [&](const std::string& s) {
        std::cout << s;
        ofs << s;
    };

    {
        std::ostringstream oss;
        oss << "========================================\n"
            << numLevels << " 档 AI 全组合对战测试 + 先手首张观测\n"
            << numLevels * numLevels << " 种组合，每种 " << gamesPerCombo << " 局\n"
            << "总计 " << totalGames << " 局\n"
            << "========================================\n\n";
        output(oss.str());
    }

    // battle stats
    std::vector<std::vector<BattleStats>> allStats(numLevels, std::vector<BattleStats>(numLevels));
    // first move stats, indexed by first player AI level
    std::vector<FirstMoveStats> fmStats(numLevels);

    int combo = 0;
    int totalCombos = numLevels * numLevels;
    for (int li = 0; li < numLevels; ++li) {
        for (int lj = 0; lj < numLevels; ++lj) {
            int lf = levels[li];
            int ls = levels[lj];
            BattleStats s;
            for (int i = 0; i < gamesPerCombo; ++i) {
                int gameIndex = combo * gamesPerCombo + i;
                runOneGame(lf, ls, rng, s, fmStats[li], gameIndex);
            }
            allStats[li][lj] = s;

            double wr   = (double)s.firstWins / gamesPerCombo;
            double avg1 = (double)s.firstScoreSum / gamesPerCombo;
            double avg2 = (double)s.secondScoreSum / gamesPerCombo;

            std::ostringstream oss;
            oss << std::left
                << std::setw(8)  << levelName(lf)
                << std::setw(8)  << levelName(ls)
                << std::setw(8)  << s.firstWins
                << std::setw(8)  << s.secondWins
                << std::setw(8)  << s.draws
                << std::setw(10) << std::fixed << std::setprecision(3) << wr
                << std::setw(12) << std::fixed << std::setprecision(1) << avg1
                << std::setw(12) << std::fixed << std::setprecision(1) << avg2
                << "\n";
            output(oss.str());
            std::cerr << "[" << combo+1 << "/" << totalCombos << "] " << levelName(lf)
                      << " vs " << levelName(ls) << " 完成 ("
                      << fmStats[li].totalGames << " 局记录)\n";
            combo++;
        }
    }

    output("\n\n");

    // 打印详细观测
    std::ostringstream detail;
    try {
        // 转换为数组格式以兼容 printAllStats
        std::vector<FirstMoveStats> fmArray(numLevels);
        for (int i = 0; i < numLevels; ++i) fmArray[i] = fmStats[i];
        printAllStats(fmArray.data(), detail);
    } catch (const std::exception& e) {
        std::cerr << "\n!!! printAllStats 崩溃: " << e.what() << "\n";
        detail << "\n[printAllStats 崩溃: " << e.what() << "]\n";
    }
    output(detail.str());

    output("\n完成。\n");
    ofs.close();
    std::cout << "结果已写入 battle_result.txt\n";
    return 0;
}