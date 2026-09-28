#include "ai/cfr/cfr_player.h"
#include "ai/ai.h"
#include "ai/ai_types.h"
#include "ai/searcher/minimax.h"
#include "core/card/deck.h"
#include "core/player.h"
#include "core/card/cardtype.h"
#include "core/tracker/cardtracker.h"
#include "core/rule/score.h"
#include "core/rule/special.h"
#include <iostream>
#include <iomanip>
#include <random>
#include <string>
#include <algorithm>
#include <cstdlib>

static std::string levelName(int lv) {
    switch (lv) {
        case 0: return "CFR";
        case 1: return "AI1";
        case 2: return "AI2";
        case 3: return "AI3";
        case 4: return "AI4";
        default: return "?";
    }
}

static int parseLevel(const std::string& s) {
    if (s == "CFR" || s == "cfr" || s == "0") return 0;
    if (s == "AI1" || s == "ai1" || s == "1") return 1;
    if (s == "AI2" || s == "ai2" || s == "2") return 2;
    if (s == "AI3" || s == "ai3" || s == "3") return 3;
    if (s == "AI4" || s == "ai4" || s == "4") return 4;
    return 0;
}

struct BattleResult {
    int firstWins = 0;
    int secondWins = 0;
    int draws = 0;
    int specialWins = 0;
    long long firstScoreSum = 0;
    long long secondScoreSum = 0;
};

static std::vector<Card> choosePlayDispatch(int level, CFRPlayer* cfr,
                                             const Player& player,
                                             const Player& opponent,
                                             const CardTypeResult& lastPlay,
                                             const Deck& deck,
                                             int tableScore,
                                             CardTracker& tracker) {
    switch (level) {
        case 0: return cfr->choosePlay(player, opponent, lastPlay, deck, tableScore);
        case 1: return aiChoosePlayAI1(player, lastPlay);
        case 2: return aiChoosePlayAI2(player, opponent, lastPlay, deck, tableScore);
        case 3: return aiChoosePlayAI3(player, opponent, lastPlay, deck, tableScore, tracker);
        case 4: return searchBestPlayCheat(player, opponent, lastPlay, deck, tableScore, 6);
        default: return {};
    }
}

static void runOneGame(int levelFirst, int levelSecond, CFRPlayer* cfr,
                       std::mt19937& rng, BattleResult& result) {
    Deck deck = createStandardDeck();
    std::shuffle(deck.cards.begin(), deck.cards.end(), rng);

    Player first = createPlayer("P1");
    Player second = createPlayer("P2");

    dealCards(first, deck, 5);
    dealCards(second, deck, 5);

    CardTracker tracker;

    if (checkSpecialVictory(first)) { result.firstWins++; result.specialWins++; return; }
    if (checkSpecialVictory(second)) { result.secondWins++; result.specialWins++; return; }

    Player* current = &first;
    Player* opponent = &second;
    Player* lastPlayer = nullptr;
    std::vector<Card> tableCards;
    CardTypeResult lastPlay;
    lastPlay.type = CardType::Invalid;
    lastPlay.cards.clear();
    lastPlay.keyPoint.clear();

    int safety = 0;
    while (true) {
        if (++safety > 500) break;

        while (true) {
            int tableScore = calculateScore(tableCards);
            int curLevel = (current == &first) ? levelFirst : levelSecond;

            std::vector<Card> play = choosePlayDispatch(
                curLevel, cfr, *current, *opponent, lastPlay, deck, tableScore, tracker);

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
                    [&](const Card& h) { return h.point == c.point && h.suit == c.suit; });
                if (it != current->hand.end()) current->hand.erase(it);
            }
            for (const Card& c : play) tableCards.push_back(c);
            tracker.recordPlayed(play);
            lastPlay = parsed;
            lastPlayer = current;

            if (checkSpecialVictory(*current)) {
                if (current == &first) result.firstWins++;
                else result.secondWins++;
                result.specialWins++;
                return;
            }

            if (current->hand.empty()) {
                if (deck.cards.empty()) {
                    settleScoreCards(*current, tableCards);
                    settleScoreCards(*current, opponent->hand);
                    opponent->hand.clear();
                    tableCards.clear();
                    if (first.totalScore > second.totalScore) result.firstWins++;
                    else if (second.totalScore > first.totalScore) result.secondWins++;
                    else result.draws++;
                    result.firstScoreSum += first.totalScore;
                    result.secondScoreSum += second.totalScore;
                    return;
                }
            }

            std::swap(current, opponent);
        }

        lastPlay.type = CardType::Invalid;
        lastPlay.cards.clear();
        lastPlay.keyPoint.clear();

        if (checkSpecialVictory(first)) { result.firstWins++; result.specialWins++; return; }
        if (checkSpecialVictory(second)) { result.secondWins++; result.specialWins++; return; }

        if (!deck.cards.empty()) {
            Player* loser = (lastPlayer == &first) ? &second : &first;
            refillToFive(*lastPlayer, deck);
            refillToFive(*loser, deck);
        }

        current = lastPlayer;
        opponent = (current == &first) ? &second : &first;

        if (deck.cards.empty() && first.hand.empty() && second.hand.empty()) break;
    }

    result.firstScoreSum += first.totalScore;
    result.secondScoreSum += second.totalScore;

    if (first.totalScore > second.totalScore) result.firstWins++;
    else if (second.totalScore > first.totalScore) result.secondWins++;
    else result.draws++;
}

