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
    int sampleCount)
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
                                             params, sampleCount);
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

struct SweepEntry {
    int sampleCount;
    int depth;
    int wins;
    double winRate;
    double avgScoreDiff;
    double avgStepMs;
    bool skipped = false;
    std::string skipReason;
};

int main() {
    std::ofstream out("sampler_sweep_result.txt");
    auto& o = out;

    o << "============================================================\n";
    o << "  Parameter Sweep: Sampled (uniform) vs Cheat\n";
    o << "  6 combos x 40 games = 240 total\n";
    o << "============================================================\n\n";

    o << std::left
      << std::setw(10) << "Samples"
      << std::setw(6)  << "Depth"
      << std::setw(8)  << "Wins"
      << std::setw(8)  << "WR%"
      << std::setw(12) << "AvgDiff"
      << std::setw(14) << "StepMs"
      << std::setw(12) << "TotalMs"
      << "  Note\n";
    o << std::string(80, '-') << "\n";

    std::vector<SweepEntry> results;

    int configs[6][2] = {
        {10, 4}, {10, 6},
        {30, 4}, {30, 6},
        {50, 4}, {50, 6}
    };

    std::random_device rd;
    std::mt19937 rng(rd());
    int totalGames = 40;

    for (auto& cfg : configs) {
        int sampleCount = cfg[0];
        int depth       = cfg[1];

        std::cerr << "Running: samples=" << sampleCount
                  << " depth=" << depth << " ..." << std::endl;

        GameResult res;

        int quickTest = 3;
        long long quickUs = 0;
        GameResult qr;
        {
            auto qt0 = std::chrono::steady_clock::now();
            for (int g = 0; g < quickTest; ++g) {
                runOneGame(g % 2 == 0, rng, qr, depth, sampleCount);
            }
            auto qt1 = std::chrono::steady_clock::now();
            quickUs = std::chrono::duration_cast<std::chrono::microseconds>(qt1 - qt0).count();
        }

        double estPerStep = 0;
        if (qr.sampledSteps > 0) {
            estPerStep = (double)qr.sampledTimeUs / qr.sampledSteps / 1000.0;
        }

        SweepEntry entry;
        entry.sampleCount = sampleCount;
        entry.depth = depth;

        if (estPerStep > 500.0) {
            entry.skipped = true;
            entry.skipReason = "per-step > 500ms";
            results.push_back(entry);
            o << std::left
              << std::setw(10) << sampleCount
              << std::setw(6)  << depth
              << std::setw(8)  << "-"
              << std::setw(8)  << "-"
              << std::setw(12) << "-"
              << std::setw(14) << "-"
              << std::setw(12) << "-"
              << "  SKIPPED (>500ms/step)\n";
            continue;
        }

        auto t0 = std::chrono::steady_clock::now();
        for (int g = 0; g < totalGames; ++g) {
            runOneGame(g % 2 == 0, rng, res, depth, sampleCount);
            if ((g + 1) % 10 == 0) {
                std::cerr << "  " << (g + 1) << "/" << totalGames << "\n";
            }
        }
        auto t1 = std::chrono::steady_clock::now();
        auto totalMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

        entry.wins = res.sampledWins;
        entry.winRate = 100.0 * res.sampledWins / totalGames;
        entry.avgScoreDiff = (res.sampledScoreSum - res.cheatScoreSum) / (double)totalGames;
        if (res.sampledSteps > 0) {
            entry.avgStepMs = res.sampledTimeUs / (double)res.sampledSteps / 1000.0;
        }

        results.push_back(entry);

        o << std::left
          << std::setw(10) << sampleCount
          << std::setw(6)  << depth
          << std::setw(8)  << (std::to_string(res.sampledWins) + "/" + std::to_string(totalGames))
          << std::setw(8)  << std::fixed << std::setprecision(1) << entry.winRate
          << std::setw(12) << std::fixed << std::setprecision(1) << entry.avgScoreDiff
          << std::setw(14) << std::fixed << std::setprecision(1) << entry.avgStepMs
          << std::setw(12) << std::fixed << std::setprecision(0) << totalMs
          << "\n";

        std::cerr << "  Done: WR=" << entry.winRate
                  << "% stepMs=" << entry.avgStepMs
                  << " total=" << totalMs << "ms\n";
    }

    o << "\n=== Analysis ===\n\n";

    o << "1. Sampling count effect (depth=4):\n";
    for (auto& e : results) {
        if (!e.skipped && e.depth == 4) {
            o << "   samples=" << e.sampleCount
              << "  WR=" << e.winRate << "%  stepMs=" << e.avgStepMs << "\n";
        }
    }

    o << "\n2. Sampling count effect (depth=6):\n";
    for (auto& e : results) {
        if (!e.skipped && e.depth == 6) {
            o << "   samples=" << e.sampleCount
              << "  WR=" << e.winRate << "%  stepMs=" << e.avgStepMs << "\n";
        }
    }

    o << "\n3. Depth effect (samples=10):\n";
    for (auto& e : results) {
        if (!e.skipped && e.sampleCount == 10) {
            o << "   depth=" << e.depth
              << "  WR=" << e.winRate << "%  stepMs=" << e.avgStepMs << "\n";
        }
    }

    o << "\n4. Depth effect (samples=30):\n";
    for (auto& e : results) {
        if (!e.skipped && e.sampleCount == 30) {
            o << "   depth=" << e.depth
              << "  WR=" << e.winRate << "%  stepMs=" << e.avgStepMs << "\n";
        }
    }

    o << "\n5. Depth effect (samples=50):\n";
    for (auto& e : results) {
        if (!e.skipped && e.sampleCount == 50) {
            o << "   depth=" << e.depth
              << "  WR=" << e.winRate << "%  stepMs=" << e.avgStepMs << "\n";
        }
    }

    double bestWR = 0;
    SweepEntry bestEntry;
    for (auto& e : results) {
        if (!e.skipped && e.winRate > bestWR) {
            bestWR = e.winRate;
            bestEntry = e;
        }
    }

    o << "\n=== Verdict ===\n\n";
    o << "Best config: samples=" << bestEntry.sampleCount
      << " depth=" << bestEntry.depth
      << " WR=" << bestEntry.winRate << "%"
      << " stepMs=" << bestEntry.avgStepMs << "\n\n";

    out.close();
    std::cerr << "\nDone. See sampler_sweep_result.txt\n";
    return 0;
}