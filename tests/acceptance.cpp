#include <iostream>
#include <random>
#include <string>
#include <iomanip>
#include <cmath>
#include <sstream>
#include <functional>
#include "core/card/deck.h"
#include "core/player.h"
#include "core/rule/score.h"
#include "ai/ai.h"
#include "ai/searcher/minimax.h"
#include "ai/engine/ai_engine.h"
#include "ai/engine/ai_levels.h"

static std::string wilsonCI(int wins, int totalGames, double z = 1.96) {
    if (totalGames == 0) return "[?]";
    double p = (double)wins / totalGames;
    double n = (double)totalGames;
    double denom = 1.0 + z * z / n;
    double center = (p + z * z / (2.0 * n)) / denom;
    double margin = z * sqrt((p * (1.0 - p) + z * z / (4.0 * n)) / n) / denom;
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1) << (p * 100.0) << "% 95%CI["
       << std::max(0.0, (center - margin) * 100.0) << "%,"
       << std::min(100.0, (center + margin) * 100.0) << "%]";
    return ss.str();
}

static std::string playStr(const std::vector<Card>& play) {
    if (play.empty()) return "PASS";
    std::string s;
    for (auto& c : play) s += c.point + c.suit + " ";
    return s;
}

static std::string handStr(const std::vector<Card>& h) {
    std::string s;
    for (auto& c : h) s += c.point + c.suit + " ";
    return s.empty() ? "(empty)" : s;
}

static bool playEq(const std::vector<Card>& a, const std::vector<Card>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (a[i].point != b[i].point || a[i].suit != b[i].suit) return false;
    return true;
}

struct Result { int wins = 0; int losses = 0; int draws = 0; double scoreSum = 0.0; };

using ChooseFn = std::function<std::vector<Card>(Player&,Player&,CardTypeResult&,Deck&,int)>;