int main(int argc, char* argv[]) {
    int games = 100;
    int firstLevel = 0;   // CFR
    int secondLevel = 1;  // AI1
    bool verbose = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--games" || arg == "-g") {
            if (i + 1 < argc) games = std::atoi(argv[++i]);
        } else if (arg == "--first" || arg == "-1") {
            if (i + 1 < argc) firstLevel = parseLevel(argv[++i]);
        } else if (arg == "--second" || arg == "-2") {
            if (i + 1 < argc) secondLevel = parseLevel(argv[++i]);
        } else if (arg == "--verbose" || arg == "-v") {
            verbose = true;
        }
    }

    std::cout << "=== CFR Battle ===\n";
    std::cout << "First:  " << levelName(firstLevel) << "\n";
    std::cout << "Second: " << levelName(secondLevel) << "\n";
    std::cout << "Games:  " << games << "\n\n";

    CFRPlayer cfr;
    if (!cfr.loadDB("data/cfr/endgame_db.bin")) {
        std::cerr << "Failed to load endgame DB from data/cfr/endgame_db.bin\n";
        std::cerr << "Run cfr_train first to build the database.\n";
        return 1;
    }
    std::cout << "Loaded endgame DB: " << cfr.db().size() << " info sets\n";

    if (!cfr.loadStrategy("data/cfr/strategy.bin")) {
        std::cerr << "Failed to load strategy from data/cfr/strategy.bin\n";
        std::cerr << "Continuing with endgame DB only.\n";
    } else {
        std::cout << "Loaded strategy: " << cfr.strategySize() << " info sets\n";
    }

    std::mt19937 rng(std::random_device{}());
    BattleResult total;

    for (int i = 0; i < games; ++i) {
        runOneGame(firstLevel, secondLevel, &cfr, rng, total);
        if (verbose && (i % 10 == 9 || i == games - 1)) {
            std::cout << "  ... " << (i + 1) << "/" << games << " complete\r";
        }
    }
    std::cout << std::endl;

    double wr = (double)total.firstWins / games * 100.0;
    double avg1 = (double)total.firstScoreSum / games;
    double avg2 = (double)total.secondScoreSum / games;

    std::cout << "Results:\n";
    std::cout << "  " << levelName(firstLevel) << " wins: " << total.firstWins
              << " (" << std::fixed << std::setprecision(1) << wr << "%)\n";
    std::cout << "  " << levelName(secondLevel) << " wins: " << total.secondWins << "\n";
    std::cout << "  Draws: " << total.draws << "\n";
    std::cout << "  Special wins: " << total.specialWins << "\n";
    std::cout << "  " << levelName(firstLevel) << " avg score: "
              << std::fixed << std::setprecision(1) << avg1 << "\n";
    std::cout << "  " << levelName(secondLevel) << " avg score: "
              << std::fixed << std::setprecision(1) << avg2 << "\n";

    return 0;
}