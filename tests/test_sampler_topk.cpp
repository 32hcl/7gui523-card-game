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

struct GameResult {
    int cheatWins = 0;
    int sampledWins = 0;
    int draws = 0;
    long long cheatScoreSum = 0;
    long long sampledScoreSum = 0;
    long long cheatTimeUs = 0;
    long long sampledTimeUs = 0;
    int cheatSteps = 0;
    int sampledSteps = 0;
};

static void runOneGame(
    bool sampledIsFirst,
    std::mt19937& rng,
    GameResult& res,
    int maxDepth,
    int sampleCount,
    int topK)
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
    params.searchDepth = maxDepth;

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
                                           deck, tableScore, maxDepth, params);
            } else {
                play = searchBestPlaySampled(*current, *opponent, lastPlay,
                                             deck, tableScore, tracker,
                                             params, sampleCount, topK);
            }

            auto t1 = std::chrono::steady_clock::now();
            auto durUs = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
            if (isCheat) {
                res.cheatTimeUs += durUs;
                res.cheatSteps++;
            } else {
                res.sampledTimeUs += durUs;
                res.sampledSteps++;
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
                if (current == &cheatP) res.cheatWins++;
                else res.sampledWins++;
                return;
            }

            if (current->hand.empty()) {
                if (deck.cards.empty()) {
                    settleScoreCards(*current, tableCards);
                    settleScoreCards(*current, opponent->hand);
                    opponent->hand.clear();
                    tableCards.clear();
                    if (first->totalScore > second->totalScore) {
                        if (first == &cheatP) res.cheatWins++;
                        else res.sampledWins++;
                    } else if (second->totalScore > first->totalScore) {
                        if (second == &cheatP) res.cheatWins++;
                        else res.sampledWins++;
                    } else {
                        res.draws++;
                    }
                    res.cheatScoreSum += cheatP.totalScore;
                    res.sampledScoreSum += sampP.totalScore;
                    return;
                }
            }

            std::swap(current, opponent);
        }

        lastPlay.type = CardType::Invalid;
        if (checkSpecialVictory(*first)) {
            if (first == &cheatP) res.cheatWins++;
            else res.sampledWins++;
            return;
        }
        if (checkSpecialVictory(*second)) {
            if (second == &cheatP) res.cheatWins++;
            else res.sampledWins++;
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

    res.cheatScoreSum += cheatP.totalScore;
    res.sampledScoreSum += sampP.totalScore;
    if (first->totalScore > second->totalScore) {
        if (first == &cheatP) res.cheatWins++;
        else res.sampledWins++;
    } else if (second->totalScore > first->totalScore) {
        if (second == &cheatP) res.cheatWins++;
        else res.sampledWins++;
    } else {
        res.draws++;
    }
}

int main() {
    std::ofstream out("sampler_topk_result.txt");
    auto& o = out;

    o << "===========================================================\n";
    o << "  Top-K Prescreening: Sampled vs Cheat\n";
    o << "  Fixed: samples=30, depth=6\n";
    o << "  Baseline (no prescreen): WR=45%, stepMs=235\n";
    o << "  3 combos x 20 games = 60 total\n";
    o << "===========================================================\n\n";

    o << std::left
      << std::setw(6)  << "topK"
      << std::setw(8)  << "Wins"
      << std::setw(8)  << "WR%"
      << std::setw(12) << "AvgDiff"
      << std::setw(14) << "StepMs"
      << std::setw(14) << "Speedup"
      << "\n";
    o << std::string(70, '-') << "\n";

    int sampleCount = 30;
    int depth = 6;
    int totalGames = 20;

    std::random_device rd;
    std::mt19937 rng(rd());

    int topKs[] = {4, 6, 8};

    for (int topK : topKs) {
        std::cerr << "Running topK=" << topK << " ...\n";
        GameResult res;

        auto t0 = std::chrono::steady_clock::now();
        for (int g = 0; g < totalGames; ++g) {
            runOneGame(g % 2 == 0, rng, res, depth, sampleCount, topK);
        }
        auto t1 = std::chrono::steady_clock::now();
        auto totalMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

        double wr = 100.0 * res.sampledWins / totalGames;
        double stepMs = res.sampledSteps > 0
            ? res.sampledTimeUs / (double)res.sampledSteps / 1000.0 : 0;
        double speedup = stepMs > 0 ? 235.0 / stepMs : 0;

        o << std::left
          << std::setw(6)  << topK
          << std::setw(8)  << (std::to_string(res.sampledWins) + "/" + std::to_string(totalGames))
          << std::setw(8)  << std::fixed << std::setprecision(1) << wr << "%"
          << std::setw(12) << std::fixed << std::setprecision(1)
                           << ((res.sampledScoreSum - res.cheatScoreSum) / (double)totalGames)
          << std::setw(14) << std::fixed << std::setprecision(1) << stepMs
          << std::setw(14) << std::fixed << std::setprecision(1) << speedup << "x"
          << "\n";

        std::cerr << "  Done: WR=" << wr << "% stepMs=" << stepMs
                  << " speedup=" << speedup << "x\n";
    }

    out.close();
    std::cerr << "\nDone. See sampler_topk_result.txt\n";
    return 0;
}