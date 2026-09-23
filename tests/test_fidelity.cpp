#include <iostream>
#include <random>
#include <string>
#include "core/card/deck.h"
#include "core/player.h"
#include "core/rule/score.h"
#include "ai/ai.h"
#include "ai/searcher/minimax.h"
#include "ai/engine/searcher.h"
#include "ai/engine/state_evaluator.h"

static std::string playStr(const std::vector<Card>& play) {
    if (play.empty()) return "PASS";
    std::string s;
    for (auto& c : play) s += c.point + c.suit + " ";
    return s;
}

static bool playEq(const std::vector<Card>& a, const std::vector<Card>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i].point != b[i].point || a[i].suit != b[i].suit) return false;
    }
    return true;
}

int main() {
    const int totalGames = 100;

    std::mt19937 rng(42);

    // ── New searcher (equivalent of old ai4's searchBestPlayCheat) ──
    SimpleStateEvaluator stateEval;
    MinimaxSearcher newSearcher(6, &stateEval);

    int turnsMatched = 0, turnsTotal = 0;

    for (int g = 0; g < totalGames; ++g) {
        Deck deck = createStandardDeck();
        std::shuffle(deck.cards.begin(), deck.cards.end(), rng);

        Player first = createPlayer("P1");
        Player second = createPlayer("P2");
        dealCards(first, deck, 5);
        dealCards(second, deck, 5);

        Player* cur = &first, *opp = &second, *lastP = nullptr;
        std::vector<Card> table;
        CardTypeResult lastPlay; lastPlay.type = CardType::Invalid;

        int safety = 0;
        while (safety++ < 500) {
            while (true) {
                int ts = calculateScore(table);
                std::vector<Card> chosen;

                if (cur == &first) {
                    // Compare: new searcher vs old searchBestPlayCheat on SAME state
                    auto newPlay = newSearcher.search(*cur, *opp, lastPlay, deck, ts, nullptr, nullptr);
                    auto oldPlay = searchBestPlayCheat(*cur, *opp, lastPlay, deck, ts, 6);

                    turnsTotal++;
                    if (playEq(newPlay, oldPlay)) {
                        turnsMatched++;
                    } else {
                        std::cout << "  G" << (g+1) << " MISMATCH: new=" << playStr(newPlay)
                                  << " old=" << playStr(oldPlay) << std::endl;
                    }
                    chosen = newPlay;
                } else {
                    // Opponent uses old AI3 equivalent (Smart + NoSearcher) for both paths
                    // We don't compare here, just advance using new engine for consistency
                    auto allPlays = enumerateLegalPlays(*cur);
                    if (allPlays.empty()) { chosen = {}; }
                    else {
                        // Simple opponent: play first beatable card, or pass
                        bool found = false;
                        for (auto& p : allPlays) {
                            auto parsed = parseCardType(p);
                            if (canBeat(parsed, lastPlay)) { chosen = p; found = true; break; }
                        }
                        if (!found) chosen = {};
                    }
                }

                if (chosen.empty()) {
                    if (lastPlay.type == CardType::Invalid) break;
                    settleScoreCards(*lastP, table);
                    table.clear();
                    break;
                }

                CardTypeResult parsed = parseCardType(chosen);
                if (parsed.type == CardType::Invalid) break;
                if (lastPlay.type != CardType::Invalid && !canBeat(parsed, lastPlay)) break;

                for (const auto& c : chosen) {
                    auto it = std::find_if(cur->hand.begin(), cur->hand.end(),
                        [&](const Card& h) { return h.point == c.point && h.suit == c.suit; });
                    if (it != cur->hand.end()) cur->hand.erase(it);
                }
                for (const auto& c : chosen) table.push_back(c);
                lastPlay = parsed;
                lastP = cur;

                if (cur->hand.empty()) {
                    if (deck.cards.empty()) {
                        settleScoreCards(*cur, table);
                        settleScoreCards(*cur, opp->hand);
                        opp->hand.clear();
                        table.clear();
                        break;
                    }
                }
                if (cur->hand.empty()) break;
                std::swap(cur, opp);
            }

            lastPlay.type = CardType::Invalid; lastPlay.cards.clear(); lastPlay.keyPoint.clear();

            if (!deck.cards.empty()) {
                Player* loser = (lastP == &first) ? &second : &first;
                refillToFive(*lastP, deck);
                refillToFive(*loser, deck);
            }

            if (first.hand.empty() || second.hand.empty()) break;
        }

        if ((g + 1) % 20 == 0 || (g + 1) == 1 || (g + 1) == totalGames) {
            std::cout << "[" << (g + 1) << "/" << totalGames
                      << "] match: " << turnsMatched << "/" << turnsTotal
                      << " (" << (turnsMatched * 100.0 / std::max(1, turnsTotal)) << "%)" << std::endl;
        }
    }

    std::cout << "\n===== Behavior Preservation Verification (Searcher vs searchBestPlayCheat) =====" << std::endl;
    std::cout << "Decision match: " << turnsMatched << "/" << turnsTotal
              << " (" << (turnsMatched * 100.0 / std::max(1, turnsTotal)) << "%)" << std::endl;

    bool pass = (turnsMatched == turnsTotal);
    std::cout << "\n" << (pass ? "PASS: Searcher 100% = old AI4!" : "FAIL: Differences found!") << std::endl;
    return pass ? 0 : 1;
}