static void runOneGame(Player& p1, Player& p2, Deck& deck, ChooseFn f1, ChooseFn f2) {
    Player* cur = &p1;
    Player* opp = &p2;
    Player* lastP = nullptr;
    std::vector<Card> table;
    CardTypeResult lp = CardTypeResult{};
    int sfty = 0;
    while (sfty++ < 500) {
        while (true) {
            int ts = calculateScore(table);
            auto chosen = (cur == &p1) ? f1(*cur, *opp, lp, deck, ts) : f2(*cur, *opp, lp, deck, ts);
            if (chosen.empty()) {
                if (lp.type == CardType::Invalid) break;
                settleScoreCards(*lastP, table);
                table.clear();
                break;
            }
            auto prs = parseCardType(chosen);
            if (prs.type == CardType::Invalid) break;
            if (lp.type != CardType::Invalid && !canBeat(prs, lp)) break;
            for (auto& c : chosen) {
                auto it = std::find_if(cur->hand.begin(), cur->hand.end(),
                    [&](const Card& h) { return h.point == c.point && h.suit == c.suit; });
                if (it != cur->hand.end()) cur->hand.erase(it);
            }
            for (auto& c : chosen) table.push_back(c);
            lp = prs;
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
        lp.type = CardType::Invalid;
        lp.cards.clear();
        lp.keyPoint.clear();
        if (!deck.cards.empty()) {
            Player* loser = (lastP == &p1) ? &p2 : &p1;
            refillToFive(*lastP, deck);
            refillToFive(*loser, deck);
        }
        if (p1.hand.empty() || p2.hand.empty()) break;
    }
}

// ===== Problem 1 =====
void runBenchmarks() {
    std::cout << "## Problem 1: 600-game protocol" << std::endl;
    int seeds[] = {20260918, 20260919, 20260920};
    const int N = 200;
    Result newAll, oldAll;

    for (int si = 0; si < 3; ++si) {
        int seed = seeds[si];
        std::cout << "\nSeed " << seed << ":" << std::endl;

        // New W7 vs W4
        {
            Result r;
            std::mt19937 rng(seed);
            SimpleStateEvaluator seval;
            MinimaxSearcher w7(6, &seval);
            for (int g = 0; g < N; ++g) {
                AIEngine w4eng(buildAIEngineConfig(4));
                Deck d = createStandardDeck();
                shuffle(d.cards.begin(), d.cards.end(), rng);
                Player p1 = createPlayer("P1");
                Player p2 = createPlayer("P2");
                dealCards(p1, d, 5);
                dealCards(p2, d, 5);
                w4eng.setOpponentHand(p1.hand);
                runOneGame(p1, p2, d,
                    [&](Player& a, Player& b, CardTypeResult& pr, Deck& dk, int ts) {
                        return w7.search(a, b, pr, dk, ts, nullptr, nullptr);
                    },
                    [&](Player& a, Player& b, CardTypeResult& pr, Deck& dk, int ts) {
                        return w4eng.choosePlay(a, b, pr, dk, ts);
                    });
                if (p1.totalScore > p2.totalScore) r.wins++;
                else if (p1.totalScore < p2.totalScore) r.losses++;
                else r.draws++;
                r.scoreSum += p1.totalScore;
            }
            newAll.wins += r.wins;
            newAll.losses += r.losses;
            newAll.draws += r.draws;
            newAll.scoreSum += r.scoreSum;
            std::cout << "  New W7 vs W4: " << r.wins << "W " << r.losses << "L " << r.draws << "D  "
                      << wilsonCI(r.wins, N) << " avg=" << std::fixed << std::setprecision(1)
                      << (r.scoreSum / N) << std::endl;
        }

        // Old AI4 vs AI3
        {
            Result r;
            std::mt19937 rng(seed);
            for (int g = 0; g < N; ++g) {
                CardTracker tracker;
                Deck d = createStandardDeck();
                shuffle(d.cards.begin(), d.cards.end(), rng);
                Player p1 = createPlayer("P1");
                Player p2 = createPlayer("P2");
                dealCards(p1, d, 5);
                dealCards(p2, d, 5);
                runOneGame(p1, p2, d,
                    [](Player& a, Player& b, CardTypeResult& pr, Deck& dk, int ts) {
                        return searchBestPlayCheat(a, b, pr, dk, ts, 6);
                    },
                    [&tracker](Player& a, Player& b, CardTypeResult& pr, Deck& dk, int ts) {
                        auto pl = aiChoosePlayAI3(a, b, pr, dk, ts, tracker);
                        tracker.recordPlayed(pl);
                        return pl;
                    });
                if (p1.totalScore > p2.totalScore) r.wins++;
                else if (p1.totalScore < p2.totalScore) r.losses++;
                else r.draws++;
                r.scoreSum += p1.totalScore;
            }
            oldAll.wins += r.wins;
            oldAll.losses += r.losses;
            oldAll.draws += r.draws;
            oldAll.scoreSum += r.scoreSum;
            std::cout << "  Old AI4 vs AI3: " << r.wins << "W " << r.losses << "L " << r.draws << "D  "
                      << wilsonCI(r.wins, N) << " avg=" << std::fixed << std::setprecision(1)
                      << (r.scoreSum / N) << std::endl;
        }
    }

    int tN = newAll.wins + newAll.losses + newAll.draws;
    int tO = oldAll.wins + oldAll.losses + oldAll.draws;
    std::cout << "\n  New W7 total: " << newAll.wins << "W " << newAll.losses << "L "
              << newAll.draws << "D  " << wilsonCI(newAll.wins, tN)
              << " avg=" << (newAll.scoreSum / tN) << std::endl;
    std::cout << "  Old AI4 total: " << oldAll.wins << "W " << oldAll.losses << "L "
              << oldAll.draws << "D  " << wilsonCI(oldAll.wins, tO)
              << " avg=" << (oldAll.scoreSum / tO) << std::endl;

    double pN = (double)newAll.wins / tN;
    double pO = (double)oldAll.wins / tO;
    double se = sqrt(pN * (1 - pN) / tN + pO * (1 - pO) / tO);
    double z = (pN - pO) / (se + 1e-10);
    std::cout << "  Statistical consistency: z=" << std::fixed << std::setprecision(3) << z
              << (fabs(z) < 1.96 ? " consistent (|z|<1.96)" : " significant diff (|z|>=1.96)") << std::endl;
}

// ===== Problem 2 =====
void runMismatchCapture() {
    std::cout << "\n## Problem 2: Engine-level mismatches (from tryFinishPlay in choosePlay)" << std::endl;
    std::mt19937 rng(42);
    int turnNo = 0;
    int totalCompare = 0;
    int totalMismatch = 0;
    int sameResult = 0;

    struct Mismatch {
        int gameNo, turnNo;
        std::string p1Hand, p2Hand, tableState, oldPlay, newPlay;
        bool newWonRound;
        int newScore, oldScore;
    };

    for (int g = 0; g < 100; ++g) {
        std::vector<Mismatch> localM;
        CardTracker trOld;
        Deck d = createStandardDeck();
        shuffle(d.cards.begin(), d.cards.end(), rng);
        Player p1 = createPlayer("P1");
        Player p2 = createPlayer("P2");
        dealCards(p1, d, 5);
        dealCards(p2, d, 5);
        Player p1old = p1;
        Player p2old = p2;
        Deck dold = d;

        AIEngine w7eng(buildAIEngineConfig(7));
        w7eng.setOpponentHand(p2.hand);
        AIEngine w4eng(buildAIEngineConfig(4));
        w4eng.setOpponentHand(p1.hand);

        Player* cur = &p1;
        Player* opp = &p2;
        Player* lastP = nullptr;
        std::vector<Card> tbl;
        CardTypeResult lp;
        lp.type = CardType::Invalid;
        int sfty = 0;

        while (sfty++ < 500) {
            while (true) {
                int ts = calculateScore(tbl);
                std::vector<Card> chosen;

                if (cur == &p1) {
                    chosen = w7eng.choosePlay(*cur, *opp, lp, d, ts);
                    auto old = searchBestPlayCheat(*cur, *opp, lp, d, ts, 6);
                    totalCompare++;
                    if (!playEq(chosen, old)) {
                        totalMismatch++;
                        Mismatch mm;
                        mm.gameNo = g + 1;
                        mm.turnNo = turnNo;
                        mm.p1Hand = handStr(p1.hand);
                        mm.p2Hand = handStr(p2.hand);
                        mm.tableState = (lp.type == CardType::Invalid ? "lead" : playStr(lp.cards));
                        mm.oldPlay = playStr(old);
                        mm.newPlay = playStr(chosen);
                        mm.newWonRound = (chosen.size() == cur->hand.size());
                        localM.push_back(mm);
                    }
                    turnNo++;
                } else {
                    chosen = w4eng.choosePlay(*cur, *opp, lp, d, ts);
                }

                if (chosen.empty()) {
                    if (lp.type == CardType::Invalid) break;
                    settleScoreCards(*lastP, tbl);
                    tbl.clear();
                    break;
                }
                auto prs = parseCardType(chosen);
                if (prs.type == CardType::Invalid) break;
                if (lp.type != CardType::Invalid && !canBeat(prs, lp)) break;
                for (auto& c : chosen) {
                    auto it = std::find_if(cur->hand.begin(), cur->hand.end(),
                        [&](const Card& h) { return h.point == c.point && h.suit == c.suit; });
                    if (it != cur->hand.end()) cur->hand.erase(it);
                }
                for (auto& c : chosen) tbl.push_back(c);
                lp = prs;
                lastP = cur;
                if (cur->hand.empty()) {
                    if (d.cards.empty()) {
                        settleScoreCards(*cur, tbl);
                        settleScoreCards(*cur, opp->hand);
                        opp->hand.clear();
                        tbl.clear();
                        break;
                    }
                }
                if (cur->hand.empty()) break;
                std::swap(cur, opp);
            }
            lp.type = CardType::Invalid;
            lp.cards.clear();
            lp.keyPoint.clear();
            if (!d.cards.empty()) {
                Player* loser = (lastP == &p1) ? &p2 : &p1;
                refillToFive(*lastP, d);
                refillToFive(*loser, d);
            }
            if (p1.hand.empty() || p2.hand.empty()) break;
        }

        int newP1Score = p1.totalScore;

        // old replay
        {
            Player f = p1old;
            Player s = p2old;
            Deck dd = dold;
            Player* c = &f;
            Player* o = &s;
            Player* lpp = nullptr;
            std::vector<Card> tbl;
            CardTypeResult lppl;
            lppl.type = CardType::Invalid;
            int sf = 0;
            while (sf++ < 500) {
                while (true) {
                    int ts = calculateScore(tbl);
                    std::vector<Card> pl;
                    if (c == &f) {
                        pl = searchBestPlayCheat(*c, *o, lppl, dd, ts, 6);
                    } else {
                        pl = aiChoosePlayAI3(*c, *o, lppl, dd, ts, trOld);
                        trOld.recordPlayed(pl);
                    }
                    if (pl.empty()) {
                        if (lppl.type == CardType::Invalid) break;
                        settleScoreCards(*lpp, tbl);
                        tbl.clear();
                        break;
                    }
                    auto prs = parseCardType(pl);
                    if (prs.type == CardType::Invalid) break;
                    if (lppl.type != CardType::Invalid && !canBeat(prs, lppl)) break;
                    for (auto& cd : pl) {
                        auto it = std::find_if(c->hand.begin(), c->hand.end(),
                            [&](const Card& h) { return h.point == cd.point && h.suit == cd.suit; });
                        if (it != c->hand.end()) c->hand.erase(it);
                    }
                    for (auto& cd : pl) tbl.push_back(cd);
                    lppl = prs;
                    lpp = c;
                    if (c->hand.empty()) {
                        if (dd.cards.empty()) {
                            settleScoreCards(*c, tbl);
                            settleScoreCards(*c, o->hand);
                            o->hand.clear();
                            tbl.clear();
                            break;
                        }
                    }
                    if (c->hand.empty()) break;
                    std::swap(c, o);
                }
                lppl.type = CardType::Invalid;
                lppl.cards.clear();
                lppl.keyPoint.clear();
                if (!dd.cards.empty()) {
                    Player* loser = (lpp == &f) ? &s : &f;
                    refillToFive(*lpp, dd);
                    refillToFive(*loser, dd);
                }
                if (f.hand.empty() || s.hand.empty()) break;
            }
            bool newWon = newP1Score > p2.totalScore;
            bool oldWon = f.totalScore > s.totalScore;
            if (newWon == oldWon) sameResult++;

            for (auto& mm : localM) {
                mm.newScore = newP1Score;
                mm.oldScore = f.totalScore;
                std::cout << "  Game#" << mm.gameNo
                          << " turn#" << mm.turnNo
                          << "\n    P1 hand: " << mm.p1Hand
                          << "\n    P2 hand: " << mm.p2Hand
                          << "\n    table: " << mm.tableState
                          << "\n    old play: " << mm.oldPlay
                          << "\n    new play: " << mm.newPlay
                          << "\n    round outcome: new " << (mm.newWonRound ? "WON (emptied hand)" : "continued")
                          << "\n    game outcome: old AI4 score=" << mm.oldScore
                          << " new W7 score=" << mm.newScore
                          << " same? " << ((newWon == oldWon) ? "YES" : "NO") << std::endl;
            }
        }
    }
    std::cout << "\n  Total compared: " << totalCompare << ", mismatches: " << totalMismatch
              << ", result-consistent games: " << sameResult << "/100" << std::endl;
}

// ===== Problem 3 =====
void runW4vsW3() {
    std::cout << "\n## Problem 3: W4 vs W3 (seat balanced)" << std::endl;
    Result r;
    std::mt19937 rng(20260918);

    for (int g = 0; g < 200; ++g) {
        AIEngine w4(buildAIEngineConfig(4));
        AIEngine w3(buildAIEngineConfig(3));
        Deck d = createStandardDeck();
        shuffle(d.cards.begin(), d.cards.end(), rng);
        Player p1 = createPlayer("P1");
        Player p2 = createPlayer("P2");
        dealCards(p1, d, 5);
        dealCards(p2, d, 5);
        w4.setOpponentHand(p2.hand);
        w3.setOpponentHand(p1.hand);
        runOneGame(p1, p2, d,
            [&](Player& a, Player& b, CardTypeResult& pr, Deck& dk, int ts) {
                return w4.choosePlay(a, b, pr, dk, ts);
            },
            [&](Player& a, Player& b, CardTypeResult& pr, Deck& dk, int ts) {
                return w3.choosePlay(a, b, pr, dk, ts);
            });
        if (p1.totalScore > p2.totalScore) r.wins++;
        else if (p1.totalScore < p2.totalScore) r.losses++;
        else r.draws++;
        r.scoreSum += p1.totalScore;
    }
    std::cout << "  W4 first vs W3: " << r.wins << "W " << r.losses << "L " << r.draws << "D  "
              << wilsonCI(r.wins, 200) << " avg=" << (r.scoreSum / 200) << std::endl;

    Result r2;
    std::mt19937 rng2(20260918);
    for (int g = 0; g < 200; ++g) {
        AIEngine w4(buildAIEngineConfig(4));
        AIEngine w3(buildAIEngineConfig(3));
        Deck d = createStandardDeck();
        shuffle(d.cards.begin(), d.cards.end(), rng2);
        Player p1 = createPlayer("P1");
        Player p2 = createPlayer("P2");
        dealCards(p1, d, 5);
        dealCards(p2, d, 5);
        w4.setOpponentHand(p2.hand);
        w3.setOpponentHand(p1.hand);
        runOneGame(p1, p2, d,
            [&](Player& a, Player& b, CardTypeResult& pr, Deck& dk, int ts) {
                return w3.choosePlay(a, b, pr, dk, ts);
            },
            [&](Player& a, Player& b, CardTypeResult& pr, Deck& dk, int ts) {
                return w4.choosePlay(a, b, pr, dk, ts);
            });
        if (p1.totalScore > p2.totalScore) r2.wins++;
        else if (p1.totalScore < p2.totalScore) r2.losses++;
        else r2.draws++;
        r2.scoreSum += p1.totalScore;
    }
    std::cout << "  W3 first vs W4: " << r2.wins << "W " << r2.losses << "L " << r2.draws << "D  "
              << "W3 wr=" << wilsonCI(r2.wins, 200) << std::endl;

    int w4W = r.wins + r2.losses;
    int w3W = r.losses + r2.wins;
    int draws = r.draws + r2.draws;
    std::cout << "  W4 total: " << w4W << "W " << w3W << "L " << draws << "D  "
              << wilsonCI(w4W, 400) << " avg=" << ((r.scoreSum + (200 - r2.scoreSum)) / 400) << std::endl;
}

int main() {
    runBenchmarks();
    runMismatchCapture();
    runW4vsW3();
    return 0;
}