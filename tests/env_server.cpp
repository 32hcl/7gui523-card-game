#include "core/card/card.h"
#include "core/card/cardtype.h"
#include "core/card/deck.h"
#include "core/player.h"
#include "core/rule/score.h"
#include "core/rule/special.h"
#include "game/game.h"
#include "ai/ai.h"
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <random>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <sstream>

// ── suppress stdout from game-library debug prints ──
class ScopedCoutSuppress {
    std::ostringstream m_buf;
    std::streambuf*       m_old;
public:
    ScopedCoutSuppress()  { m_old = std::cout.rdbuf(m_buf.rdbuf()); }
    ~ScopedCoutSuppress() { std::cout.rdbuf(m_old); }
};

static void printJson(const std::string& json) {
    fprintf(stdout, "%s\n", json.c_str());
    fflush(stdout);
}

static std::mt19937 g_rng(std::random_device{}());

static const char* g_opponentLevelStr = "AI1";

static AILevel parseAILevel(const char* level) {
    if (std::strcmp(level, "AI2") == 0) return AILevel::AI2_Rule;
    if (std::strcmp(level, "AI3") == 0) return AILevel::AI3_Tracker;
    if (std::strcmp(level, "Random") == 0) return AILevel::AI1_Simple; // handled separately
    return AILevel::AI1_Simple; // default
}

static std::string cardToString(const Card& c) {
    if (c.suit.empty()) return c.point;
    return c.suit + c.point;
}

static Card stringToCard(const std::string& s) {
    Card c;
    if (s == "\u5927\u9b3c" || s == "\u5c0f\u9b3c") {
        c.point = s;
        c.suit = "";
    } else {
        static const std::vector<std::string> suits = {
            "\u9ed1\u6843", "\u7ea2\u6843", "\u6885\u82b1", "\u65b9\u5757"
        };
        for (const auto& suit : suits) {
            if (s.rfind(suit, 0) == 0) {
                c.suit = suit;
                c.point = s.substr(suit.size());
                break;
            }
        }
    }
    if (c.point == "5")       c.score = 5;
    else if (c.point == "10") c.score = 10;
    else if (c.point == "K")  c.score = 20;
    else                      c.score = 0;
    return c;
}

static Card findCardInHand(const std::vector<Card>& hand, const Card& target) {
    for (const auto& h : hand) {
        if (h.point == target.point && h.suit == target.suit && h.score == target.score)
            return h;
    }
    return Card{};
}

static std::string jsonEscape(const std::string& s) {
    std::string r;
    for (char ch : s) {
        if (ch == '"') r += "\\\"";
        else if (ch == '\\') r += "\\\\";
        else r += ch;
    }
    return r;
}

static std::string jsonArray(const std::vector<std::string>& items) {
    std::string r = "[";
    for (size_t i = 0; i < items.size(); ++i) {
        if (i > 0) r += ", ";
        r += "\"" + jsonEscape(items[i]) + "\"";
    }
    r += "]";
    return r;
}

struct EnvState {
    Player me, opp;
    Deck deck;
    std::vector<Card> tableCards;
    int tableBonus = 0;
    CardTypeResult lastPlay;
    Player* lastPlayer = nullptr;
    int roundCount = 0;
    bool gameOver = false;
    std::string winner;  // "me", "opp", "draw"
    int myFinalScore = 0, oppFinalScore = 0;
    CardTracker tracker;
    int playedPointCount[15] = {0};  // cumulative cards played (persists across rounds)

    Player* currentPlayer() { return &me; }
    const Player* currentPlayer() const { return &me; }
    Player* opponentPlayer() { return &opp; }
    const Player* opponentPlayer() const { return &opp; }
};

static CardTypeResult safeParseCardType(const std::vector<Card>& cards) {
    if (cards.empty()) return CardTypeResult{};
    auto result = parseCardType(cards);
    if (result.cards.empty()) {
        result.cards = cards;
    }
    return result;
}

static std::string cardTypeName(CardType t) {
    switch (t) {
        case CardType::Single:        return "Single";
        case CardType::Pair:          return "Pair";
        case CardType::Triple:        return "Triple";
        case CardType::TripleWithOne: return "TripleWithOne";
        case CardType::TripleWithTwo: return "TripleWithTwo";
        case CardType::Bomb:          return "Bomb";
        case CardType::Rocket:        return "Rocket";
        case CardType::Special523:    return "Special523";
        default:                      return "Invalid";
    }
}

// ── state vector encoding (73-dim) ──
static const char* POINT_ORDER[15] = {
    "4", "6", "8", "9", "10", "J", "Q", "K", "A", "3", "2", "5",
    "\u5c0f\u9b3c", "\u5927\u9b3c", "7"  // 小鬼, 大鬼, 7
};

