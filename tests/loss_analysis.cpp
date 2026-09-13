#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <algorithm>
#include <numeric>
#include <random>
#include <chrono>
#include <cstring>
#include <cassert>
#include <cstdlib>

#include "core/ai.h"
#include "core/ai_params.h"
#include "core/deck.h"
#include "core/player.h"
#include "core/cardtype.h"
#include "core/cardtracker.h"
#include "core/score.h"
#include "core/special.h"

// ============================================================
// 数据结构定义
// ============================================================

/// 单回合事件（AI4 出牌或不要）
struct TurnEvent {
    int turn;                             // 第几回合（从 1 开始）
    bool isAI4Turn;                       // AI4 的回合？
    std::vector<Card> played;             // 出的牌（空=不要）
    CardTypeResult parsed;                // 牌型解析结果
    CardTypeResult lastPlayBeforeTurn;    // 本回合开始时的 lastPlay（Invalid=自由出牌）
    int tableScoreBefore;                 // 出牌前桌面分
    int tableScoreAfter;                  // 出牌后桌面分
    int ai4HandSize;                      // AI4 当时手牌数
    int oppHandSize;                      // 对手当时手牌数
    int deckRemaining;                    // 牌堆剩余
    bool isBomb;                          // 是否是炸弹/王炸
    bool isBigCard;                       // 是否是 7/大鬼/小鬼
    std::string bigCardPoint;             // 大牌的点数
    std::string opponentName;
    std::vector<Card> handBefore;               // AI4 回合开始时的完整手牌
};

/// 终局事件
struct FinalEvent {
    int ai4Score = 0;
    int oppScore = 0;
    bool ai4Special = false;
    bool oppSpecial = false;
    bool ai4FinishedFirst = false;
    std::vector<Card> ai4FinalHand;
    std::vector<Card> oppFinalHand;
    std::vector<Card> tableCards;
    int totalTurns = 0;                   // 总回合数
};

/// 一局完整追踪
struct GameTrace {
    int gameId = 0;
    int seed = 0;
    bool ai4First = true;                 // AI4 是否先手
    AILevel opponentLevel = AILevel::AI3_Tracker;

    // 胜负
    int ai4Score = 0;
    int oppScore = 0;
    std::string result;                   // "WIN" / "LOSE" / "TIE"
    bool finalPhase = false;              // 是否进入终局阶段（牌堆空后）

    // 事件
    std::vector<TurnEvent> turns;
    FinalEvent final;

    // 败因（仅输局有意义）
    std::string mainCause;                // 主因枚举名
    std::vector<std::string> secondaryTags;
};

// ============================================================
// 败因分类枚举
// ============================================================

static const char* MAIN_CAUSE_NAMES[] = {
    "BIG_CARD_EARLY",
    "BOMB_WASTED",
    "TRUE_ABANDON",
    "FORCED_PASS",
    "ACTIVE_SPLIT",
    "FORCED_SPLIT",
    "HAD_CHANCE",
    "NEVER_HAD_CHANCE",
    "HAD_BOMB",
    "NEVER_HAD_BOMB",
    "CLOSE_LOSS",
    "OTHER"
};
enum MainCause {
    BIG_CARD_EARLY = 0,
    BOMB_WASTED,
    TRUE_ABANDON,
    FORCED_PASS,
    ACTIVE_SPLIT,
    FORCED_SPLIT,
    HAD_CHANCE,
    NEVER_HAD_CHANCE,
    HAD_BOMB,
    NEVER_HAD_BOMB,
    CLOSE_LOSS,
    OTHER,
    MAIN_CAUSE_COUNT
};

static const char* TIE_CAUSE_NAMES[] = {
    "TIE_BOTH_EMPTY",
    "TIE_SCORE_EQUAL",
    "TIE_SPECIAL_BOTH"
};
enum TieCause {
    TIE_BOTH_EMPTY = 0,
    TIE_SCORE_EQUAL,
    TIE_SPECIAL_BOTH,
    TIE_CAUSE_COUNT
};

static const char* WIN_FEATURE_NAMES[] = {
    "WIN_BIG_MARGIN",
    "WIN_CLOSE",
    "WIN_SPECIAL",
    "WIN_FINISH_FIRST"
};
enum WinFeature {
    WIN_BIG_MARGIN = 0,
    WIN_CLOSE,
    WIN_SPECIAL,
    WIN_FINISH_FIRST,
    WIN_FEATURE_COUNT
};

// ============================================================
// 辅助函数
// ============================================================

static int cardScore(const Card& c) { return c.score; }

static bool isBigCard(const Card& c) {
    return c.point == "7" || c.point == "大鬼" || c.point == "小鬼";
}

static bool isSpecialCard(const Card& c) {
    static const std::set<std::string> special = {"7", "大鬼", "小鬼", "5", "2", "3"};
    return special.count(c.point) > 0;
}

static std::string pointToString(const std::vector<Card>& cards) {
    std::ostringstream oss;
    for (size_t i = 0; i < cards.size(); ++i) {
        if (i > 0) oss << " ";
        oss << cards[i].point;
        if (!cards[i].suit.empty()) oss << cards[i].suit;
    }
    return oss.str();
}

static std::string handToString(const std::vector<Card>& cards) {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < cards.size(); ++i) {
        if (i > 0) oss << " ";
        oss << cards[i].point;
        if (!cards[i].suit.empty()) oss << cards[i].suit;
        if (cards[i].score > 0) oss << "(" << cards[i].score << ")";
    }
    oss << "]";
    return oss.str();
}

// ============================================================
// 分类函数
// ============================================================

// 检查是否拆对/拆三张
static bool checkSplitCombo(const TurnEvent& te, const Player& ai4BeforePlay) {
    if (te.played.empty() || te.parsed.type == CardType::Invalid) return false;
    // 只对出单张的情况检测拆牌
    if (te.parsed.type != CardType::Single) return false;
    // 检查该点数在手牌中是否至少有两张
    std::string point = te.parsed.keyPoint;
    int inHand = 0;
    for (const Card& c : ai4BeforePlay.hand) {
        if (c.point == point) inHand++;
    }
    int inPlay = (int)te.played.size(); // 对于单张，inPlay == 1
    // 如果手牌中该点数有 >= 2 张但只出了 1 张，说明拆了对子
    if (inHand >= 2 && inPlay == 1 && inHand - inPlay >= 1) return true;
    return false;
}

