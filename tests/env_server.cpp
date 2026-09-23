#include "core/card/card.h"
#include "core/card/cardtype.h"
#include "core/card/deck.h"
#include "core/player.h"
#include "core/rule/score.h"
#include "core/rule/special.h"
#include "game/game.h"
#include "ai/ai.h"
#include "ai/fair_engine.h"
#include "ai/fair_common.h"
#include "ai/searcher/minimax.h"
#include <climits>
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

static std::mt19937 g_rng(714025);
static unsigned g_seed=714025,g_game=0;

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
    Observation fairObs[2];
    FairEngine engines[2];
    Deck deck;
    std::vector<Card> tableCards;
    int tableBonus = 0;
    CardTypeResult lastPlay;
    Player* lastPlayer = nullptr;
    int roundCount = 0;
    bool gameOver = false;
    bool myTurn=true,finalPhase=false;
    int firstEmpty=0;
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
    for(int i=0;i<2;++i){dst.fairObs[i]=src.fairObs[i];dst.engines[i]=src.engines[i];}
    dst.roundCount = src.roundCount;
    dst.myTurn=src.myTurn;dst.finalPhase=src.finalPhase;dst.firstEmpty=src.firstEmpty;
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
    oss << "\"final_phase\":" << (env.finalPhase?"true":"false") << ",\"played_counts\":[";
    for(int i=0;i<15;++i){if(i)oss<<",";oss<<env.playedPointCount[i];}oss<<"],";

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

static SearchState positionOf(const EnvState& env) {
 SearchState s;s.myHand=env.me.hand;s.oppHand=env.opp.hand;s.deckCards=env.deck.cards;
 s.myScore=env.me.totalScore;s.oppScore=env.opp.totalScore;s.tableScore=calculateScore(env.tableCards);s.tableBonus=env.tableBonus;
 s.lastPlay=env.lastPlay;s.myTurn=env.myTurn;s.finalPhase=env.finalPhase;s.firstEmpty=env.firstEmpty;s.terminal=env.gameOver;return s;
}
static void applyEnvAction(EnvState& env,const std::vector<Card>& action) {
 auto previous=positionOf(env);auto n=applyMove(previous,action);
 for(int side=0;side<2;++side){auto& obs=env.fairObs[side];bool mine=side==0;auto before=mine?previous.myHand:previous.oppHand;const auto& after=mine?n.myHand:n.oppHand;
  PublicEvent event;event.actorMe=previous.myTurn==mine;event.cards=fair::canonical(action);event.myCount=static_cast<int>(after.size());event.opponentCount=static_cast<int>((mine?n.oppHand:n.myHand).size());event.deckCount=static_cast<int>(n.deckCards.size());
  if(event.actorMe)for(const auto& c:action){auto it=std::find_if(before.begin(),before.end(),[&](const Card& h){return h.point==c.point&&h.suit==c.suit;});if(it!=before.end())before.erase(it);}
  for(const auto& c:after){auto it=std::find_if(before.begin(),before.end(),[&](const Card& h){return h.point==c.point&&h.suit==c.suit;});if(it==before.end())event.myDraws.push_back(fair::card(fair::index(c.point)));else before.erase(it);}
  obs.history.push_back(event);for(const auto& c:action)++obs.played[fair::index(c.point)];obs.hand=after;obs.opponentCount=event.opponentCount;obs.deckCount=event.deckCount;obs.myScore=mine?n.myScore:n.oppScore;obs.opponentScore=mine?n.oppScore:n.myScore;obs.tableScore=n.tableScore;obs.tableBonus=n.tableBonus;obs.previous=n.lastPlay;obs.finalPhase=n.finalPhase;obs.firstEmpty=mine?n.firstEmpty:-n.firstEmpty;
 }
 env.me.hand=n.myHand;env.opp.hand=n.oppHand;env.deck.cards=n.deckCards;
 env.me.totalScore=n.myScore;env.opp.totalScore=n.oppScore;env.tableBonus=n.tableBonus;
 if(action.empty()||n.terminal)env.tableCards.clear();else env.tableCards.insert(env.tableCards.end(),action.begin(),action.end());
 env.lastPlayer=nullptr;env.lastPlay=n.lastPlay;env.myTurn=n.myTurn;env.finalPhase=n.finalPhase;env.firstEmpty=n.firstEmpty;
 env.tracker.recordPlayed(action);incPlayedCounts(env,action);if(action.empty())++env.roundCount;
 env.gameOver=n.terminal;if(n.terminal)env.winner=n.winner==1?"me":n.winner==-1?"opp":"draw";
 env.myFinalScore=n.myScore;env.oppFinalScore=n.oppScore;
}
static std::vector<Card> envMove(EnvState& env,const std::string& level) {
 if(level=="Fair1"||level=="Fair2"){int side=env.myTurn?0:1;return env.engines[side].choosePlay(env.fairObs[side],level=="Fair1"?1:2);}
 auto s=positionOf(env);
 if(!s.myTurn){std::swap(s.myHand,s.oppHand);std::swap(s.myScore,s.oppScore);s.myTurn=true;s.firstEmpty=-s.firstEmpty;}
 auto legal=genLegalMoves(s);
 if(level=="Random")return legal[std::uniform_int_distribution<size_t>(0,legal.size()-1)(g_rng)];
 if(level=="AI4") {int best=INT_MIN+1;std::vector<Card> result;for(const auto& a:legal){auto child=applyMove(s,a);int v=minimax(child,5,INT_MIN+1,INT_MAX-1,false,SearchParams{});if(v>best){best=v;result=a;}}return result;}
 Player me=createPlayer("actor"),opp=createPlayer("opponent");me.hand=s.myHand;opp.hand=s.oppHand;me.totalScore=s.myScore;opp.totalScore=s.oppScore;me.aiLevel=parseAILevel(level.c_str());Deck d;d.cards=s.deckCards;
 return aiChoosePlay(me,opp,s.lastPlay,d,s.tableScore+s.tableBonus,env.tracker);
}
static void envReset(EnvState& env) {
 env=EnvState{};env.me=createPlayer("me");env.opp=createPlayer("opp");env.opp.aiLevel=parseAILevel(g_opponentLevelStr);
 env.deck=createStandardDeck();g_rng.seed(g_seed+g_game/2);std::shuffle(env.deck.cards.begin(),env.deck.cards.end(),g_rng);
 bool first=(g_game++%2)==0;dealCards(first?env.me:env.opp,env.deck,5);dealCards(first?env.opp:env.me,env.deck,5);env.myTurn=first;
 for(int side=0;side<2;++side){auto& obs=env.fairObs[side];obs.hand=side==0?env.me.hand:env.opp.hand;obs.initialHand=obs.hand;obs.initialMyTurn=(side==0)==first;}
}