static int pointToIndex(const std::string& point) {
    for (int i = 0; i < 15; ++i) {
        if (point == POINT_ORDER[i]) return i;
    }
    return -1;
}

static int cardTypeToVecIndex(CardType t) {
    switch (t) {
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

static void incPlayedCounts(EnvState& env, const std::vector<Card>& cards) {
    for (const auto& c : cards) {
        int idx = pointToIndex(c.point);
        if (idx >= 0) env.playedPointCount[idx]++;
    }
}

static const int TOTAL_PER_POINT[15] = {
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,   // regular: 4 each
    1, 1,                                   // ghosts: 1 each
    4                                        // 7
};

static std::vector<float> buildStateVec(const EnvState& env) {
    std::vector<float> vec(103, 0.0f);

    auto handVec = [&](const std::vector<Card>& hand, int base) {
        for (const auto& c : hand) {
            int idx = pointToIndex(c.point);
            if (idx >= 0) vec[base + idx] += 1.0f;
        }
    };

    // 1. my hand (15)
    handVec(env.me.hand, 0);

    // 2. table cards (15)
    handVec(env.tableCards, 15);

    // 3. played cards cumulative (15)
    for (int i = 0; i < 15; ++i) {
        vec[30 + i] = (float)env.playedPointCount[i];
    }

    // 4. opponent expected count per point (15)
    //     unknown[p] = TOTAL[p] - played[p] - my_hand[p]
    //     opp_est[p] = unknown[p] * opp_hand_size / (opp_hand_size + deck_size)
    {
        float oppSize = (float)env.opp.hand.size();
        float deckSize = (float)env.deck.cards.size();
        float denom = oppSize + deckSize;
        if (denom > 0.0f) {
            for (int i = 0; i < 15; ++i) {
                float myCount = vec[i];
                float played = vec[30 + i];
                float unknown = TOTAL_PER_POINT[i] - played - myCount;
                if (unknown < 0.0f) unknown = 0.0f;
                vec[45 + i] = unknown * oppSize / denom;
            }
        }
    }

    // 5. deck expected count per point (15)
    //     deck_est[p] = unknown[p] - opp_est[p]
    {
        float oppSize = (float)env.opp.hand.size();
        float deckSize = (float)env.deck.cards.size();
        float denom = oppSize + deckSize;
        if (denom > 0.0f) {
            for (int i = 0; i < 15; ++i) {
                float myCount = vec[i];
                float played = vec[30 + i];
                float unknown = TOTAL_PER_POINT[i] - played - myCount;
                if (unknown < 0.0f) unknown = 0.0f;
                vec[60 + i] = unknown * deckSize / denom;
            }
        }
    }

    // 6. last play type (8-dim one-hot, skipping Invalid)
    int typeIdx = cardTypeToVecIndex(env.lastPlay.type);
    if (typeIdx >= 0) vec[75 + typeIdx] = 1.0f;

    // 7. last play key point (15-dim one-hot)
    if (!env.lastPlay.keyPoint.empty()) {
        int kpIdx = pointToIndex(env.lastPlay.keyPoint);
        if (kpIdx >= 0) vec[83 + kpIdx] = 1.0f;
    }

    // 8. table score (1)
    vec[98] = (float)calculateTableScore(env.tableCards, env.tableBonus) / 200.0f;

    // 9. score diff (1)
    vec[99] = (float)(env.me.totalScore - env.opp.totalScore) / 200.0f;

    // 10. deck remaining (1)
    vec[100] = (float)env.deck.cards.size() / 54.0f;

    // 11. opponent hand size (1)
    vec[101] = (float)env.opp.hand.size() / 20.0f;

    // 12. my hand size (1)
    vec[102] = (float)env.me.hand.size() / 20.0f;

    return vec;
}

static std::string vecToJson(const std::vector<float>& vec) {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < vec.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << vec[i];
    }
    oss << "]";
    return oss.str();
}

// ── clone helper (shallow copy, deep-copy vectors) ──
static EnvState cloneEnv(const EnvState& src) {
    EnvState dst;
    dst.me = src.me;
    dst.opp = src.opp;
    dst.deck = src.deck;
    dst.tableCards = src.tableCards;
    dst.tableBonus = src.tableBonus;
    dst.lastPlay = src.lastPlay;
    dst.lastPlayer = (src.lastPlayer == &src.me) ? &dst.me :
                     (src.lastPlayer == &src.opp) ? &dst.opp : nullptr;
    dst.roundCount = src.roundCount;
    dst.gameOver = src.gameOver;
    dst.winner = src.winner;
    dst.myFinalScore = src.myFinalScore;
    dst.oppFinalScore = src.oppFinalScore;
    dst.tracker = src.tracker;
    for (int i = 0; i < 15; ++i) dst.playedPointCount[i] = src.playedPointCount[i];
    return dst;
}

static std::string buildObservation(const EnvState& env) {
    std::ostringstream oss;
    oss << "{";

    oss << "\"my_hand\": [";
    for (size_t i = 0; i < env.me.hand.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << "\"" << jsonEscape(cardToString(env.me.hand[i])) << "\"";
    }
    oss << "], ";

    oss << "\"opp_hand_size\": " << env.opp.hand.size() << ", ";

    oss << "\"table_cards\": [";
    for (size_t i = 0; i < env.tableCards.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << "\"" << jsonEscape(cardToString(env.tableCards[i])) << "\"";
    }
    oss << "], ";

    oss << "\"table_score\": " << calculateTableScore(env.tableCards, env.tableBonus) << ", ";
    oss << "\"my_score\": " << env.me.totalScore << ", ";
    oss << "\"opp_score\": " << env.opp.totalScore << ", ";
    oss << "\"deck_remaining\": " << env.deck.cards.size() << ", ";

    if (env.lastPlay.type != CardType::Invalid) {
        oss << "\"last_play\": {";
        oss << "\"type\": \"" << cardTypeName(env.lastPlay.type) << "\", ";
        oss << "\"key\": \"" << jsonEscape(env.lastPlay.keyPoint) << "\", ";
        oss << "\"cards\": [";
        for (size_t i = 0; i < env.lastPlay.cards.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << "\"" << jsonEscape(cardToString(env.lastPlay.cards[i])) << "\"";
        }
        oss << "]}";
    } else {
        oss << "\"last_play\": null";
    }

    oss << "}";
    return oss.str();
}

static std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) a++;
    while (b > a && std::isspace((unsigned char)s[b-1])) b--;
    return s.substr(a, b - a);
}

