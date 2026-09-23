#include <iostream>
#include <vector>
#include <algorithm>
#include "core/card/deck.h"
#include "core/player.h"
#include "game/game.h"
#include "game/round.h"
#include "game/campaign.h"
#include "core/tracker/cardtracker.h"

int main() {
    const int total = 5;
    int clears = 0;
    int totalCleared = 0;
    int totalHp = 0;

    std::cout << "Campaign Simulation (" << total << " runs)" << std::endl;
    std::cout << "========================" << std::endl;

    for (int run = 0; run < total; ++run) {
        Campaign camp;
        int lvCleared = 0;

        std::cout << std::endl << "=== Run " << (run + 1) << " ===" << std::endl;

        for (int lv = 1; lv <= camp.config().totalLevels; ++lv) {
            camp.startLevel(lv);

            Player player = createPlayer("Player");
            Player boss = createPlayer("Boss");

            Deck stdDeck = createStandardDeck();
            shuffleDeck(stdDeck);

            auto playerCards = removeCards(stdDeck.cards, {});
            Deck playerDeck;
            playerDeck.cards = drawRandom(playerCards, 27);

            auto bossCards = removeCards(stdDeck.cards, getLevelRemoveTable(lv));
            Deck bossDeck;
            bossDeck.cards = drawRandom(bossCards, 27);

            dealCards(player, playerDeck, 5);
            dealCards(boss, bossDeck, 5);

            CardTracker tracker;

            int roundNum = 0;
            bool levelDone = false;
            while (!levelDone) {
                ++roundNum;
                RoundResult rr = playRoundDualDeck(player, boss, playerDeck, bossDeck, &tracker);

                bool oneSideEmpty = (player.hand.empty() && playerDeck.cards.empty())
                                 || (boss.hand.empty() && bossDeck.cards.empty());

                if (oneSideEmpty || camp.bossHp() <= 0) {
                    std::cout << "  Lv" << lv << " R" << roundNum
                              << " end: " << rr.winnerName
                              << " pScore=" << player.totalScore
                              << " bScore=" << boss.totalScore
                              << " pDeck=" << playerDeck.cards.size()
                              << " bDeck=" << bossDeck.cards.size()
                              << " pHand=" << player.hand.size()
                              << " bHand=" << boss.hand.size();

                    camp.finishLevel(rr, player, boss,
                        static_cast<int>(playerDeck.cards.size()),
                        static_cast<int>(bossDeck.cards.size()));

                    std::cout << " => pHP=" << camp.playerHp()
                              << " bHP=" << camp.bossHp() << std::endl;
                    levelDone = true;
                    break;
                }

                refillToFive(player, playerDeck);
                refillToFive(boss, bossDeck);
            }

            if (camp.playerHp() <= 0) {
                std::cout << "  GAME OVER at Lv" << lv << std::endl;
                break;
            }

            lvCleared++;

            if (lv == camp.config().totalLevels && camp.bossHp() <= 0) {
                clears++;
            }
        }

        totalCleared += lvCleared;
        totalHp += std::max(0, camp.playerHp());

        std::cout << "  Result: " << lvCleared << "/9, HP="
                  << std::max(0, camp.playerHp()) << std::endl;
    }

    std::cout << std::endl;
    std::cout << "Total clears (all 9): " << clears << "/" << total << std::endl;
    std::cout << "Avg levels cleared: " << (double)totalCleared / total << std::endl;
    std::cout << "Avg remaining HP: " << (double)totalHp / total << std::endl;

    return 0;
}