#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <fstream>
#include <sstream>

#include "core/ai.h"
#include "core/ai_params.h"
#include "core/deck.h"
#include "core/player.h"
#include "core/cardtype.h"
#include "core/cardtracker.h"
#include "core/score.h"
#include "core/special.h"

struct BattleStats {
    int firstWins = 0;
    int secondWins = 0;
    int draws = 0;
    int specialWins = 0;
    long long firstScoreSum = 0;
    long long secondScoreSum = 0;
};

static std::string levelName(AILevel lv) {
    switch (lv) {
        case AILevel::AI1_Simple:  return "AI1";
        case AILevel::AI2_Rule:    return "AI2";
        case AILevel::AI3_Tracker: return "AI3";
        case AILevel::AI4_Expert:  return "AI4";
    }
    return "?";
}

static void runOneGame(AILevel levelFirst, AILevel levelSecond,
                       std::mt19937& rng, BattleStats& stats) {
    Deck deck = createStandardDeck();
    std::shuffle(deck.cards.begin(), deck.cards.end(), rng);

    Player first = createPlayer("P1");
    first.aiLevel = levelFirst;
    Player second = createPlayer("P2");
    second.aiLevel = levelSecond;

    dealCards(first, deck, 5);
    dealCards(second, deck, 5);

    CardTracker tracker;

    if (checkSpecialVictory(first)) {
        stats.firstWins++; stats.specialWins++; return;
    }
    if (checkSpecialVictory(second)) {
        stats.secondWins++; stats.specialWins++; return;
    }

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
        safety++;
        if (safety > 500) break;

        while (true) {
            int tableScore = calculateScore(tableCards);
            std::vector<Card> play = aiChoosePlay(
                *current, *opponent, lastPlay, deck, tableScore, tracker);

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
                if (current == &first) stats.firstWins++;
                else stats.secondWins++;
                stats.specialWins++;
                return;
            }

            if (current->hand.empty()) {
                if (deck.cards.empty()) {
                    settleScoreCards(*current, tableCards);
                    settleScoreCards(*current, opponent->hand);
                    opponent->hand.clear();
                    tableCards.clear();
                    if (first.totalScore > second.totalScore) stats.firstWins++;
                    else if (second.totalScore > first.totalScore) stats.secondWins++;
                    else stats.draws++;
                    stats.firstScoreSum += first.totalScore;
                    stats.secondScoreSum += second.totalScore;
                    return;
                }
            }

            std::swap(current, opponent);
        }

        lastPlay.type = CardType::Invalid;
        lastPlay.cards.clear();
        lastPlay.keyPoint.clear();

        if (checkSpecialVictory(first)) {
            stats.firstWins++; stats.specialWins++; return;
        }
        if (checkSpecialVictory(second)) {
            stats.secondWins++; stats.specialWins++; return;
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

    stats.firstScoreSum += first.totalScore;
    stats.secondScoreSum += second.totalScore;

    if (first.totalScore > second.totalScore) stats.firstWins++;
    else if (second.totalScore > first.totalScore) stats.secondWins++;
    else stats.draws++;
}

int main(int argc, char* argv[]) {
    int gamesPerCombo = 250;
    if (argc > 1) gamesPerCombo = std::atoi(argv[1]);

    std::vector<AILevel> levels = {
        AILevel::AI1_Simple,
        AILevel::AI2_Rule,
        AILevel::AI3_Tracker,
        AILevel::AI4_Expert
    };

    std::random_device rd;
    std::mt19937 rng(rd());

    int totalGames = 16 * gamesPerCombo;

    // 同时输出到屏幕和文件
    std::ofstream ofs("battle_result.txt");
    auto output = [&](const std::string& s) {
        std::cout << s;
        ofs << s;
    };

    {
        std::ostringstream oss;
        oss << "========================================\n"
            << "四档 AI 全组合对战测试\n"
            << "16 种组合，每种 " << gamesPerCombo << " 局\n"
            << "总计 " << totalGames << " 局\n"
            << "========================================\n\n";
        output(oss.str());
    }

    {
        std::ostringstream oss;
        oss << std::left
            << std::setw(8)  << "先手"
            << std::setw(8)  << "后手"
            << std::setw(8)  << "先手胜"
            << std::setw(8)  << "后手胜"
            << std::setw(8)  << "平局"
            << std::setw(10) << "先手胜率"
            << std::setw(12) << "先手均分"
            << std::setw(12) << "后手均分"
            << "\n"
            << std::string(80, '-') << "\n";
        output(oss.str());
    }

    int combo = 0;
    for (AILevel lf : levels) {
        for (AILevel ls : levels) {
            combo++;
            BattleStats s;
            for (int i = 0; i < gamesPerCombo; ++i) {
                runOneGame(lf, ls, rng, s);
            }

            double wr   = (double)s.firstWins / gamesPerCombo;
            double avg1 = (double)s.firstScoreSum / gamesPerCombo;
            double avg2 = (double)s.secondScoreSum / gamesPerCombo;

            std::ostringstream oss;
            oss << std::left
                << std::setw(8)  << levelName(lf)
                << std::setw(8)  << levelName(ls)
                << std::setw(8)  << s.firstWins
                << std::setw(8)  << s.secondWins
                << std::setw(8)  << s.draws
                << std::setw(10) << std::fixed << std::setprecision(3) << wr
                << std::setw(12) << std::fixed << std::setprecision(1) << avg1
                << std::setw(12) << std::fixed << std::setprecision(1) << avg2
                << "\n";
            output(oss.str());
            std::cerr << "[" << combo << "/16] " << levelName(lf)
                      << " vs " << levelName(ls) << " 完成\n";
        }
    }

    output("\n完成。\n");
    ofs.close();
    std::cout << "结果已写入 battle_result.txt\n";
    return 0;
}