static std::vector<std::string> splitActionStr(const std::string& s) {
    std::vector<std::string> result;
    bool inQuote = false;
    std::string cur;
    for (size_t i = 0; i < s.size(); ++i) {
        char ch = s[i];
        if (ch == '"') {
            inQuote = !inQuote;
        } else if (inQuote) {
            cur += ch;
        } else if (ch == ',' || ch == '[' || ch == ']' || std::isspace((unsigned char)ch)) {
            if (!cur.empty()) {
                result.push_back(cur);
                cur.clear();
            }
        } else {
            cur += ch;
        }
    }
    if (!cur.empty()) result.push_back(cur);
    return result;
}

static std::vector<Card> envParseAction(const std::string& actionStr, const Player& player,
                                         std::string& error) {
    error.clear();
    auto parts = splitActionStr(actionStr);
    if (parts.empty()) return {};

    std::vector<Card> cards;
    for (const auto& p : parts) {
        Card c = stringToCard(p);
        if (c.point.empty()) {
            error = "invalid card: " + p;
            return {};
        }
        bool found = false;
        for (const auto& h : player.hand) {
            if (h.point == c.point && h.suit == c.suit) {
                cards.push_back(h);
                found = true;
                break;
            }
        }
        if (!found) {
            error = "card not in hand: " + p;
            return {};
        }
    }
    auto parsed = safeParseCardType(cards);
    if (parsed.type == CardType::Invalid) {
        error = "invalid card combination";
        return {};
    }
    return cards;
}

static void envReset(EnvState& env) {
    env.deck = createStandardDeck();
    std::shuffle(env.deck.cards.begin(), env.deck.cards.end(), g_rng);

    env.me = createPlayer("me");
    env.opp = createPlayer("opp");
    env.opp.aiLevel = parseAILevel(g_opponentLevelStr);

    dealCards(env.me, env.deck, 5);
    dealCards(env.opp, env.deck, 5);

    env.tableCards.clear();
    env.tableBonus = 0;
    env.lastPlay = CardTypeResult{};
    env.lastPlayer = nullptr;
    env.roundCount = 0;
    env.gameOver = false;
    env.winner.clear();
    env.myFinalScore = 0;
    env.oppFinalScore = 0;
    env.tracker.reset();
    for (int i = 0; i < 15; ++i) env.playedPointCount[i] = 0;
}