/// 主因分类：根据完整 trace 判定输局的主因（优先级从高到低）
static MainCause classifyMainCause(const GameTrace& trace) {
    const auto& turns = trace.turns;
    const auto& final = trace.final;

    // 1. BIG_CARD_EARLY：前 3 回合内出过 7/大鬼/小鬼
    for (const auto& te : turns) {
        if (te.turn <= 3 && te.isAI4Turn && !te.played.empty() && te.isBigCard) {
            return BIG_CARD_EARLY;
        }
    }

    // 2. BOMB_WASTED：对手手牌 >= 4 时使用炸弹
    for (const auto& te : turns) {
        if (te.isAI4Turn && te.isBomb && te.oppHandSize >= 4) {
            return BOMB_WASTED;
        }
    }

    // 3. TRUE_ABANDON / FORCED_PASS：桌面分 >= 20 时 AI4 选择不要
    //    先判断对手最后一手是否容易被压，再检查 AI4 实际有无可压的牌
    for (const auto& te : turns) {
        if (!te.isAI4Turn || !te.played.empty()) continue;
        if (te.tableScoreBefore < 20 || te.lastPlayBeforeTurn.type == CardType::Invalid)
            continue;
        auto itk = RANK_MAP.find(te.lastPlayBeforeTurn.keyPoint);
        int oppRank = (itk != RANK_MAP.end()) ? itk->second : 0;
        bool beatableSingle = (te.lastPlayBeforeTurn.type == CardType::Single && oppRank <= 8);
        bool beatablePair = (te.lastPlayBeforeTurn.type == CardType::Pair && oppRank <= 8);
        if (!beatableSingle && !beatablePair) continue;

        // 检查 AI4 当时手牌是否有能压过的牌
        Player temp;
        temp.hand = te.handBefore;
        auto allPlays = enumerateLegalPlays(temp);
        bool couldBeat = false;
        for (const auto& p : allPlays) {
            if (canBeat(parseCardType(p), te.lastPlayBeforeTurn)) {
                couldBeat = true;
                break;
            }
        }
        return couldBeat ? TRUE_ABANDON : FORCED_PASS;
    }


    // 4. ACTIVE_SPLIT / FORCED_SPLIT
    {
        // 扫描每个回合，找"手牌中有对子/三张但选择拆单张"的回合
        for (const auto& te : turns) {
            if (!te.isAI4Turn || te.played.empty()) continue;
            if (te.parsed.type != CardType::Single) continue;

            // 检查当前回合的手牌中，AI4 打的这个点数是否有 ≥2 张
            int inHandCount = 0;
            for (const Card& hc : te.handBefore)
                if (hc.point == te.parsed.keyPoint) inHandCount++;
            if (inHandCount < 2) continue;  // 手里只有 1 张，不是拆对

            // 这是一个真正的拆对/三张回合
            Player temp;
            temp.hand = te.handBefore;
            auto allPlays = enumerateLegalPlays(temp);
            bool hasNonSplit = false;
            for (const auto& p : allPlays) {
                std::map<std::string, int> pc;
                for (const Card& c : p) pc[c.point]++;
                bool nonSplit = true;
                for (const auto& kv2 : pc) {
                    int inHand = 0;
                    for (const Card& hc : te.handBefore)
                        if (hc.point == kv2.first) inHand++;
                    if (inHand >= 2 && kv2.second == 1) { nonSplit = false; break; }
                    if (inHand >= 3 && kv2.second < inHand && kv2.second < 3) { nonSplit = false; break; }
                }
                if (nonSplit) {
                    auto parsed = parseCardType(p);
                    if (parsed.type != CardType::Single &&
                        (te.lastPlayBeforeTurn.type == CardType::Invalid ||
                         canBeat(parsed, te.lastPlayBeforeTurn))) {
                        hasNonSplit = true; break;
                    }
                }
            }
            return hasNonSplit ? ACTIVE_SPLIT : FORCED_SPLIT;
        }
    }

    // 5. HAD_CHANCE / NEVER_HAD_CHANCE
    {
        bool everHadChance = false;
        for (const auto& te : turns) {
            if (!te.isAI4Turn || te.handBefore.empty()) continue;
            std::set<std::string> special = {"7", "大鬼", "小鬼", "5", "2", "3"};
            for (const Card& c : te.handBefore) special.erase(c.point);
            if ((int)special.size() <= 2) { everHadChance = true; break; }
        }
        std::set<std::string> needed = {"7", "大鬼", "小鬼", "5", "2", "3"};
        for (const Card& c : final.ai4FinalHand) needed.erase(c.point);
        for (const auto& te : turns)
            if (te.isAI4Turn)
                for (const Card& c : te.played) needed.erase(c.point);
        if ((int)needed.size() <= 1)
            return everHadChance ? HAD_CHANCE : NEVER_HAD_CHANCE;
    }

    // 6. HAD_BOMB / NEVER_HAD_BOMB
    if (trace.finalPhase) {
        bool everHadBomb = false;
        for (const auto& te : turns) {
            if (!te.isAI4Turn || te.handBefore.empty()) continue;
            std::map<std::string, int> counts;
            for (const Card& c : te.handBefore) counts[c.point]++;
            for (const auto& kv : counts)
                if (kv.second >= 4) { everHadBomb = true; break; }
            if (everHadBomb) break;
            bool hasBig = false, hasSmall = false;
            for (const Card& c : te.handBefore) {
                if (c.point == "大鬼") hasBig = true;
                if (c.point == "小鬼") hasSmall = true;
            }
            if (hasBig && hasSmall) { everHadBomb = true; break; }
        }
        return everHadBomb ? HAD_BOMB : NEVER_HAD_BOMB;
    }

    // 7. CLOSE_LOSS：分差 <= 5
    if (std::abs(trace.ai4Score - trace.oppScore) <= 5) {
        return CLOSE_LOSS;
    }

    return OTHER;
}

/// 收集次要标签
static std::vector<std::string> collectSecondaryTags(const GameTrace& trace) {
    std::vector<std::string> tags;
    const auto& turns = trace.turns;

    // 是否有出完
    bool finished = false;
    for (const auto& te : turns) {
        if (te.isAI4Turn && te.played.size() > 0) {
            // 检查出完后手牌是否为空（从后续事件判断）
        }
    }
    if (trace.final.ai4FinishedFirst && trace.result == "WIN") tags.push_back("FINISHED_FIRST");
    if (trace.final.ai4Special) tags.push_back("SPECIAL_VICTORY");

    // 是否有过炸弹使用
    for (const auto& te : turns) {
        if (te.isAI4Turn && te.isBomb) {
            tags.push_back("USED_BOMB");
            break;
        }
    }

    // 对手特殊胜利
    if (trace.final.oppSpecial) tags.push_back("OPP_SPECIAL");

    return tags;
}

