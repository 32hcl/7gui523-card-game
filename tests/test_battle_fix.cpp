#include <iostream>
#include <random>
#include "core/card/deck.h"
#include "core/player.h"
#include "core/rule/score.h"
#include "core/tracker/cardtracker.h"
#include "ai/engine/ai_engine.h"
#include "ai/engine/ai_levels.h"

int main() {
    const int levelFirst = 7;
    const int levelSecond = 4;
    const int totalGames = 200;

    std::mt19937 rng(42);
    int firstWins = 0, secondWins = 0, draws = 0;
    double firstScoreSum = 0, secondScoreSum = 0;

    for (int g = 0; g < totalGames; ++g) {
        Deck deck = createStandardDeck();
        std::shuffle(deck.cards.begin(), deck.cards.end(), rng);

        Player first = createPlayer("P1");
        Player second = createPlayer("P2");
        dealCards(first, deck, 5);
        dealCards(second, deck, 5);

        CardTracker tracker;

        AIEngine engineFirst(buildAIEngineConfig(levelFirst));
        AIEngine engineSecond(buildAIEngineConfig(levelSecond));
        engineFirst.setOpponentHand(second.hand);
        engineSecond.setOpponentHand(first.hand);

        Player* current = &first;
        Player* opponent = &second;
        Player* lastPlayer = nullptr;
        std::vector<Card> tableCards;
        CardTypeResult lastPlay;
        lastPlay.type = CardType::Invalid;

        int safety = 0;
        while (true) {
            safety++;
            if (safety > 500) break;

            while (true) {
                int tableScore = calculateScore(tableCards);
                std::vector<Card> play;
                if (current == &first) play = engineFirst.choosePlay(*current, *opponent, lastPlay, deck, tableScore);
                else play = engineSecond.choosePlay(*current, *opponent, lastPlay, deck, tableScore);

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

                if (current->hand.empty()) {
                    if (deck.cards.empty()) {
                        settleScoreCards(*current, tableCards);
                        settleScoreCards(*current, opponent->hand);
                        opponent->hand.clear();
                        tableCards.clear();
                        break;
                    }
                }

                if (current->hand.empty()) break;

                std::swap(current, opponent);
            }

            lastPlay.type = CardType::Invalid;
            lastPlay.cards.clear();
            lastPlay.keyPoint.clear();

            if (!deck.cards.empty()) {
                Player* loser = (lastPlayer == &first) ? &second : &first;
                refillToFive(*lastPlayer, deck);
                refillToFive(*loser, deck);
            }

            if (first.hand.empty() || second.hand.empty()) break;
        }

        if (first.totalScore > second.totalScore) { firstWins++; }
        else if (second.totalScore > first.totalScore) { secondWins++; }
        else { draws++; }

        firstScoreSum += first.totalScore;
        secondScoreSum += second.totalScore;

        if ((g + 1) % 20 == 0 || (g + 1) == 1 || (g + 1) == totalGames) {
            std::cout << "[" << (g + 1) << "/" << totalGames
                      << "] W" << levelFirst << ":" << firstWins
                      << " (" << (firstWins * 100.0 / (g + 1)) << "%)"
                      << " W" << levelSecond << ":" << secondWins
                      << " D:" << draws << std::endl;
        }
    }

    std::cout << "\n===== Result =====" << std::endl;
    std::cout << "W" << levelFirst << ": " << firstWins
              << " (" << (firstWins * 100.0 / totalGames) << "%)" << std::endl;
    std::cout << "W" << levelSecond << ": " << secondWins
              << " (" << (secondWins * 100.0 / totalGames) << "%)" << std::endl;
    std::cout << "Draws: " << draws << std::endl;

    return 0;
}