static void runOpponentUntilMyTurn(EnvState& env) {
    while (!env.gameOver) {
        int tableScore = calculateTableScore(env.tableCards, env.tableBonus);

        // ---- opponent chooses a play ----
        std::vector<Card> oppPlay;
        fprintf(stderr, "[DEBUG] opp.aiLevel = %d\n", (int)env.opp.aiLevel);
        if (env.opp.isHuman) {
            oppPlay = humanChoosePlay(env.opp, env.lastPlay);
        } else if (std::strcmp(g_opponentLevelStr, "Random") == 0) {
            // random play: pick uniformly from legal plays
            auto allPlays = enumerateLegalPlays(env.opp);
            std::vector<std::vector<Card>> valid;
            for (auto& play : allPlays) {
                auto parsed = safeParseCardType(play);
                if (parsed.type == CardType::Invalid) continue;
                if (!env.lastPlay.cards.empty() && !canBeat(parsed, env.lastPlay)) continue;
                valid.push_back(play);
            }
            if (valid.empty()) {
                if (env.lastPlay.cards.empty()) {
                    // first to play, force-play first card
                    oppPlay = {env.opp.hand[0]};
                } else {
                    oppPlay = {}; // pass
                }
            } else {
                std::uniform_int_distribution<size_t> dist(0, valid.size() - 1);
                oppPlay = valid[dist(g_rng)];
            }
        } else {
            oppPlay = aiChoosePlay(env.opp, env.me, env.lastPlay,
                                   env.deck, tableScore, env.tracker);
        }

        if (oppPlay.empty()) {
            if (env.lastPlay.cards.empty()) {
                // first move, AI returned empty → force play first card
                if (env.opp.hand.empty()) {
                    env.gameOver = true;
                    env.winner = "me";
                    finalSettlement(env.me, env.opp, env.tableCards);
                    return;
                }
                oppPlay = {env.opp.hand[0]};
            } else {
                // opp passes, I (me) win this round
                settleScoreCards(env.me, env.tableCards);
                env.me.totalScore += env.tableBonus;
                env.tableCards.clear();
                env.tableBonus = 0;
                env.lastPlay = CardTypeResult{};
                env.lastPlayer = nullptr;
                env.roundCount++;

                refillToFive(env.me, env.deck);
                refillToFive(env.opp, env.deck);

                if (checkSpecialVictory(env.me)) {
                    env.gameOver = true; env.winner = "me"; return;
                }
                if (checkSpecialVictory(env.opp)) {
                    env.gameOver = true; env.winner = "opp"; return;
                }
                if (env.me.hand.empty() && env.opp.hand.empty() && env.deck.cards.empty()) {
                    env.gameOver = true;
                    if (env.me.totalScore > env.opp.totalScore) env.winner = "me";
                    else if (env.opp.totalScore > env.me.totalScore) env.winner = "opp";
                    else env.winner = "draw";
                    return;
                }
                // I start new round → return to agent
                return;
            }
        }

        CardTypeResult oppParsed = safeParseCardType(oppPlay);
        if (oppParsed.type == CardType::Invalid) continue;
        if (!env.lastPlay.cards.empty() && !canBeat(oppParsed, env.lastPlay)) continue;

        if (oppParsed.type == CardType::Special523) {
            settleScoreCards(env.opp, env.tableCards);
            env.opp.totalScore += env.tableBonus;
            env.tableCards.clear();
            env.tableBonus = 0;
            env.gameOver = true;
            env.winner = "opp";
            finalSettlement(env.opp, env.me, env.tableCards);
            return;
        }

        int bonus = calculatePressureBonus(oppParsed, env.lastPlay);
        env.tableBonus += bonus;
        removeCardsFromHand(env.opp, oppPlay);
        env.tableCards.insert(env.tableCards.end(), oppPlay.begin(), oppPlay.end());
        env.tracker.recordPlayed(oppPlay);
        incPlayedCounts(env, oppPlay);
        env.lastPlay = oppParsed;
        env.lastPlayer = &env.opp;

        if (env.opp.hand.empty()) {
            if (env.deck.cards.empty()) {
                env.gameOver = true;
                finalSettlement(env.opp, env.me, env.tableCards);
                if (env.me.totalScore > env.opp.totalScore) env.winner = "me";
                else if (env.opp.totalScore > env.me.totalScore) env.winner = "opp";
                else env.winner = "draw";
                return;
            }
            // opp emptied hand → opp wins round, starts next
            settleScoreCards(env.opp, env.tableCards);
            env.opp.totalScore += env.tableBonus;
            env.tableCards.clear();
            env.tableBonus = 0;
            env.lastPlay = CardTypeResult{};
            env.lastPlayer = nullptr;
            env.roundCount++;
            refillToFive(env.me, env.deck);
            refillToFive(env.opp, env.deck);
            if (checkSpecialVictory(env.me))      { env.gameOver = true; env.winner = "me"; return; }
            if (checkSpecialVictory(env.opp))     { env.gameOver = true; env.winner = "opp"; return; }
            continue;  // opp starts new round
        }

        // opp played, still has cards → my turn
        return;
    }
}