/// 平局原因
static std::string classifyTieCause(const GameTrace& trace) {
    if (trace.final.ai4Special && trace.final.oppSpecial) return "TIE_SPECIAL_BOTH";
    if (trace.final.ai4FinishedFirst && trace.final.oppFinalHand.empty()) return "TIE_BOTH_EMPTY";
    return "TIE_SCORE_EQUAL";
}

/// 胜局特征
static std::vector<std::string> classifyWinFeatures(const GameTrace& trace) {
    std::vector<std::string> features;
    int diff = trace.ai4Score - trace.oppScore;
    if (diff >= 20) features.push_back("WIN_BIG_MARGIN");
    if (diff <= 5 && diff >= -5) features.push_back("WIN_CLOSE");
    if (trace.final.ai4Special) features.push_back("WIN_SPECIAL");
    if (trace.final.ai4FinishedFirst) features.push_back("WIN_FINISH_FIRST");
    return features;
}

// ============================================================
// 单局运行 + 追踪
// ============================================================

static GameTrace runOneTracedGame(
    int gameId, int seed, bool ai4First, AILevel opponentLevel,
    std::mt19937& rng, bool verbose)
{
    GameTrace trace;
    trace.gameId = gameId;
    trace.seed = seed;
    trace.ai4First = ai4First;
    trace.opponentLevel = opponentLevel;

    // 创建牌堆
    Deck deck = createStandardDeck();
    std::shuffle(deck.cards.begin(), deck.cards.end(), rng);

    // 创建玩家
    AILevel ai4Level = AILevel::AI4_Expert;
    Player first, second;
    if (ai4First) {
        first = createPlayer("AI4");  first.aiLevel = ai4Level;
        second = createPlayer("OPP"); second.aiLevel = opponentLevel;
    } else {
        first = createPlayer("OPP");  first.aiLevel = opponentLevel;
        second = createPlayer("AI4"); second.aiLevel = ai4Level;
    }
    Player* ai4 = ai4First ? &first : &second;
    Player* opp = ai4First ? &second : &first;

    dealCards(first, deck, 5);
    dealCards(second, deck, 5);

    // 检查特殊胜利
    if (checkSpecialVictory(*ai4)) {
        trace.ai4Score = ai4->totalScore;
        trace.oppScore = opp->totalScore;
        trace.result = "WIN";
        trace.final.ai4Special = true;
        trace.final.ai4FinishedFirst = true;
        trace.final.ai4FinalHand = ai4->hand;
        trace.final.oppFinalHand = opp->hand;
        if (verbose) std::cout << "  #" << gameId << " AI4 开局特殊胜利!\n";
        return trace;
    }
    if (checkSpecialVictory(*opp)) {
        trace.ai4Score = ai4->totalScore;
        trace.oppScore = opp->totalScore;
        trace.result = "LOSE";
        trace.final.oppSpecial = true;
        trace.final.ai4FinalHand = ai4->hand;
        trace.final.oppFinalHand = opp->hand;
        if (verbose) std::cout << "  #" << gameId << " 对手开局特殊胜利!\n";
        return trace;
    }

    CardTracker tracker;
    Player* current = &first;
    Player* opponent = &second;
    Player* lastPlayer = nullptr;
    std::vector<Card> tableCards;
    CardTypeResult lastPlay;
    lastPlay.type = CardType::Invalid;
    lastPlay.cards.clear();
    lastPlay.keyPoint.clear();

    int turnCounter = 0;
    int safety = 0;
    const int MAX_SAFETY = 1000;
    bool finalPhase = false;

    while (true) {
        safety++;
        if (safety > MAX_SAFETY) break;

        // 开始新回合
        while (true) {
            int tableScore = calculateScore(tableCards);
            turnCounter++;

            TurnEvent te;
            te.turn = turnCounter;
            te.isAI4Turn = (current == ai4);
            te.lastPlayBeforeTurn = lastPlay;
            te.tableScoreBefore = tableScore;
            te.ai4HandSize = (int)current->hand.size();
            te.handBefore = current->hand;
            te.oppHandSize = (int)opponent->hand.size();
            te.deckRemaining = (int)deck.cards.size();
            te.opponentName = opponent->name;

            // AI 出牌
            std::vector<Card> play = aiChoosePlay(
                *current, *opponent, lastPlay, deck, tableScore, tracker);

            te.played = play;

            if (play.empty()) {
                if (lastPlay.type == CardType::Invalid) {
                    // 先手无牌可出
                    if (current->hand.empty()) {
                        // 手牌为空 → 出完终局
                        if (deck.cards.empty() || finalPhase) {
                            settleScoreCards(*current, tableCards);
                            settleScoreCards(*current, opponent->hand);
                            opponent->hand.clear();
                            tableCards.clear();
                            if (current == ai4) trace.final.ai4FinishedFirst = true;
                            trace.ai4Score = ai4->totalScore;
                            trace.oppScore = opp->totalScore;
                            trace.final.ai4FinalHand = ai4->hand;
                            trace.final.oppFinalHand = opp->hand;
                            trace.final.tableCards = tableCards;
                            trace.final.totalTurns = turnCounter;
                            if (ai4->totalScore > opp->totalScore) trace.result = "WIN";
                            else if (opp->totalScore > ai4->totalScore) trace.result = "LOSE";
                            else trace.result = "TIE";
                            return trace;
                        }
                        break;
                    }
                    // 有牌但 AI 返回空 → 自动出第一张
                    play = {current->hand[0]};
                    te.played = play;
                }
                // 不要（非先手或先手出完终局）
                if (play.empty() && lastPlay.type != CardType::Invalid) {
                    settleScoreCards(*lastPlayer, tableCards);
                    tableCards.clear();
                    tableScore = 0;
                    te.tableScoreAfter = 0;
                    trace.turns.push_back(te);
                    break;
                }
                // 先手空且已处理（出完终局 return 或自动出牌），继续走下面逻辑
                if (play.empty()) break;
            }

            CardTypeResult parsed = parseCardType(play);
            if (parsed.type == CardType::Invalid) break;
            if (lastPlay.type != CardType::Invalid && !canBeat(parsed, lastPlay)) break;

            // 验证手牌
            for (const Card& c : play) {
                auto it = std::find_if(current->hand.begin(), current->hand.end(),
                    [&](const Card& h){ return h.point == c.point && h.suit == c.suit; });
                if (it != current->hand.end()) current->hand.erase(it);
            }
            for (const Card& c : play) tableCards.push_back(c);
            tracker.recordPlayed(play);

            te.parsed = parsed;
            te.isBomb = (parsed.type == CardType::Bomb || parsed.type == CardType::Rocket);
            te.isBigCard = false;
            for (const Card& c : play) {
                if (isBigCard(c)) {
                    te.isBigCard = true;
                    te.bigCardPoint = c.point;
                    break;
                }
            }
            te.tableScoreAfter = calculateScore(tableCards);

            trace.turns.push_back(te);

            lastPlay = parsed;
            lastPlayer = current;

            // 检查特殊胜利
            if (checkSpecialVictory(*current)) {
                if (current == ai4) {
                    trace.ai4Score = calculateScore(ai4->collected);
                    trace.oppScore = calculateScore(opp->collected);
                    trace.result = "WIN";
                    trace.final.ai4Special = true;
                    trace.final.ai4FinishedFirst = true;
                } else {
                    trace.ai4Score = calculateScore(ai4->collected);
                    trace.oppScore = calculateScore(opp->collected);
                    trace.result = "LOSE";
                    trace.final.oppSpecial = true;
                }
                trace.final.ai4FinalHand = ai4->hand;
                trace.final.oppFinalHand = opp->hand;
                trace.final.totalTurns = turnCounter;
                if (verbose) {
                    std::cout << "  #" << gameId << " 特殊胜利: "
                              << (current == ai4 ? "AI4" : "对手") << "\n";
                }
                return trace;
            }

            // 出完检查
            if (current->hand.empty()) {
                if (deck.cards.empty() || finalPhase) {
                    // 终局结算
                    settleScoreCards(*current, tableCards);
                    settleScoreCards(*current, opponent->hand);
                    opponent->hand.clear();
                    tableCards.clear();

                    if (current == ai4) trace.final.ai4FinishedFirst = true;

                    trace.ai4Score = ai4->totalScore;
                    trace.oppScore = opp->totalScore;
                    trace.final.ai4FinalHand = ai4->hand;
                    trace.final.oppFinalHand = opp->hand;
                    trace.final.tableCards = tableCards;
                    trace.final.totalTurns = turnCounter;

                    if (ai4->totalScore > opp->totalScore) trace.result = "WIN";
                    else if (opp->totalScore > ai4->totalScore) trace.result = "LOSE";
                    else trace.result = "TIE";

                    if (verbose) {
                        std::cout << "  #" << gameId << " 终局: AI4 "
                                  << ai4->totalScore << " vs " << opp->totalScore
                                  << " (" << trace.result << ")\n";
                    }
                    return trace;
                }
                // 牌堆还有牌，继续
            }

            std::swap(current, opponent);
        }

        // 回合结束
        lastPlay.type = CardType::Invalid;
        lastPlay.cards.clear();
        lastPlay.keyPoint.clear();

        // 检查是否进入终局阶段
        if (deck.cards.empty()) {
            finalPhase = true;
            trace.finalPhase = true;
        }

        if (finalPhase) {
            // 终局阶段：不补牌，检查是否有人手牌已空
            if (lastPlayer && lastPlayer->hand.empty()) {
                Player* finisher = lastPlayer;
                Player* other = (finisher == &first) ? &second : &first;
                settleScoreCards(*finisher, tableCards);
                settleScoreCards(*finisher, other->hand);
                other->hand.clear();
                tableCards.clear();

                if (finisher == ai4) trace.final.ai4FinishedFirst = true;
                trace.ai4Score = ai4->totalScore;
                trace.oppScore = opp->totalScore;
                trace.final.ai4FinalHand = ai4->hand;
                trace.final.oppFinalHand = opp->hand;
                trace.final.tableCards = tableCards;
                trace.final.totalTurns = turnCounter;
                if (ai4->totalScore > opp->totalScore) trace.result = "WIN";
                else if (opp->totalScore > ai4->totalScore) trace.result = "LOSE";
                else trace.result = "TIE";
                if (verbose) {
                    std::cout << "  #" << gameId << " 终局: AI4 "
                              << ai4->totalScore << " vs " << opp->totalScore
                              << " (" << trace.result << ")\n";
                }
                return trace;
            }
            if (lastPlayer) {
                Player* other = (lastPlayer == &first) ? &second : &first;
                if (other->hand.empty()) {
                    Player* finisher = other;
                    settleScoreCards(*finisher, tableCards);
                    settleScoreCards(*finisher, lastPlayer->hand);
                    lastPlayer->hand.clear();
                    tableCards.clear();

                    if (finisher == ai4) trace.final.ai4FinishedFirst = true;
                    trace.ai4Score = ai4->totalScore;
                    trace.oppScore = opp->totalScore;
                    trace.final.ai4FinalHand = ai4->hand;
                    trace.final.oppFinalHand = opp->hand;
                    trace.final.tableCards = tableCards;
                    trace.final.totalTurns = turnCounter;
                    if (ai4->totalScore > opp->totalScore) trace.result = "WIN";
                    else if (opp->totalScore > ai4->totalScore) trace.result = "LOSE";
                    else trace.result = "TIE";
                    if (verbose) {
                        std::cout << "  #" << gameId << " 终局: AI4 "
                                  << ai4->totalScore << " vs " << opp->totalScore
                                  << " (" << trace.result << ")\n";
                    }
                    return trace;
                }
            }
        } else {
            // 正常补牌
            if (!deck.cards.empty()) {
                if (lastPlayer) {
                    Player* other = (lastPlayer == &first) ? &second : &first;
                    refillToFive(*lastPlayer, deck);
                    refillToFive(*other, deck);
                } else {
                    refillToFive(first, deck);
                    refillToFive(second, deck);
                }
                if (deck.cards.empty()) {
                    finalPhase = true;
                }
            }
        }

        if (lastPlayer) {
            current = lastPlayer;
            opponent = (current == &first) ? &second : &first;
        } else {
            current = &first;
            opponent = &second;
        }

        if (finalPhase && (first.hand.empty() || second.hand.empty())) {
            // 终局阶段双方手牌都已检查过，这里作为安全兜底
            break;
        }
    }

    // 游戏自然结束
    trace.ai4Score = ai4->totalScore;
    trace.oppScore = opp->totalScore;
    trace.final.ai4FinalHand = ai4->hand;
    trace.final.oppFinalHand = opp->hand;
    trace.final.tableCards = tableCards;
    trace.final.totalTurns = turnCounter;

    if (ai4->totalScore > opp->totalScore) trace.result = "WIN";
    else if (opp->totalScore > ai4->totalScore) trace.result = "LOSE";
    else trace.result = "TIE";

    if (verbose) {
        std::cout << "  #" << gameId << " 终局: AI4 "
                  << ai4->totalScore << " vs " << opp->totalScore
                  << " (" << trace.result << ")\n";
    }
    return trace;
}

