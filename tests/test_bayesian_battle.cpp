#include "ai/sampler/uniform_sampler.h"
#include "ai/searcher/sample_search.h"
#include "ai/searcher/minimax.h"
#include "ai/searcher/search_params.h"
#include "ai/searcher/search_state.h"
#include "core/card/deck.h"
#include "core/card/cardtype.h"
#include "core/player.h"
#include "core/tracker/cardtracker.h"
#include "core/rule/score.h"
#include "core/rule/special.h"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <chrono>
#include <random>
#include <algorithm>

struct BattleResult {
    int wins = 0;
    long long scoreSum = 0;
    long long timeUs = 0;
    int steps = 0;
};

static void runOneGame(
    bool sampledIsFirst,
    std::mt19937& rng,
    BattleResult& sampRes,
    BattleResult& cheatRes,
    int depth,
    int sampleCount,
    int topK,
    bool useBayesian)
{
    Deck deck = createStandardDeck();
    std::shuffle(deck.cards.begin(), deck.cards.end(), rng);

    Player cheatP = createPlayer("Cheat");
    Player sampP = createPlayer("Samp");
    dealCards(cheatP, deck, 5);
    dealCards(sampP, deck, 5);

    CardTracker tracker;

    Player* first   = sampledIsFirst ? &sampP  : &cheatP;
    Player* second  = sampledIsFirst ? &cheatP : &sampP;
    Player* current = first;
    Player* opponent = second;
    Player* lastPlayer = nullptr;

    std::vector<Card> tableCards;
    CardTypeResult lastPlay;
    lastPlay.type = CardType::Invalid;

    SearchParams params;
    params.searchDepth = depth;

    int roundCount = 0;
    int safety = 0;

    while (true) {
        if (++safety > 500) break;

        while (true) {
            int tableScore = calculateScore(tableCards);
            std::vector<Card> play;
            bool isCheat = (current == &cheatP);

            auto t0 = std::chrono::steady_clock::now();

            if (isCheat) {
                play = searchBestPlayCheat(*current, *opponent, lastPlay,
                                           deck, tableScore, depth, params);
            } else {
                play = searchBestPlaySampled(*current, *opponent, lastPlay,
                                             deck, tableScore, tracker,
                                             params, sampleCount, topK,
                                             roundCount, useBayesian);
            }

            auto t1 = std::chrono::steady_clock::now();
            auto durUs = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
            if (isCheat) {
                cheatRes.timeUs += durUs;
                cheatRes.steps++;
            } else {
                sampRes.timeUs += durUs;
                sampRes.steps++;
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
                if (current == &sampP) sampRes.wins++;
                else cheatRes.wins++;
                sampRes.scoreSum += sampP.totalScore;
                cheatRes.scoreSum += cheatP.totalScore;
                return;
            }

            if (current->hand.empty()) {
                if (deck.cards.empty()) {
                    settleScoreCards(*current, tableCards);
                    settleScoreCards(*current, opponent->hand);
                    opponent->hand.clear();
                    tableCards.clear();
                    if (first->totalScore > second->totalScore) {
                        if (first == &sampP) sampRes.wins++;
                        else cheatRes.wins++;
                    } else if (second->totalScore > first->totalScore) {
                        if (second == &sampP) sampRes.wins++;
                        else cheatRes.wins++;
                    }
                    sampRes.scoreSum += sampP.totalScore;
                    cheatRes.scoreSum += cheatP.totalScore;
                    return;
                }
            }

            std::swap(current, opponent);
        }

        roundCount++;

        lastPlay.type = CardType::Invalid;
        if (checkSpecialVictory(*first)) {
            if (first == &sampP) sampRes.wins++;
            else cheatRes.wins++;
            sampRes.scoreSum += sampP.totalScore;
            cheatRes.scoreSum += cheatP.totalScore;
            return;
        }
        if (checkSpecialVictory(*second)) {
            if (second == &sampP) sampRes.wins++;
            else cheatRes.wins++;
            sampRes.scoreSum += sampP.totalScore;
            cheatRes.scoreSum += cheatP.totalScore;
            return;
        }

        if (!deck.cards.empty()) {
            Player* loser = (lastPlayer == first) ? second : first;
            refillToFive(*lastPlayer, deck);
            refillToFive(*loser, deck);
        }
        current = lastPlayer;
        opponent = (current == first) ? second : first;
        if (deck.cards.empty() && first->hand.empty() && second->hand.empty()) break;
    }

    sampRes.scoreSum += sampP.totalScore;
    cheatRes.scoreSum += cheatP.totalScore;
    if (first->totalScore > second->totalScore) {
        if (first == &sampP) sampRes.wins++;
        else cheatRes.wins++;
    } else if (second->totalScore > first->totalScore) {
        if (second == &sampP) sampRes.wins++;
        else cheatRes.wins++;
    }
}