static void runOpponentUntilMyTurn(EnvState& env) {
 while(!env.gameOver&&!env.myTurn)applyEnvAction(env,envMove(env,g_opponentLevelStr));
}

static bool isPassAction(const std::string& actionStr) {
    std::string t = trim(actionStr);
    if (t == "[]" || t == "[\"\"]") return true;
    std::string lower;
    for (char ch : t) lower += (char)std::tolower((unsigned char)ch);
    if (lower == "[]" || lower == "pass" || lower == "\"pass\"" || lower == "\"\"") return true;
    return false;
}

static void handleStep(EnvState& env,const std::string& actionStr) {
 ScopedCoutSuppress suppress;
 if(env.gameOver){printJson("{\"status\":\"error\",\"message\":\"game already over\"}");return;}
 try {
  std::string error;auto cards=isPassAction(actionStr)?std::vector<Card>{}:envParseAction(actionStr,env.me,error);
  if(!error.empty())throw std::invalid_argument(error);
  applyEnvAction(env,cards);runOpponentUntilMyTurn(env);
  int reward=env.winner=="me"?1:env.winner=="opp"?-1:0;
  std::ostringstream out;out<<"{\"status\":\"ok\",\"observation\":"<<buildObservation(env)<<",\"reward\":"<<reward<<",\"done\":"<<(env.gameOver?"true":"false")<<",\"winner\":\""<<env.winner<<"\",\"my_final_score\":"<<env.myFinalScore<<",\"opp_final_score\":"<<env.oppFinalScore<<"}";printJson(out.str());
 }catch(const std::exception& e){printJson("{\"status\":\"error\",\"message\":\""+jsonEscape(e.what())+"\"}");}
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
 if(env.gameOver){printJson("{\"status\":\"error\",\"message\":\"game already over\"}");return;}
 const auto legal=genLegalMoves(positionOf(env));std::string actions,states,rewards,dones;
 auto saved=g_rng;
 for(const auto& a:legal){auto clone=cloneEnv(env);g_rng=saved;applyEnvAction(clone,a);runOpponentUntilMyTurn(clone);std::vector<std::string> cs;for(const auto& c:a)cs.push_back(cardToString(c));
 if(!actions.empty()){actions+=",";states+=",";rewards+=",";dones+=",";}
 actions+=jsonArray(cs);states+=vecToJson(buildStateVec(clone));rewards+=clone.winner=="me"?"1":clone.winner=="opp"?"-1":"0";dones+=clone.gameOver?"true":"false";
 }g_rng=saved;printJson("{\"status\":\"ok\",\"privileged\":true,\"actions\":["+actions+"],\"next_states\":["+states+"],\"rewards\":["+rewards+"],\"dones\":["+dones+"]}");
}

static void handleAutoPlay(EnvState& env,const std::string& oppAI,const std::string& agentAI) {
 ScopedCoutSuppress suppress;envReset(env);std::vector<std::vector<float>> states;std::vector<int> rounds;
 while(!env.gameOver){if(env.myTurn){states.push_back(buildStateVec(env));rounds.push_back(env.roundCount);}applyEnvAction(env,envMove(env,env.myTurn?agentAI:oppAI));}
 int reward=env.winner=="me"?1:env.winner=="opp"?-1:0;std::ostringstream out;out<<"{\"status\":\"ok\",\"target_version\":\"terminal_wdl_v2\",\"trajectory\":[";
 for(size_t i=0;i<states.size();++i){if(i)out<<",";out<<"{\"state\":"<<vecToJson(states[i])<<",\"round\":"<<rounds[i]<<",\"cumulative_reward\":"<<reward<<"}";}out<<"]}";printJson(out.str());
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

    if(argc>2)g_seed=static_cast<unsigned>(std::stoul(argv[2]));
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    EnvState env;
    bool initialized = false;

    std::string line;
    while (std::getline(std::cin, line)) {
        line = trim(line);
        if (line.empty()) continue;

        std::string cmd = extractField(line, "command");

        if (cmd == "reset") {
            auto seed=extractField(line,"seed");if(!seed.empty()){g_seed=static_cast<unsigned>(std::stoul(seed));g_game=0;}
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