// ============================================================
// 命令行参数解析
// ============================================================

struct Options {
    int games = 500;
    int seed = 20260913;
    bool ai4First = false;   // false = 随机，true = 固定先手
    bool ai4Second = false;  // true = 固定后手
    bool fixedFirst = false; // 是否固定位置
    bool randomOrder = true;  // 随机先后手
    AILevel opponent = AILevel::AI3_Tracker;
    bool verbose = false;
    bool dumpActiveSplit = false;
    bool dumpActiveSplitOnly = false;
    std::string csvPath = "loss_analysis.csv";
    std::string logPath = "loss_analysis.txt";
    std::string activeSplitPath = "active_splits.csv";
};

static AILevel parseOpponent(const std::string& s) {
    if (s == "AI1" || s == "ai1") return AILevel::AI1_Simple;
    if (s == "AI2" || s == "ai2") return AILevel::AI2_Rule;
    if (s == "AI3" || s == "ai3") return AILevel::AI3_Tracker;
    if (s == "AI4" || s == "ai4") return AILevel::AI4_Expert;
    std::cerr << "未知对手等级: " << s << "，使用 AI3\n";
    return AILevel::AI3_Tracker;
}

static Options parseOptions(int argc, char* argv[]) {
    Options opts;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--games" && i + 1 < argc) {
            opts.games = std::atoi(argv[++i]);
        } else if (arg == "--seed" && i + 1 < argc) {
            opts.seed = std::atoi(argv[++i]);
        } else if (arg == "--ai4-first") {
            opts.ai4First = true;
            opts.fixedFirst = true;
            opts.randomOrder = false;
        } else if (arg == "--ai4-second") {
            opts.ai4Second = true;
            opts.fixedFirst = true;
            opts.randomOrder = false;
        } else if (arg == "--opponent" && i + 1 < argc) {
            opts.opponent = parseOpponent(argv[++i]);
        } else if (arg == "--verbose") {
            opts.verbose = true;
        } else if (arg == "--dump-active-split") {
            opts.dumpActiveSplit = true;
        } else if (arg == "--dump-active-split-only") {
            opts.dumpActiveSplitOnly = true;
        } else if (arg == "--csv" && i + 1 < argc) {
            opts.csvPath = argv[++i];
        } else if (arg == "--log" && i + 1 < argc) {
            opts.logPath = argv[++i];
        } else {
            std::cerr << "未知选项: " << arg << "\n";
        }
    }
    return opts;
}

