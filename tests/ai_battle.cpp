#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <iomanip>
#include <map>

#include "core/deck.h"
#include "core/player.h"
#include "core/cardtype.h"
#include "core/ai.h"
#include "core/score.h"
#include "core/special.h"
#include "core/cardtracker.h"

struct GameResult {
    int winner;
    int scoreFirst;
    int scoreSecond;
    bool specialWin;
};

GameResult runOneGame(AILevel levelFirst, AILevel levelSecond, std::mt19937& rng) {
    Deck deck = createStandardDeck();
    std::shuffle(deck.cards.begin(), deck.cards.end(), rng);

    Player first = createPlayer("先手");
    first.aiLevel = levelFirst;
    Player second = createPlayer("后手");
    second.aiLevel = levelSecond;

    dealCards(first, deck, 5);
    dealCards(second, deck, 5);

    CardTracker tracker;

    if (checkSpecialVictory(first))  return {1, 0, 0, true};
    if (checkSpecialVictory(second)) return {2, 0, 0, true};

    Player* current = &first;
    Player* opponent = &second;
    Player* lastPlayer = nullptr;
    std::vector<Card> tableCards;
    CardTypeResult lastPlay;
    lastPlay.type = CardType::Invalid;
    lastPlay.cards.clear();
    lastPlay.keyPoint.clear();
    int roundCount = 0;

    while (true) {
        roundCount++;
        if (roundCount > 200) break;

        while (true) {
            int tableScore  = calculateScore(tableCards);

            std::vector<Card> play = aiChoosePlay(
                *current, *opponent, lastPlay, deck, tableScore, tracker);

            if (play.empty()) {
                if (lastPlay.type == CardType::Invalid) {
                    break;
                }
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

            if (checkSpecialVictory(*current))
                return {current == &first ? 1 : 2, 0, 0, true};

            if (current->hand.empty()) {
                Player* responder  = opponent;
                Player* finisher   = current;
                int respTableScore = calculateScore(tableCards);
                std::vector<Card> response = aiChoosePlay(
                    *responder, *finisher, lastPlay, deck, respTableScore, tracker);

                if (!response.empty()) {
                    CardTypeResult respParsed = parseCardType(response);
                    bool valid  = respParsed.type != CardType::Invalid;
                    bool beats  = lastPlay.type == CardType::Invalid
                                  || canBeat(respParsed, lastPlay);
                    if (valid && beats) {
                        for (const Card& c : response) {
                            auto it = std::find_if(responder->hand.begin(), responder->hand.end(),
                                [&](const Card& h){ return h.point == c.point && h.suit == c.suit; });
                            if (it != responder->hand.end()) responder->hand.erase(it);
                        }
                        for (const Card& c : response) tableCards.push_back(c);
                        tracker.recordPlayed(response);
                        settleScoreCards(*responder, tableCards);
                        tableCards.clear();
                        lastPlayer = responder;
                        break;
                    }
                }
                settleScoreCards(*finisher, tableCards);
                tableCards.clear();
                if (lastPlayer == nullptr) lastPlayer = finisher;
                break;
            }

            std::swap(current, opponent);
        }

        lastPlay.type = CardType::Invalid;
        lastPlay.cards.clear();
        lastPlay.keyPoint.clear();

        if (checkSpecialVictory(first))  return {1, 0, 0, true};
        if (checkSpecialVictory(second)) return {2, 0, 0, true};

        if (!deck.cards.empty()) {
            refillToFive(*lastPlayer, deck);
            refillToFive(*(lastPlayer == &first ? &second : &first), deck);
        }

        current = lastPlayer;
        opponent = (current == &first) ? &second : &first;

        if (deck.cards.empty() && first.hand.empty() && second.hand.empty()) break;
    }

finished:
    if (!first.hand.empty()) {
        settleScoreCards(second, first.hand);
        first.hand.clear();
    }
    if (!second.hand.empty()) {
        settleScoreCards(first, second.hand);
        second.hand.clear();
    }

    if (first.totalScore > second.totalScore)
        return {1, first.totalScore, second.totalScore, false};
    else if (second.totalScore > first.totalScore)
        return {2, first.totalScore, second.totalScore, false};
    else
        return {0, first.totalScore, second.totalScore, false};
}

struct BattleStats {
    int firstWins = 0;
    int secondWins = 0;
    int draws = 0;
    int specialWins = 0;
    long long scoreFirst = 0;
    long long scoreSecond = 0;
};

BattleStats runBattle(AILevel levelFirst, AILevel levelSecond, int games, std::mt19937& rng) {
    BattleStats stats;
    for (int i = 0; i < games; ++i) {
        GameResult r = runOneGame(levelFirst, levelSecond, rng);
        if (r.winner == 1) stats.firstWins++;
        else if (r.winner == 2) stats.secondWins++;
        else stats.draws++;
        if (r.specialWin) stats.specialWins++;
        stats.scoreFirst += r.scoreFirst;
        stats.scoreSecond += r.scoreSecond;
    }
    return stats;
}

std::string levelName(AILevel lv) {
    switch (lv) {
        case AILevel::AI1_Simple:  return "AI1";
        case AILevel::AI2_Rule:    return "AI2";
        case AILevel::AI3_Tracker: return "AI3";
    }
    return "?";
}

int main() {
    const int GAMES = 1000;

    std::vector<AILevel> levels = {
        AILevel::AI1_Simple,
        AILevel::AI2_Rule,
        AILevel::AI3_Tracker
    };

    std::random_device rd;
    std::mt19937 rng(rd());

    std::cout << "========================================\n";
    std::cout << "三档 AI 相互对弈测试\n";
    std::cout << "每种组合 " << GAMES << " 局\n";
    std::cout << "========================================\n\n";

    std::cout << std::left
              << std::setw(10) << "先手"
              << std::setw(10) << "后手"
              << std::setw(10) << "先手胜"
              << std::setw(10) << "后手胜"
              << std::setw(10) << "平局"
              << std::setw(10) << "特殊胜"
              << std::setw(12) << "先手均分"
              << std::setw(12) << "后手均分"
              << "\n";
    std::cout << std::string(84, '-') << "\n";

    for (AILevel lf : levels) {
        for (AILevel ls : levels) {
            BattleStats s = runBattle(lf, ls, GAMES, rng);
            std::cout << std::left
                      << std::setw(10) << levelName(lf)
                      << std::setw(10) << levelName(ls)
                      << std::setw(10) << s.firstWins
                      << std::setw(10) << s.secondWins
                      << std::setw(10) << s.draws
                      << std::setw(10) << s.specialWins
                      << std::setw(12) << std::fixed << std::setprecision(1)
                      << static_cast<double>(s.scoreFirst) / GAMES
                      << std::setw(12)
                      << static_cast<double>(s.scoreSecond) / GAMES
                      << "\n";
        }
    }

    std::cout << "\n完成。\n";
    return 0;
}