static bool isPassAction(const std::string& actionStr) {
    std::string t = trim(actionStr);
    if (t == "[]" || t == "[\"\"]") return true;
    std::string lower;
    for (char ch : t) lower += (char)std::tolower((unsigned char)ch);
    if (lower == "[]" || lower == "pass" || lower == "\"pass\"" || lower == "\"\"") return true;
    return false;
}

static void handleStep(EnvState& env, const std::string& actionStr) {
    ScopedCoutSuppress suppress;
    if (env.gameOver) {
        printJson("{\"status\": \"error\", \"message\": \"game already over\"}");
        return;
    }

    bool isPass = isPassAction(actionStr);
    std::string error;
    std::vector<Card> cards;
    CardTypeResult parsed;

    if (!isPass) {
        cards = envParseAction(actionStr, env.me, error);
        if (cards.empty() || !error.empty()) {
            std::ostringstream oss;
            oss << "{\"status\": \"error\", \"message\": \""
                << jsonEscape(error.empty() ? "invalid action" : error) << "\"}";
            printJson(oss.str());
            return;
        }
        parsed = safeParseCardType(cards);
        if (parsed.type == CardType::Invalid) {
            printJson("{\"status\": \"error\", \"message\": \"invalid card combination\"}");
            return;
        }
        if (!env.lastPlay.cards.empty() && !canBeat(parsed, env.lastPlay)) {
            printJson("{\"status\": \"error\", \"message\": \"cannot beat last play\"}");
            return;
        }
    }

    // ── execute the action ──
    if (isPass) {
        if (env.lastPlay.cards.empty()) {
            printJson("{\"status\": \"error\", \"message\": \"cannot pass as first player\"}");
            return;
        }
        if (env.lastPlayer) {
            settleScoreCards(*env.lastPlayer, env.tableCards);
            env.lastPlayer->totalScore += env.tableBonus;
        }
        env.tableCards.clear();
        env.tableBonus = 0;
        env.lastPlay = CardTypeResult{};
        env.lastPlayer = nullptr;
        env.roundCount++;

        refillToFive(env.me, env.deck);
        refillToFive(env.opp, env.deck);

        if (checkSpecialVictory(env.me))      { env.gameOver = true; env.winner = "me"; }
        else if (checkSpecialVictory(env.opp)) { env.gameOver = true; env.winner = "opp"; }
        else if (env.me.hand.empty() && env.opp.hand.empty() && env.deck.cards.empty()) {
            env.gameOver = true;
            if (env.me.totalScore > env.opp.totalScore) env.winner = "me";
            else if (env.opp.totalScore > env.me.totalScore) env.winner = "opp";
            else env.winner = "draw";
        }

    } else if (parsed.type == CardType::Special523) {
        settleScoreCards(env.me, env.tableCards);
        env.me.totalScore += env.tableBonus;
        env.tableCards.clear();
        env.tableBonus = 0;
        env.gameOver = true;
        env.winner = "me";
        finalSettlement(env.me, env.opp, env.tableCards);

    } else {
        int bonus = calculatePressureBonus(parsed, env.lastPlay);
        env.tableBonus += bonus;

        removeCardsFromHand(env.me, cards);
        env.tableCards.insert(env.tableCards.end(), cards.begin(), cards.end());
        env.tracker.recordPlayed(cards);
        incPlayedCounts(env, cards);
        env.lastPlay = parsed;
        env.lastPlayer = &env.me;

        if (env.me.hand.empty()) {
            if (env.deck.cards.empty()) {
                env.gameOver = true;
                finalSettlement(env.me, env.opp, env.tableCards);
                if (env.me.totalScore > env.opp.totalScore) env.winner = "me";
                else if (env.opp.totalScore > env.me.totalScore) env.winner = "opp";
                else env.winner = "draw";
            } else {
                settleScoreCards(env.me, env.tableCards);
                env.me.totalScore += env.tableBonus;
                env.tableCards.clear();
                env.tableBonus = 0;
                env.lastPlay = CardTypeResult{};
                env.lastPlayer = nullptr;
                env.roundCount++;

                refillToFive(env.me, env.deck);
                refillToFive(env.opp, env.deck);

                if (checkSpecialVictory(env.me))      { env.gameOver = true; env.winner = "me"; }
                else if (checkSpecialVictory(env.opp)) { env.gameOver = true; env.winner = "opp"; }
            }
        }
    }

    // ── after my action: run opponent if game continues ──
    if (!env.gameOver) {
        runOpponentUntilMyTurn(env);
    }

    env.myFinalScore = env.me.totalScore;
    env.oppFinalScore = env.opp.totalScore;

    // ── build response ──
    std::ostringstream oss;
    oss << "{\"status\": \"ok\", "
        << "\"observation\": " << buildObservation(env) << ", ";
    if (env.gameOver) {
        int reward = 0;
        if (env.winner == "me")       reward = 1;
        else if (env.winner == "opp") reward = -1;
        oss << "\"reward\": " << reward << ", "
            << "\"done\": true, "
            << "\"winner\": \"" << jsonEscape(env.winner) << "\", "
            << "\"my_final_score\": " << env.myFinalScore << ", "
            << "\"opp_final_score\": " << env.oppFinalScore;
    } else {
        oss << "\"reward\": 0, \"done\": false";
    }
    oss << "}";
    printJson(oss.str());
}