// ============================================================
// 统计数据结构
// ============================================================

struct LossStats {
    int totalGames = 0;
    int wins = 0;
    int losses = 0;
    int ties = 0;
    long long ai4ScoreSum = 0;
    long long oppScoreSum = 0;
    long long firstScoreDiffSum = 0;   // AI4 先手时的分差
    long long secondScoreDiffSum = 0;  // AI4 后手时的分差
    int firstCount = 0;
    int secondCount = 0;

    // 败因计数
    int lossCauseCount[MAIN_CAUSE_COUNT] = {0};
    // 败因样本（每类最多 3 局）
    std::vector<GameTrace> lossSamples[MAIN_CAUSE_COUNT];

    // 平局原因计数
    int tieCauseCount[TIE_CAUSE_COUNT] = {0};

    // 胜局特征计数
    int winFeatureCount[WIN_FEATURE_COUNT] = {0};

    // 所有 trace（用于 CSV 输出）
    std::vector<GameTrace> allTraces;
};

// ============================================================
// 报告生成
// ============================================================

static std::string levelName(AILevel lv) {
    switch (lv) {
        case AILevel::AI1_Simple:  return "AI1";
        case AILevel::AI2_Rule:    return "AI2";
        case AILevel::AI3_Tracker: return "AI3";
        case AILevel::AI4_Expert:  return "AI4";
    }
    return "?";
}

