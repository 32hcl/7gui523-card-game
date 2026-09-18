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
#include <chrono>
#include <random>
#include <algorithm>

static std::string cardsToString(const std::vector<Card>& cards) {
    std::string r;
    for (size_t i = 0; i < cards.size(); ++i) {
        if (i > 0) r += " ";
        r += cards[i].suit + cards[i].point;
    }
    return r.empty() ? "(pass)" : r;
}

static std::vector<Card> searchBestPlayCheatWrapper(
    const Player& me,
    const Player& opp,
    const CardTypeResult& previous,
    const Deck& deck,
    int tableScore)
{
    SearchParams params;
    params.searchDepth = 4;
    return searchBestPlayCheat(me, opp, previous, deck, tableScore,
                               params.searchDepth, params);
}

static std::vector<Card> searchBestPlaySampledWrapper(
    const Player& me,
    const Player& opp,
    const CardTypeResult& previous,
    const Deck& deck,
    int tableScore,
    const CardTracker& tracker)
{
    SearchParams params;
    params.searchDepth = 4;
    int sampleCount = 10;
    return searchBestPlaySampled(me, opp, previous, deck, tableScore,
                                 tracker, params, sampleCount);
}

struct BattleStats {
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
    BattleStats& stats,
    std::ostream& log,
    int gameNo)
{
    Deck deck = createStandardDeck();
    std::shuffle(deck.cards.begin(), deck.cards.end(), rng);

    Player first = createPlayer("P1");
    Player second = createPlayer("P2");
    dealCards(first, deck, 5);
    dealCards(second, deck, 5);

    CardTracker tracker;

    Player* current = &first;
    Player* opponent = &second;
    Player* lastPlayer = nullptr;
    std::vector<Card> tableCards;
    CardTypeResult lastPlay;
    lastPlay.type = CardType::Invalid;
    lastPlay.cards.clear();
    lastPlay.keyPoint.clear();

    Player* cheatPlayer   = sampledIsFirst ? &second : &first;
    Player* sampledPlayer = sampledIsFirst ? &first  : &second;

    int safety = 0;
    while (true) {
        safety++;
        if (safety > 500) break;

        while (true) {
            int tableScore = calculateScore(tableCards);
            std::vector<Card> play;

            auto t0 = std::chrono::steady_clock::now();

            if (current == cheatPlayer) {
                play = searchBestPlayCheatWrapper(
                    *current, *opponent, lastPlay, deck, tableScore);
            } else {
                play = searchBestPlaySampledWrapper(
                    *current, *opponent, lastPlay, deck, tableScore, tracker);
            }

            auto t1 = std::chrono::steady_clock::now();
            auto durUs = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
            if (current == cheatPlayer) {
                stats.cheatTimeUs += durUs;
                stats.cheatSteps++;
            } else {
                stats.sampledTimeUs += durUs;
                stats.sampledSteps++;
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
                if (current == cheatPlayer) stats.cheatWins++;
                else stats.sampledWins++;
                return;
            }

            if (current->hand.empty()) {
                if (deck.cards.empty()) {
                    settleScoreCards(*current, tableCards);
                    settleScoreCards(*current, opponent->hand);
                    opponent->hand.clear();
                    tableCards.clear();
                    if (first.totalScore > second.totalScore) {
                        if (&first == cheatPlayer) stats.cheatWins++;
                        else stats.sampledWins++;
                    } else if (second.totalScore > first.totalScore) {
                        if (&second == cheatPlayer) stats.cheatWins++;
                        else stats.sampledWins++;
                    } else {
                        stats.draws++;
                    }
                    stats.cheatScoreSum += cheatPlayer->totalScore;
                    stats.sampledScoreSum += sampledPlayer->totalScore;
                    return;
                }
            }

            std::swap(current, opponent);
        }

        lastPlay.type = CardType::Invalid;
        lastPlay.cards.clear();
        lastPlay.keyPoint.clear();

        if (checkSpecialVictory(first)) {
            if (&first == cheatPlayer) stats.cheatWins++;
            else stats.sampledWins++;
            return;
        }
        if (checkSpecialVictory(second)) {
            if (&second == cheatPlayer) stats.cheatWins++;
            else stats.sampledWins++;
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

    stats.cheatScoreSum += first.totalScore;
    stats.sampledScoreSum += second.totalScore;

    if (first.totalScore > second.totalScore) {
        if (&first == cheatPlayer) stats.cheatWins++;
        else stats.sampledWins++;
    } else if (second.totalScore > first.totalScore) {
        if (&second == cheatPlayer) stats.cheatWins++;
        else stats.sampledWins++;
    } else {
        stats.draws++;
    }
}

int main() {
    std::ofstream out("sampler_battle_result.txt");
    auto& o = out;

    o << "========================================\n";
    o << "  Cheat (depth=4) vs Sampled (depth=4, samples=10)\n";
    o << "  40 games total\n";
    o << "========================================\n\n";

    BattleStats all;
    std::random_device rd;
    std::mt19937 rng(rd());

    int total = 40;

    auto tStart = std::chrono::steady_clock::now();

    for (int i = 0; i < total; ++i) {
        bool sampledIsFirst = (i % 2 == 0);
        runOneGame(sampledIsFirst, rng, all, o, i + 1);
        if ((i + 1) % 10 == 0) {
            std::cerr << "  " << (i + 1) << "/" << total << " games done\n";
        }
    }

    auto tEnd = std::chrono::steady_clock::now();
    auto totalMs = std::chrono::duration<double, std::milli>(tEnd - tStart).count();

    o << "\n=== Results ===\n\n";
    o << "Cheat    wins: " << all.cheatWins
      << "  (" << (100.0 * all.cheatWins / total) << "%)\n";
    o << "Sampled  wins: " << all.sampledWins
      << "  (" << (100.0 * all.sampledWins / total) << "%)\n";
    o << "Draws:         " << all.draws << "\n";
    o << "Cheat    avg score: "
      << (1.0 * all.cheatScoreSum / total) << "\n";
    o << "Sampled  avg score: "
      << (1.0 * all.sampledScoreSum / total) << "\n";

    o << "\n--- Timing ---\n";
    if (all.cheatSteps > 0) {
        o << "Cheat    avg per step: "
          << (all.cheatTimeUs / all.cheatSteps / 1000.0) << " ms\n";
    }
    if (all.sampledSteps > 0) {
        o << "Sampled  avg per step: "
          << (all.sampledTimeUs / all.sampledSteps / 1000.0) << " ms\n";
    }
    o << "Total time: " << (totalMs / 1000.0) << " s\n";

    double wr = 100.0 * all.sampledWins / total;
    o << "\n--- Verdict ---\n";
    if (wr > 50.0) {
        o << "WR = " << wr << "% > 50%: sampled stronger than cheat (BUG?)\n";
    } else if (wr >= 30.0) {
        o << "WR = " << wr << "% in [30%,50%]: reasonable (imperfect info)\n";
    } else {
        o << "WR = " << wr << "% < 30%: too weak, need more samples/depth\n";
    }

    out.close();
    std::cerr << "\nDone. See sampler_battle_result.txt\n";
    return 0;
}