static void handleReset(EnvState& env) {
    ScopedCoutSuppress suppress;
    envReset(env);

    runOpponentUntilMyTurn(env);

    if (env.gameOver) {
        env.myFinalScore = env.me.totalScore;
        env.oppFinalScore = env.opp.totalScore;
    }

    std::ostringstream oss;
    oss << "{";
    oss << "\"status\": \"ok\", ";
    oss << "\"observation\": " << buildObservation(env);
    if (env.gameOver) {
        int reward = 0;
        if (env.winner == "me")      reward = 1;
        else if (env.winner == "opp") reward = -1;
        oss << ", \"reward\": " << reward
            << ", \"done\": true"
            << ", \"winner\": \"" << jsonEscape(env.winner) << "\"";
    } else {
        oss << ", \"reward\": 0, \"done\": false";
    }
    oss << "}";
    printJson(oss.str());
}

static void handleLegalActions(const EnvState& env) {
    if (env.gameOver) {
        printJson("{\"status\": \"error\", \"message\": \"game already over\"}");
        return;
    }

    auto allPlays = enumerateLegalPlays(env.me);
    std::vector<std::string> actions;

    for (const auto& play : allPlays) {
        auto parsed = safeParseCardType(play);
        if (parsed.type == CardType::Invalid) continue;

        if (!env.lastPlay.cards.empty() && !canBeat(parsed, env.lastPlay)) {
            continue;
        }

        std::vector<std::string> cardStrs;
        for (const auto& c : play) cardStrs.push_back(cardToString(c));
        actions.push_back(jsonArray(cardStrs));
    }

    if (!env.lastPlay.cards.empty()) {
        actions.push_back("[]");
    }

    std::ostringstream oss;
    oss << "{\"status\": \"ok\", \"actions\": [";
    for (size_t i = 0; i < actions.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << actions[i];
    }
    oss << "]}";
    printJson(oss.str());
}

static void handleStateVec(const EnvState& env) {
    auto vec = buildStateVec(env);
    std::ostringstream oss;
    oss << "{\"status\": \"ok\", \"state_vec\": " << vecToJson(vec) << "}";
    printJson(oss.str());
}

static void handlePeek(const EnvState& env) {
    if (env.gameOver) {
        printJson("{\"status\": \"error\", \"message\": \"game already over\"}");
        return;
    }

    auto allPlays = enumerateLegalPlays(env.me);
    std::vector<std::string> actionJsons;
    std::vector<std::vector<float>> nextStates;

    for (const auto& play : allPlays) {
        auto parsed = safeParseCardType(play);
        if (parsed.type == CardType::Invalid) continue;
        if (!env.lastPlay.cards.empty() && !canBeat(parsed, env.lastPlay)) continue;

        std::vector<std::string> cardStrs;
        for (const auto& c : play) cardStrs.push_back(cardToString(c));
        actionJsons.push_back(jsonArray(cardStrs));

        // clone env and apply this action (without running opponent)
        EnvState clone = cloneEnv(env);
        if (parsed.type == CardType::Special523) {
            // game ending move, no meaningful next state
            nextStates.push_back(buildStateVec(clone));
        } else {
            // apply action to clone
            int bonus = calculatePressureBonus(parsed, clone.lastPlay);
            clone.tableBonus += bonus;
            removeCardsFromHand(clone.me, play);
            clone.tableCards.insert(clone.tableCards.end(), play.begin(), play.end());
            clone.tracker.recordPlayed(play);
            incPlayedCounts(clone, play);
            clone.lastPlay = parsed;
            clone.lastPlayer = &clone.me;
            nextStates.push_back(buildStateVec(clone));
        }
    }

    // add pass action if not first player
    if (!env.lastPlay.cards.empty()) {
        actionJsons.push_back("[]");
        EnvState clone = cloneEnv(env);
        // pass: opponent wins the round
        settleScoreCards(clone.opp, clone.tableCards);
        clone.opp.totalScore += clone.tableBonus;
        clone.tableCards.clear();
        clone.tableBonus = 0;
        clone.lastPlay = CardTypeResult{};
        clone.lastPlayer = nullptr;
        clone.roundCount++;
        refillToFive(clone.me, clone.deck);
        refillToFive(clone.opp, clone.deck);
        nextStates.push_back(buildStateVec(clone));
    }

    std::ostringstream oss;
    oss << "{\"status\": \"ok\", \"actions\": [";
    for (size_t i = 0; i < actionJsons.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << actionJsons[i];
    }
    oss << "], \"next_states\": [";
    for (size_t i = 0; i < nextStates.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << vecToJson(nextStates[i]);
    }
    oss << "]}";
    printJson(oss.str());
}