static void writeTextReport(const std::string& path, const Options& opts,
                            const LossStats& stats,
                            const std::chrono::duration<double>& elapsed)
{
    std::ofstream f(path);
    if (!f) {
        std::cerr << "无法写入: " << path << "\n";
        return;
    }

    auto& o = f;
    o << "========================================\n";
    o << "AI4 败局误差分析报告\n";
    o << "========================================\n";
    o << "配置：\n";
    o << "  对局数：" << opts.games << "\n";
    o << "  种子：" << opts.seed << "\n";
    o << "  AI4 位置："
      << (opts.fixedFirst ? (opts.ai4First ? "先手" : "后手") : "随机")
      << "\n";
    o << "  对手等级：" << levelName(opts.opponent) << "\n";
    o << "  总耗时：" << std::fixed << std::setprecision(2) << elapsed.count() << " 秒\n";
    o << "\n";

    // 总体战绩
    o << "----------------------------------------\n";
    o << "总体战绩\n";
    o << "----------------------------------------\n";
    double winRate = stats.totalGames > 0 ? 100.0 * stats.wins / stats.totalGames : 0;
    double lossRate = stats.totalGames > 0 ? 100.0 * stats.losses / stats.totalGames : 0;
    double tieRate = stats.totalGames > 0 ? 100.0 * stats.ties / stats.totalGames : 0;
    double ai4Avg = stats.totalGames > 0 ? (double)stats.ai4ScoreSum / stats.totalGames : 0;
    double oppAvg = stats.totalGames > 0 ? (double)stats.oppScoreSum / stats.totalGames : 0;

    o << "AI4 胜：" << stats.wins << " (" << std::fixed << std::setprecision(1)
      << winRate << "%)\n";
    o << "AI4 负：" << stats.losses << " (" << lossRate << "%)\n";
    o << "平局： " << stats.ties << " (" << tieRate << "%)\n";
    o << "AI4 总均分：" << std::fixed << std::setprecision(1) << ai4Avg << "\n";
    o << "对手总均分：" << oppAvg << "\n";
    double firstDiff = stats.firstCount > 0
        ? (double)stats.firstScoreDiffSum / stats.firstCount : 0;
    double secondDiff = stats.secondCount > 0
        ? (double)stats.secondScoreDiffSum / stats.secondCount : 0;
    o << "先手均分差：+" << (firstDiff > 0 ? firstDiff : 0) << " / -"
      << (firstDiff < 0 ? -firstDiff : 0) << "\n";
    o << "后手均分差：+" << (secondDiff > 0 ? secondDiff : 0) << " / -"
      << (secondDiff < 0 ? -secondDiff : 0) << "\n";
    o << "\n";

    // 败因分布
    o << "----------------------------------------\n";
    o << "败因分布（共 " << stats.losses << " 负局）\n";
    o << "----------------------------------------\n";
    int totalLosses = stats.losses;
    // 先输出表格
    for (int i = 0; i < MAIN_CAUSE_COUNT; ++i) {
        int cnt = stats.lossCauseCount[i];
        double pct = totalLosses > 0 ? 100.0 * cnt / totalLosses : 0;
        o << (i + 1) << ". " << std::left << std::setw(22) << MAIN_CAUSE_NAMES[i]
          << std::right << std::setw(5) << cnt << " 局  "
          << std::fixed << std::setprecision(1) << pct << "%\n";
    }
    o << "\n";

    // 平局分布
    o << "----------------------------------------\n";
    o << "平局分布（共 " << stats.ties << " 平局）\n";
    o << "----------------------------------------\n";
    for (int i = 0; i < TIE_CAUSE_COUNT; ++i) {
        int cnt = stats.tieCauseCount[i];
        double pct = stats.ties > 0 ? 100.0 * cnt / stats.ties : 0;
        o << (i + 1) << ". " << std::left << std::setw(20) << TIE_CAUSE_NAMES[i]
          << std::right << std::setw(5) << cnt << " 局  "
          << std::fixed << std::setprecision(1) << pct << "%\n";
    }
    o << "\n";

    // 胜局特征
    o << "----------------------------------------\n";
    o << "胜局特征（共 " << stats.wins << " 胜局，可叠加）\n";
    o << "----------------------------------------\n";
    for (int i = 0; i < WIN_FEATURE_COUNT; ++i) {
        int cnt = stats.winFeatureCount[i];
        double pct = stats.wins > 0 ? 100.0 * cnt / stats.wins : 0;
        o << (i + 1) << ". " << std::left << std::setw(20) << WIN_FEATURE_NAMES[i]
          << std::right << std::setw(5) << cnt << " 局  "
          << std::fixed << std::setprecision(1) << pct << "%\n";
    }
    o << "\n";

    // 样本
    o << "----------------------------------------\n";
    o << "Top 3 高频败因详细样本（每类最多 3 局）\n";
    o << "----------------------------------------\n";
    // 败因排序
    std::vector<std::pair<int, int>> causeOrder;
    for (int i = 0; i < MAIN_CAUSE_COUNT; ++i) {
        if (stats.lossCauseCount[i] > 0)
            causeOrder.push_back({stats.lossCauseCount[i], i});
    }
    std::sort(causeOrder.begin(), causeOrder.end(),
              std::greater<std::pair<int, int>>());

    int sampleIdx = 0;
    for (auto& co : causeOrder) {
        if (sampleIdx >= 3) break;
        int causeIdx = co.second;
        const auto& samples = stats.lossSamples[causeIdx];
        for (size_t s = 0; s < samples.size() && s < 3; ++s) {
            sampleIdx++;
            const GameTrace& gt = samples[s];
            o << "[样本 " << sampleIdx << "] 局号 #" << gt.gameId
              << "，主因 " << MAIN_CAUSE_NAMES[causeIdx] << "\n";
            o << "  种子：" << gt.seed
              << " AI4 " << (gt.ai4First ? "先手" : "后手") << "\n";
            // 开局手牌（取第一个回合时 AI4 的手牌状态）
            o << "  终局比分：AI4 " << gt.ai4Score << " : 对手 " << gt.oppScore << "\n";
            if (!gt.secondaryTags.empty()) {
                o << "  次要标签：";
                for (const auto& tag : gt.secondaryTags) o << tag << " ";
                o << "\n";
            }
            // 关键事件
            for (const auto& te : gt.turns) {
                if (!te.isAI4Turn) continue;
                if (te.isBigCard) {
                    o << "  关键事件：回合 " << te.turn
                      << " AI4 出大牌 " << te.bigCardPoint
                      << "（当时桌面分 " << te.tableScoreBefore << "）\n";
                }
                if (te.isBomb) {
                    o << "  关键事件：回合 " << te.turn
                      << " AI4 使用炸弹（对手手牌 " << te.oppHandSize << " 张）\n";
                }
                if (te.played.empty() && te.tableScoreBefore >= 20) {
                    o << "  关键事件：回合 " << te.turn
                      << " AI4 放弃桌面分 " << te.tableScoreBefore << "\n";
                }
            }
            o << "\n";
        }
    }

    // 改进优先级建议
    o << "----------------------------------------\n";
    o << "改进优先级建议（自动生成）\n";
    o << "----------------------------------------\n";
    for (int rank = 0; rank < 3 && rank < (int)causeOrder.size(); ++rank) {
        int causeIdx = causeOrder[rank].second;
        int cnt = causeOrder[rank].first;
        double pct = totalLosses > 0 ? 100.0 * cnt / totalLosses : 0;
        o << (rank + 1) << ". " << MAIN_CAUSE_NAMES[causeIdx] << "（"
          << std::fixed << std::setprecision(1) << pct << "%）→ ";

        switch (causeIdx) {
            case BIG_CARD_EARLY:
                o << "建议：加强前期扣分权重，或在非必要回合避免出 7/鬼/5/2/3 等关键牌";
                break;
            case BOMB_WASTED:
                o << "建议：炸弹使用条件收紧，仅在对手手牌少或有大分桌面时使用";
                break;
            case TRUE_ABANDON:
               o << "建议：降低抢分阈值（stealThreshold），提高抢分收益倍数（stealMultiplier）";
               break;
            case FORCED_PASS:
               o << "建议：调整单牌/对子出牌策略，避免关键回合无牌可压";
               break;
            case ACTIVE_SPLIT:
                o << "建议：避免主动拆对/三张，优先出其他牌型";
                break;
            case FORCED_SPLIT:
                o << "建议：手牌结构导致被迫拆牌，需优化组合策略";
                break;
            case HAD_CHANCE:
                o << "建议：提高 specialKeepBonus、减少拆特殊牌";
                break;
            case NEVER_HAD_CHANCE:
                o << "建议：无特殊胜利机会，属正常分布";
                break;
            case HAD_BOMB:
                o << "建议：提高 endgameBombBonus，把炸弹留到终局";
                break;
            case NEVER_HAD_BOMB:
                o << "建议：无炸弹属牌运，不可控";
                break;
            case CLOSE_LOSS:
                o << "建议：查收尾阶段决策是否遗漏抢分或压分奖励机会";
                break;
            default:
                o << "建议：需要进一步分析具体对局细节";
                break;
        }
        o << "\n";
    }
    o << "\n";
    o << "========================================\n";

    f.close();
    std::cout << "文本报告已写入: " << path << "\n";
}

