#include <iostream>
#include <vector>
#include <algorithm>
#include "core/card/deck.h"
#include "core/player.h"
#include "core/rule/score.h"
#include "core/card/cardtype.h"
#include "game/game.h"
#include "game/campaign.h"
#include "ai/engine/ai_engine.h"
#include "ai/engine/ai_levels.h"
#include "core/tracker/cardtracker.h"

static void runOneLevel(Campaign& camp, int lv, bool verbose) {
    camp.startLevel(lv);

    Player player = createPlayer("Player");
    Player boss = createPlayer("Boss");

    AIEngineConfig playerCfg = buildAIEngineConfig(std::min(lv, 9));
    AIEngineConfig bossCfg = buildAIEngineConfig(std::min(lv + 2, 15));
    AIEngine playerAI(playerCfg);
    AIEngine bossAI(bossCfg);

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

    Player* current = &player;
    Player* opponent = &boss;
    Deck* currentDeck = &playerDeck;
    Deck* opponentDeck = &bossDeck;
    AIEngine* currentAI = &playerAI;
    AIEngine* opponentAI = &bossAI;
    Player* lastPlayer = nullptr;
    CardTypeResult lastPlay;
    std::vector<Card> tableCards;
    int tableBonus = 0;
    int roundCount = 0;

    while (true) {
        if (current->hand.empty() && currentDeck->cards.empty()) break;
        if (opponent->hand.empty() && opponentDeck->cards.empty()) break;

        int tableScore = calculateTableScore(tableCards, tableBonus);

        std::vector<Card> play = currentAI->choosePlay(*current, *opponent, lastPlay, *currentDeck, tableScore);

        if (play.empty()) {
            if (lastPlay.cards.empty()) {
                if (current->hand.empty()) {
                    if (currentDeck->cards.empty()) break;
                    refillToFive(*current, *currentDeck);
                }
                play = {current->hand[0]};
            } else {
                if (verbose) std::cout << current->name << " 不要" << std::endl;
                settleScoreCards(*lastPlayer, tableCards);
                lastPlayer->totalScore += tableBonus;
                tableCards.clear();
                lastPlay.cards.clear();
                lastPlayer = nullptr;
                roundCount++;
                std::swap(current, opponent);
                std::swap(currentDeck, opponentDeck);
                std::swap(currentAI, opponentAI);
                continue;
            }
        }

        CardTypeResult parsed = parseCardType(play);
        if (parsed.type == CardType::Invalid) {
            std::swap(current, opponent);
            std::swap(currentDeck, opponentDeck);
            std::swap(currentAI, opponentAI);
            continue;
        }
        if (!lastPlay.cards.empty() && !canBeat(parsed, lastPlay)) {
            std::swap(current, opponent);
            std::swap(currentDeck, opponentDeck);
            std::swap(currentAI, opponentAI);
            continue;
        }

        if (parsed.type == CardType::Special523) {
            settleScoreCards(*current, tableCards);
            current->totalScore += tableBonus;
            tableCards.clear();
            lastPlay.cards.clear();
            lastPlayer = current;
            break;
        }

        int bonus = calculatePressureBonus(parsed, lastPlay);
        parsed.bonusScore = bonus;
        tableBonus += bonus;

        removeCardsFromHand(*current, play);
        tableCards.insert(tableCards.end(), play.begin(), play.end());
        tracker.recordPlayed(play);
        currentAI->recordPlayed(play);
        opponentAI->recordPlayed(play);
        lastPlay = parsed;
        lastPlayer = current;

        if (verbose) {
            std::cout << current->name << " 出牌: ";
            for (size_t i = 0; i < play.size(); ++i) {
                std::cout << play[i].suit << play[i].point;
                if (i + 1 < play.size()) std::cout << " ";
            }
            std::cout << std::endl;
        }

        if (current->hand.empty()) {
            if (currentDeck->cards.empty()) {
                break;
            }
            refillToFive(*current, *currentDeck);
        }

        std::swap(current, opponent);
        std::swap(currentDeck, opponentDeck);
        std::swap(currentAI, opponentAI);
    }

    RoundResult rr;
    rr.winnerName = (lastPlayer) ? lastPlayer->name : "";
    rr.handEmptied = (player.hand.empty() && playerDeck.cards.empty())
                  || (boss.hand.empty() && bossDeck.cards.empty());
    rr.specialVictory = false;
    rr.tableCards = tableCards;

    camp.finishLevel(rr, player, boss,
        static_cast<int>(playerDeck.cards.size()),
        static_cast<int>(bossDeck.cards.size()));

    if (verbose) {
        std::cout << "  Lv" << lv << " done: pScore=" << player.totalScore
                  << " bScore=" << boss.totalScore
                  << " => pHP=" << camp.playerHp()
                  << " bHP=" << camp.bossHp() << std::endl;
    }
}

int main() {
    const int total = 20;
    int clears = 0;
    int totalCleared = 0;
    int totalHp = 0;

    std::cout << "Campaign Simulation with AIEngine (" << total << " runs)" << std::endl;
    std::cout << "=================================================" << std::endl;

    for (int run = 0; run < total; ++run) {
        Campaign camp;
        int lvCleared = 0;
        bool verbose = (run == 0);

        if (verbose) std::cout << std::endl << "=== Run 1 (verbose) ===" << std::endl;

        for (int lv = 1; lv <= camp.config().totalLevels; ++lv) {
            runOneLevel(camp, lv, verbose);

            if (camp.playerHp() <= 0) break;

            lvCleared++;

            if (lv == camp.config().totalLevels && camp.bossHp() <= 0) {
                clears++;
            }
        }

        totalCleared += lvCleared;
        totalHp += std::max(0, camp.playerHp());

        if (!verbose) {
            std::cout << "Run " << (run + 1) << "/" << total
                      << " -> cleared " << lvCleared << "/9"
                      << " HP=" << std::max(0, camp.playerHp()) << std::endl;
        } else {
            std::cout << "Result: " << lvCleared << "/9, HP="
                      << std::max(0, camp.playerHp()) << std::endl;
        }
    }

    std::cout << std::endl;
    std::cout << "Total clears (all 9): " << clears << "/" << total << std::endl;
    std::cout << "Avg levels cleared: " << (double)totalCleared / total << std::endl;
    std::cout << "Avg remaining HP: " << (double)totalHp / total << std::endl;

    return 0;
}