static void handleAutoPlay(EnvState& env,
                           const std::string& oppAIStr,
                           const std::string& agentAIStr) {
    ScopedCoutSuppress suppress;

    AILevel oppLevel = parseAILevel(oppAIStr.c_str());
    AILevel agentLevel = parseAILevel(agentAIStr.c_str());

    envReset(env);
    env.opp.aiLevel = oppLevel;
    env.me.aiLevel = agentLevel;

    std::vector<std::vector<float>> states;
    std::vector<int> stepRoundIndex;          // which round each state belongs to
    std::vector<double> roundRewards;         // per-round reward
    bool myTurn = false;
    int currentRound = 0;
    double meScoreBaseline = (double)env.me.totalScore;
    double oppScoreBaseline = (double)env.opp.totalScore;

    auto settleRoundReward = [&]() {
        double meDelta = (double)env.me.totalScore - meScoreBaseline;
        double oppDelta = (double)env.opp.totalScore - oppScoreBaseline;
        roundRewards.push_back((meDelta - oppDelta) * 0.01);
        meScoreBaseline = (double)env.me.totalScore;
        oppScoreBaseline = (double)env.opp.totalScore;
    };

    while (!env.gameOver) {
        Player& cur  = myTurn ? env.me : env.opp;
        Player& other = myTurn ? env.opp : env.me;
        bool isMe = myTurn;

        if (myTurn) {
            states.push_back(buildStateVec(env));
            stepRoundIndex.push_back(currentRound);
        }

        int tableScore = calculateTableScore(env.tableCards, env.tableBonus);
        auto play = aiChoosePlay(cur, other, env.lastPlay,
                                 env.deck, tableScore, env.tracker);

        if (play.empty()) {
            if (env.lastPlay.cards.empty()) {
                if (cur.hand.empty()) {
                    env.gameOver = true;
                    env.winner = isMe ? "opp" : "me";
                    finalSettlement(env.me, env.opp, env.tableCards);
                    break;
                }
                play = {cur.hand[0]};
            } else {
                settleScoreCards(other, env.tableCards);
                other.totalScore += env.tableBonus;
                env.tableCards.clear();
                env.tableBonus = 0;
                env.lastPlay = CardTypeResult{};
                env.lastPlayer = nullptr;
                settleRoundReward();
                env.roundCount++;
                currentRound++;

                refillToFive(env.me, env.deck);
                refillToFive(env.opp, env.deck);

                if (checkSpecialVictory(env.me))  { env.gameOver = true; env.winner = "me"; break; }
                if (checkSpecialVictory(env.opp)) { env.gameOver = true; env.winner = "opp"; break; }
                if (env.me.hand.empty() && env.opp.hand.empty() && env.deck.cards.empty()) {
                    env.gameOver = true;
                    if (env.me.totalScore > env.opp.totalScore) env.winner = "me";
                    else if (env.opp.totalScore > env.me.totalScore) env.winner = "opp";
                    else env.winner = "draw";
                    break;
                }
                myTurn = !myTurn;
                continue;
            }
        }

        CardTypeResult parsed = safeParseCardType(play);

        if (parsed.type == CardType::Special523) {
            settleScoreCards(cur, env.tableCards);
            cur.totalScore += env.tableBonus;
            env.tableCards.clear();
            env.tableBonus = 0;
            env.gameOver = true;
            env.winner = isMe ? "me" : "opp";
            finalSettlement(cur, other, env.tableCards);
            break;
        }

        int bonus = calculatePressureBonus(parsed, env.lastPlay);
        env.tableBonus += bonus;
        removeCardsFromHand(cur, play);
        env.tableCards.insert(env.tableCards.end(), play.begin(), play.end());
        env.tracker.recordPlayed(play);
        incPlayedCounts(env, play);
        env.lastPlay = parsed;
        env.lastPlayer = &cur;

        if (cur.hand.empty()) {
            if (env.deck.cards.empty()) {
                env.gameOver = true;
                finalSettlement(env.me, env.opp, env.tableCards);
                if (env.me.totalScore > env.opp.totalScore) env.winner = "me";
                else if (env.opp.totalScore > env.me.totalScore) env.winner = "opp";
                else env.winner = "draw";
                break;
            }
            settleScoreCards(cur, env.tableCards);
            cur.totalScore += env.tableBonus;
            env.tableCards.clear();
            env.tableBonus = 0;
            env.lastPlay = CardTypeResult{};
            env.lastPlayer = nullptr;
            settleRoundReward();
            env.roundCount++;
            currentRound++;
            refillToFive(env.me, env.deck);
            refillToFive(env.opp, env.deck);
            if (checkSpecialVictory(env.me))  { env.gameOver = true; env.winner = "me"; break; }
            if (checkSpecialVictory(env.opp)) { env.gameOver = true; env.winner = "opp"; break; }
            continue;
        }

        myTurn = !myTurn;
    }

    // ── result from agent perspective ──
    int result = 0;
    if (env.winner == "me")       result = 1;
    else if (env.winner == "opp") result = -1;

    // ── compute cumulative rewards (backward) ──
    int nRounds = (int)roundRewards.size();
    std::vector<double> cumulative(nRounds, 0.0);
    if (nRounds > 0) {
        cumulative[nRounds - 1] = roundRewards[nRounds - 1] + (double)result;
        for (int r = nRounds - 2; r >= 0; --r) {
            cumulative[r] = roundRewards[r] + cumulative[r + 1];
        }
    } else {
        cumulative.push_back((double)result);
    }

    // ── build JSON response ──
    std::ostringstream oss;
    oss << "{\"status\": \"ok\", \"trajectory\": [";
    for (size_t i = 0; i < states.size(); ++i) {
        if (i > 0) oss << ", ";
        int r = (i < stepRoundIndex.size()) ? stepRoundIndex[i] : 0;
        double cr = (r < (int)cumulative.size()) ? cumulative[r] : 0.0;
        oss << "{\"state\": " << vecToJson(states[i])
            << ", \"round\": " << r
            << ", \"cumulative_reward\": " << cr << "}";
    }
    oss << "]}";
    printJson(oss.str());
}