int main() {
    std::ofstream out("bayesian_battle_result.txt");
    auto& o = out;

    o << "===========================================================\n";
    o << "  Bayesian vs Uniform: Battle vs Cheat\n";
    o << "  3 configs x 40 games = 120 total\n";
    o << "===========================================================\n\n";

    o << std::left
      << std::setw(12) << "Config"
      << std::setw(8)  << "Wins"
      << std::setw(8)  << "WR%"
      << std::setw(12) << "AvgDiff"
      << std::setw(14) << "StepMs"
      << "\n";
    o << std::string(60, '-') << "\n";

    struct Config {
        const char* name;
        int samples;
        bool bayesian;
    };
    Config configs[] = {
        {"Uniform30",   30, false},
        {"Bayes30",     30, true },
        {"Bayes15",     15, true },
    };

    int depth  = 6;
    int topK   = 999;
    int nGames = 40;

    std::random_device rd;
    std::mt19937 rng(rd());

    // 验证：跑 1 局，检查候选走法是否全保留
    {
        std::cerr << "--- topK verification (1 game) ---\n";
        Deck vDeck = createStandardDeck();
        std::shuffle(vDeck.cards.begin(), vDeck.cards.end(), rng);
        Player vP1 = createPlayer("V1"), vP2 = createPlayer("V2");
        dealCards(vP1, vDeck, 5);
        dealCards(vP2, vDeck, 5);
        CardTracker vTracker;
        SearchState vState;
        vState.myHand = vP1.hand;
        vState.oppHand = vP2.hand;
        vState.deckCards = vDeck.cards;
        vState.myTurn = true;
        vState.tableScore = 0;
        vState.myScore = 0;
        vState.oppScore = 0;
        vState.terminal = false;
        vState.winner = 0;
        CardTypeResult vLast;
        vLast.type = CardType::Invalid;
        vState.lastPlay = vLast;

        auto allMoves = genLegalMoves(vState);
        std::cerr << "  total legal moves: " << allMoves.size() << "\n";

        SearchParams vParams;
        vParams.searchDepth = depth;
        // 用 Uniform30+topK=999 调用一次
        auto chosen = searchBestPlaySampled(vP1, vP2, vLast, vDeck, 0,
                                            vTracker, vParams, 30, topK,
                                            0, false);
        std::cerr << "  chosen move size: " << chosen.size() << "\n";

        // 如果有 topK 过滤，搜索的走法数应该 = allMoves.size()
        // 如果 allMoves 很多而 searchBestPlaySampled 只搜了少数，说明 topK 没生效
        std::cerr << "  topK=" << topK << " (should search all " << allMoves.size()
                  << " moves)\n\n";
    }

    for (auto& cfg : configs) {
        std::cerr << "Running " << cfg.name << " ...\n";
        BattleResult sampRes, cheatRes;

        for (int g = 0; g < nGames; ++g) {
            runOneGame(g % 2 == 0, rng, sampRes, cheatRes,
                       depth, cfg.samples, topK, cfg.bayesian);
        }

        double stepMs = sampRes.steps > 0
            ? sampRes.timeUs / (double)sampRes.steps / 1000.0 : 0;

        o << std::left
          << std::setw(12) << cfg.name
          << std::setw(8)  << (std::to_string(sampRes.wins) + "/" + std::to_string(nGames))
          << std::setw(8)  << std::fixed << std::setprecision(1)
                           << (100.0 * sampRes.wins / nGames) << "%"
          << std::setw(12) << std::fixed << std::setprecision(1)
                           << ((sampRes.scoreSum - cheatRes.scoreSum) / (double)nGames)
          << std::setw(14) << std::fixed << std::setprecision(1) << stepMs
          << "\n";

        std::cerr << "  Done: WR=" << (100.0 * sampRes.wins / nGames)
                  << "% stepMs=" << stepMs << "\n";
    }

    out.close();
    std::cerr << "\nDone. See bayesian_battle_result.txt\n";
    return 0;
}