static void writeCsvReport(const std::string& path, const LossStats& stats) {
    std::ofstream f(path);
    if (!f) {
        std::cerr << "无法写入: " << path << "\n";
        return;
    }

    // CSV 头
    f << "game_id,seed,ai4_first,ai4_score,opp_score,result,turns,main_cause,"
         "secondary_tags,special_win,winner\n";

    for (const auto& gt : stats.allTraces) {
        f << gt.gameId << ","
          << gt.seed << ","
          << (gt.ai4First ? "1" : "0") << ","
          << gt.ai4Score << ","
          << gt.oppScore << ","
          << gt.result << ","
          << gt.final.totalTurns << ","
          << gt.mainCause << ","
          << "\"";
        for (size_t i = 0; i < gt.secondaryTags.size(); ++i) {
            if (i > 0) f << ";";
            f << gt.secondaryTags[i];
        }
        f << "\","
          << (gt.final.ai4Special ? "1" : "0") << ","
          << (gt.ai4Score > gt.oppScore ? "AI4" :
              gt.oppScore > gt.ai4Score ? "OPP" : "DRAW")
          << "\n";
    }

    f.close();
    std::cout << "CSV 报告已写入: " << path << "\n";
}

// ============================================================
// ACTIVE_SPLIT 导出
// ============================================================

static std::string cardTypeName(CardType t) {
    switch (t) {
        case CardType::Single:        return "单张";
        case CardType::Pair:          return "对子";
        case CardType::Triple:        return "三张";
        case CardType::TripleWithOne: return "三带一";
        case CardType::TripleWithTwo: return "三带二";
        case CardType::Bomb:          return "炸弹";
        case CardType::Rocket:        return "王炸";
        case CardType::Special523:    return "Special523";
        default: return "?";
    }
}

static void writeActiveSplitHeader(std::ostream& f) {
    f << "回合号,桌面分,AI4手牌,上一手牌型,上一手点数,上一手张数,"
         "AI4实际出牌型,AI4实际出点数,AI4实际出张数,"
         "拆了哪个点数,"
         "合法选择(牌型:点数:张数|...),对手手牌数,牌堆剩余\n";
}

static void writeActiveSplitLine(std::ostream& f, const GameTrace& trace) {
    const auto& turns = trace.turns;

    // 与 classifyMainCause 相同的逻辑：找到真正的 ACTIVE_SPLIT 回合
    size_t splitTurnIdx = (size_t)-1;
    std::string splitPoint;
    for (size_t i = 0; i < turns.size(); ++i) {
        const auto& te = turns[i];
        if (!te.isAI4Turn || te.played.empty()) continue;
        if (te.parsed.type != CardType::Single) continue;
        int inHandCount = 0;
        for (const Card& hc : te.handBefore)
            if (hc.point == te.parsed.keyPoint) inHandCount++;
        if (inHandCount < 2) continue;

        // 检查是否有不拆对且能压过上一手的合法出牌（与分类器一致）
        Player temp;
        temp.hand = te.handBefore;
        auto allPlays = enumerateLegalPlays(temp);
        bool hasNonSplit = false;
        for (const auto& p : allPlays) {
            std::map<std::string, int> pc;
            for (const Card& c : p) pc[c.point]++;
            bool nonSplit = true;
            for (const auto& kv2 : pc) {
                int inHand = 0;
                for (const Card& hc : te.handBefore)
                    if (hc.point == kv2.first) inHand++;
                if (inHand >= 2 && kv2.second == 1) { nonSplit = false; break; }
                if (inHand >= 3 && kv2.second < inHand && kv2.second < 3) { nonSplit = false; break; }
            }
            if (nonSplit) {
                auto parsed = parseCardType(p);
                if (parsed.type != CardType::Single &&
                    (te.lastPlayBeforeTurn.type == CardType::Invalid ||
                     canBeat(parsed, te.lastPlayBeforeTurn))) {
                    hasNonSplit = true; break;
                }
            }
        }
        if (!hasNonSplit) continue;  // FORCED_SPLIT 不是我们想要的

        splitTurnIdx = i;
        splitPoint = te.parsed.keyPoint;
        break;
    }
    if (splitTurnIdx == (size_t)-1) return;

    const auto& te = turns[splitTurnIdx];

    // 手牌
    std::string handStr;
    for (const auto& c : te.handBefore) {
        if (!handStr.empty()) handStr += " ";
        handStr += c.point;
        if (!c.suit.empty()) handStr += c.suit;
    }

    // 上一手
    std::string lastTypeStr = "自由";
    std::string lastPoint = "-";
    int lastCount = 0;
    if (te.lastPlayBeforeTurn.type != CardType::Invalid) {
        lastTypeStr = cardTypeName(te.lastPlayBeforeTurn.type);
        lastPoint = te.lastPlayBeforeTurn.keyPoint;
        lastCount = (int)te.lastPlayBeforeTurn.cards.size();
    }

    // 实际出牌
    std::string playTypeStr = cardTypeName(te.parsed.type);
    std::string playPoint = te.parsed.keyPoint;
    int playCount = (int)te.played.size();

    // 所有合法选择
    Player temp;
    temp.hand = te.handBefore;
    auto allPlays = enumerateLegalPlays(temp);
    std::string choicesStr;
    for (const auto& p : allPlays) {
        // 过滤 — 只保留"不要"和能压过上一手的
        // 注意：这里列出的应该是所有可能的合法出牌，不限于能压过的
        // 为了精简，只列出能压过上一手或有出牌意义的
        auto parsed = parseCardType(p);
        if (te.lastPlayBeforeTurn.type == CardType::Invalid ||
            canBeat(parsed, te.lastPlayBeforeTurn)) {
            if (!choicesStr.empty()) choicesStr += " | ";
            choicesStr += cardTypeName(parsed.type) + ":" + parsed.keyPoint + ":" + std::to_string(p.size());
        }
    }
    // 也列出"不要"选项
    if (te.lastPlayBeforeTurn.type != CardType::Invalid) {
        if (!choicesStr.empty()) choicesStr += " | ";
        choicesStr += "不要";
    }

    f << te.turn << ","
      << te.tableScoreBefore << ","
      << "\"" << handStr << "\","
      << lastTypeStr << "," << lastPoint << "," << lastCount << ","
      << playTypeStr << "," << playPoint << "," << playCount << ","
      << splitPoint << ","
      << "\"" << choicesStr << "\","
      << te.oppHandSize << ","
      << te.deckRemaining << "\n";
}

