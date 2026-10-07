#include "ai/plugins/bayesian_sampler.h"
#include "core/card/deck.h"
#include "core/player.h"
#include <iostream>
#include <fstream>
#include <map>
#include <iomanip>

int main() {
    std::ofstream out("bayesian_test_result.txt");
    auto& o = out;

    o << "===========================================================\n";
    o << "  Bayesian Sampler Validation\n";
    o << "===========================================================\n\n";

    // ========== Test 1: Decay direction ==========
    o << "--- Test 1: Weight order (rounds=5) ---\n";
    o << "Expected: 7/鬼 > 5/2/3 > 10/K > 4/6/8/9\n";

    struct { const char* pt; double d; } expected[] = {
        {"7", 0.95}, {"大鬼", 0.95}, {"小鬼", 0.95},
        {"5", 0.92}, {"2", 0.92}, {"3", 0.92},
        {"10", 0.85}, {"K", 0.85},
        {"4", 0.80}, {"6", 0.80}, {"8", 0.80}, {"9", 0.80},
    };

    // Trick: create tracker with no played cards, so all cards in pool
    // Call the internal decay indirectly via sampling
    // For direct verification, we just check the sample distribution

    // ========== Test 2: Exhausted cards excluded ==========
    o << "\n--- Test 2: Exhausted cards excluded ---\n";
    {
        CardTracker tracker;
        Card bigJoker; bigJoker.point = "大鬼"; bigJoker.suit = "";
        Card smallJoker; smallJoker.point = "小鬼"; smallJoker.suit = "";
        tracker.recordPlayed({bigJoker, smallJoker});

        std::vector<Card> myHand;
        Card c1; c1.point = "A"; c1.suit = "黑桃";
        myHand.push_back(c1);

        auto samples = sampleOpponentHandsBayesian(
            tracker, myHand, 5, 100, 3);

        int ghostCount = 0;
        for (const auto& s : samples) {
            for (const auto& c : s.hand) {
                if (c.point == "大鬼" || c.point == "小鬼") ghostCount++;
            }
        }
        bool pass = (ghostCount == 0);
        o << "  Ghost cards in samples: " << ghostCount
          << "  -> " << (pass ? "PASS" : "FAIL") << "\n";
    }

    // ========== Test 3: Weight distribution ==========
    o << "\n--- Test 3: Weight distribution (1000 samples, rounds=5) ---\n";
    o << "Expected: 7/鬼 appear more than 4/6/8/9\n";
    {
        CardTracker tracker;
        std::vector<Card> myHand;

        auto samples = sampleOpponentHandsBayesian(
            tracker, myHand, 5, 1000, 5);

        std::map<std::string, int> counts;
        int total = 0;
        for (const auto& s : samples) {
            for (const auto& c : s.hand) {
                counts[c.point]++;
                total++;
            }
        }

        // Group by category
        double ghostFreq = 0, bigFreq = 0, scoreFreq = 0, smallFreq = 0;
        int ghostN = 0, bigN = 0, scoreN = 0, smallN = 0;

        for (const auto& p : counts) {
            double freq = 100.0 * p.second / total;
            if (p.first == "7" || p.first == "大鬼" || p.first == "小鬼") {
                ghostFreq += freq; ghostN++;
            } else if (p.first == "5" || p.first == "2" || p.first == "3") {
                bigFreq += freq; bigN++;
            } else if (p.first == "10" || p.first == "K") {
                scoreFreq += freq; scoreN++;
            } else if (p.first == "4" || p.first == "6" ||
                       p.first == "8" || p.first == "9") {
                smallFreq += freq; smallN++;
            }
        }

        double ghostAvg = ghostN > 0 ? ghostFreq / ghostN : 0;
        double bigAvg   = bigN   > 0 ? bigFreq   / bigN   : 0;
        double scoreAvg = scoreN > 0 ? scoreFreq / scoreN : 0;
        double smallAvg = smallN > 0 ? smallFreq / smallN : 0;

        o << std::fixed << std::setprecision(2);
        o << "  Ghost(7/鬼) avg freq: " << ghostAvg << "%\n";
        o << "  Big  (5/2/3) avg freq: " << bigAvg << "%\n";
        o << "  Score(10/K) avg freq: " << scoreAvg << "%\n";
        o << "  Small(4/6/8/9)avg freq: " << smallAvg << "%\n";

        bool pass = (ghostAvg > smallAvg);
        o << "  Ghost > Small: " << (pass ? "PASS" : "FAIL") << "\n";

        o << "\nPer-point breakdown:\n";
        o << std::setw(6) << "Point" << std::setw(10) << "Count"
          << std::setw(10) << "Freq%\n";
        for (const auto& p : counts) {
            o << std::setw(6) << p.first
              << std::setw(10) << p.second
              << std::setw(10) << 100.0 * p.second / total << "\n";
        }
    }

    // ========== Test 4: Round effect ==========
    o << "\n--- Test 4: Round decay effect ---\n";
    o << "Expected: rounds=0 all equal, rounds=10 big gap\n";
    {
        CardTracker tracker;
        std::vector<Card> myHand;

        for (int rounds : {0, 5, 10}) {
            auto samples = sampleOpponentHandsBayesian(
                tracker, myHand, 5, 500, rounds);

            std::map<std::string, int> counts;
            int total = 0;
            for (const auto& s : samples) {
                for (const auto& c : s.hand) {
                    counts[c.point]++; total++;
                }
            }

            int ghostSum = counts["7"] + counts["大鬼"] + counts["小鬼"];
            int smallSum = counts["4"] + counts["6"] + counts["8"] + counts["9"];
            double ratio = smallSum > 0 ? (double)ghostSum / smallSum : 0;

            o << "  rounds=" << rounds
              << "  ghost=" << ghostSum << " small=" << smallSum
              << "  ratio=" << std::fixed << std::setprecision(3) << ratio << "\n";
        }
    }

    out.close();
    std::cerr << "Done. See bayesian_test_result.txt\n";
    return 0;
}