static std::string extractField(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos += search.size();
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == ':' || json[pos] == '\t'))
        pos++;
    if (pos >= json.size()) return "";
    if (json[pos] == '"') {
        pos++;
        std::string val;
        while (pos < json.size() && json[pos] != '"') {
            if (json[pos] == '\\' && pos + 1 < json.size()) {
                val += json[pos + 1];
                pos += 2;
            } else {
                val += json[pos];
                pos++;
            }
        }
        return val;
    } else if (json[pos] == '[') {
        size_t end = json.find(']', pos);
        if (end == std::string::npos) return "";
        return json.substr(pos, end - pos + 1);
    }
    return "";
}

int main(int argc, char* argv[]) {
    if (argc > 1) {
        g_opponentLevelStr = argv[1];
    }

    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    EnvState env;
    envReset(env);
    bool initialized = false;

    std::string line;
    while (std::getline(std::cin, line)) {
        line = trim(line);
        if (line.empty()) continue;

        std::string cmd = extractField(line, "command");

        if (cmd == "reset") {
            handleReset(env);
            initialized = true;
        } else if (cmd == "legal_actions") {
            if (!initialized) {
                printJson("{\"status\": \"error\", \"message\": \"call reset first\"}");
            } else {
                handleLegalActions(env);
            }
        } else if (cmd == "step") {
            if (!initialized) {
                printJson("{\"status\": \"error\", \"message\": \"call reset first\"}");
            } else {
                std::string action = extractField(line, "action");
                handleStep(env, action);
            }
        } else if (cmd == "state_vec") {
            if (!initialized) {
                printJson("{\"status\": \"error\", \"message\": \"call reset first\"}");
            } else {
                handleStateVec(env);
            }
        } else if (cmd == "peek") {
            if (!initialized) {
                printJson("{\"status\": \"error\", \"message\": \"call reset first\"}");
            } else {
                handlePeek(env);
            }
        } else if (cmd == "auto_play") {
            std::string oppAI = extractField(line, "opp_ai");
            std::string agentAI = extractField(line, "agent_ai");
            if (oppAI.empty()) oppAI = "AI2";
            if (agentAI.empty()) agentAI = "AI2";
            handleAutoPlay(env, oppAI, agentAI);
            initialized = true;
        } else if (cmd == "quit") {
            printJson("{\"status\": \"ok\", \"message\": \"bye\"}");
            break;
        } else {
            std::ostringstream oss;
            oss << "{\"status\": \"error\", \"message\": \"unknown command: "
                << jsonEscape(cmd) << "\"}";
            printJson(oss.str());
        }
    }

    return 0;
}