// ============================================================
// 主函数
// ============================================================

int main(int argc, char* argv[]) {
    Options opts = parseOptions(argc, argv);

    // 随机数生成器
    std::mt19937 rng(opts.seed);
    std::uniform_int_distribution<int> coin(0, 1);

    LossStats stats;

    auto startTime = std::chrono::steady_clock::now();

    std::cout << "========================================\n";
    std::cout << "AI4 败局分析工具\n";
    std::cout << "对局数: " << opts.games
              << "  种子: " << opts.seed
              << "  对手: " << levelName(opts.opponent) << "\n";
    if (opts.fixedFirst) {
        std::cout << "AI4 位置: " << (opts.ai4First ? "先手" : "后手") << "\n";
    } else {
        std::cout << "AI4 位置: 随机\n";
    }
    std::cout << "========================================\n\n";

    std::ofstream splitFile;
    if (opts.dumpActiveSplit) {
        splitFile.open(opts.activeSplitPath);
        writeActiveSplitHeader(splitFile);
    }
    if (opts.dumpActiveSplitOnly) {
        splitFile.open(opts.activeSplitPath);
        if (!splitFile.is_open()) {
            std::cerr << "无法写入: " << opts.activeSplitPath << "\n";
            return 1;
        }
        writeActiveSplitHeader(splitFile);
    }

    for (int g = 0; g < opts.games; ++g) {
        bool ai4First;
        int gameSeed = opts.seed + g * 31337;
        if (opts.fixedFirst) {
            ai4First = opts.ai4First;
        } else {
            std::mt19937 gamerng(gameSeed);
            ai4First = coin(gamerng) == 1;
        }

        std::mt19937 gamerng(gameSeed);
        GameTrace trace = runOneTracedGame(
            g, gameSeed, ai4First, opts.opponent, gamerng, opts.verbose);

        // 分类
        if (trace.result == "LOSE") {
            int mc = (int)classifyMainCause(trace);
            trace.mainCause = MAIN_CAUSE_NAMES[mc];
            trace.secondaryTags = collectSecondaryTags(trace);
            stats.lossCauseCount[mc]++;
            if (opts.dumpActiveSplit && mc == ACTIVE_SPLIT) {
                writeActiveSplitLine(splitFile, trace);
            }
            if (opts.dumpActiveSplitOnly && mc == ACTIVE_SPLIT) {
                writeActiveSplitLine(splitFile, trace);
            }
            // 存样本
            if ((int)stats.lossSamples[mc].size() < 3) {
                stats.lossSamples[mc].push_back(trace);
            }
        } else if (trace.result == "TIE") {
            std::string tc = classifyTieCause(trace);
            for (int i = 0; i < TIE_CAUSE_COUNT; ++i) {
                if (tc == TIE_CAUSE_NAMES[i]) {
                    stats.tieCauseCount[i]++;
                    break;
                }
            }
        } else { // WIN
            auto features = classifyWinFeatures(trace);
            for (const auto& f : features) {
                for (int i = 0; i < WIN_FEATURE_COUNT; ++i) {
                    if (f == WIN_FEATURE_NAMES[i]) {
                        stats.winFeatureCount[i]++;
                        break;
                    }
                }
            }
        }

        // 更新统计
        stats.totalGames++;
        if (trace.result == "WIN") stats.wins++;
        else if (trace.result == "LOSE") stats.losses++;
        else stats.ties++;

        stats.ai4ScoreSum += trace.ai4Score;
        stats.oppScoreSum += trace.oppScore;

        if (trace.ai4First) {
            stats.firstScoreDiffSum += (trace.ai4Score - trace.oppScore);
            stats.firstCount++;
        } else {
            stats.secondScoreDiffSum += (trace.ai4Score - trace.oppScore);
            stats.secondCount++;
        }

        stats.allTraces.push_back(trace);
    }

    if (splitFile.is_open()) {
        splitFile.close();
        std::cout << "Active Split CSV 已写入: " << opts.activeSplitPath << "\n";
    }

    auto endTime = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed = endTime - startTime;

    // 输出报告
    if (!opts.dumpActiveSplitOnly) {
        writeTextReport(opts.logPath, opts, stats, elapsed);
        writeCsvReport(opts.csvPath, stats);
    }

    // 控制台摘要
    if (!opts.dumpActiveSplitOnly) {
        std::cout << "\n=== 摘要 ===\n";
    double wr = stats.totalGames > 0 ? 100.0 * stats.wins / stats.totalGames : 0;
    double lr = stats.totalGames > 0 ? 100.0 * stats.losses / stats.totalGames : 0;
    double tr = stats.totalGames > 0 ? 100.0 * stats.ties / stats.totalGames : 0;
    std::cout << "胜 " << stats.wins << " (" << std::fixed << std::setprecision(1) << wr << "%)"
              << "  负 " << stats.losses << " (" << lr << "%)"
              << "  平 " << stats.ties << " (" << tr << "%)\n";
    std::cout << "AI4 均分 " << (double)stats.ai4ScoreSum / stats.totalGames
              << "  对手均分 " << (double)stats.oppScoreSum / stats.totalGames << "\n";

    std::cout << "\n败因分布:\n";
    for (int i = 0; i < MAIN_CAUSE_COUNT; ++i) {
        double pct = stats.losses > 0 ? 100.0 * stats.lossCauseCount[i] / stats.losses : 0;
        std::cout << "  " << MAIN_CAUSE_NAMES[i] << ": "
                  << stats.lossCauseCount[i] << " (" << std::fixed << std::setprecision(1) << pct << "%)\n";
    }
    }

    return 0;
}