#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <random>

#include "core/deck.h"
#include "core/player.h"
#include "core/cardtype.h"
#include "core/ai.h"
#include "core/score.h"
#include "core/special.h"
#include "core/cardtracker.h"

int main() {
    std::random_device rd;
    std::mt19937 rng(rd());

    Deck deck = createStandardDeck();
    std::shuffle(deck.cards.begin(), deck.cards.end(), rng);

    Player first = createPlayer("first");
    first.aiLevel = AILevel::AI1_Simple;
    Player second = createPlayer("second");
    second.aiLevel = AILevel::AI2_Rule;

    dealCards(first, deck, 5);
    dealCards(second, deck, 5);

    std::cout << "first init: ";
    for (auto& c : first.hand) std::cout << c.suit << c.point << " ";
    std::cout << "\nsecond init: ";
    for (auto& c : second.hand) std::cout << c.suit << c.point << " ";
    std::cout << "\ndeck: " << deck.cards.size() << std::endl;

    CardTracker tracker;
    Player* current = &first;
    Player* opponent = &second;
    std::vector<Card> tableCards;
    CardTypeResult lastPlay;
    lastPlay.type = CardType::Invalid;
    lastPlay.cards.clear();
    lastPlay.keyPoint.clear();
    int roundCount = 0;

    while (true) {
        roundCount++;
        if (roundCount > 50) { std::cout << "HIT 50 ROUND CAP!\n"; break; }

        Player* roundWinner = nullptr;

        while (true) {
            int tableScore = calculateScore(tableCards);
            std::cout << "R" << roundCount << " " << current->name
                      << " hand=" << current->hand.size()
                      << " opp=" << opponent->hand.size()
                      << " tableScore=" << tableScore
                      << " deck=" << deck.cards.size()
                      << " lastPlayType=" << (int)lastPlay.type
                      << " lastPlayCards=" << lastPlay.cards.size() << std::endl;

            if (lastPlay.type == CardType::Invalid && current->hand.empty()) {
                std::cout << "  -> leader no cards, break inner\n";
                break;
            }

            DecisionBreakdown bd;
            std::vector<Card> play = aiChoosePlayWithBreakdown(
                *current, *opponent, lastPlay, deck, tableScore, tracker, &bd);

            if (play.empty()) {
                std::cout << "  -> PASS (lastPlay="
                          << (lastPlay.type == CardType::Invalid ? "Invalid" : "valid")
                          << ")\n";
                if (lastPlay.type == CardType::Invalid) break;
                break;
            }

            std::cout << "  -> PLAY: ";
            for (auto& c : play) std::cout << c.suit << c.point << " ";
            std::cout << " val=" << bd.total << std::endl;

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

            if (current->hand.empty()) {
                Player* responder = opponent;
                Player* finisher = current;
                int respTableScore = calculateScore(tableCards);
                std::cout << "  -> finisher(" << finisher->name
                          << ") hand empty! responder=" << responder->name
                          << " tries...\n";
                DecisionBreakdown respBd;
                std::vector<Card> response = aiChoosePlayWithBreakdown(
                    *responder, *finisher, lastPlay, deck, respTableScore, tracker, &respBd);

                if (!response.empty()) {
                    CardTypeResult respParsed = parseCardType(response);
                    bool beats = lastPlay.type == CardType::Invalid
                                 || canBeat(respParsed, lastPlay);
                    if (respParsed.type != CardType::Invalid && beats) {
                        std::cout << "  -> responder PLAYS: ";
                        for (auto& c : response) std::cout << c.suit << c.point << " ";
                        std::cout << std::endl;

                        for (const Card& c : response) {
                            auto it = std::find_if(responder->hand.begin(), responder->hand.end(),
                                [&](const Card& h){ return h.point == c.point && h.suit == c.suit; });
                            if (it != responder->hand.end()) responder->hand.erase(it);
                        }
                        for (const Card& c : response) tableCards.push_back(c);
                        tracker.recordPlayed(response);
                        settleScoreCards(*responder, tableCards);
                        tableCards.clear();
                        roundWinner = responder;
                        break;
                    }
                }
                std::cout << "  -> responder passes, finisher gets table\n";
                settleScoreCards(*finisher, tableCards);
                tableCards.clear();
                roundWinner = finisher;
                break;
            }

            std::swap(current, opponent);
        }

        std::cout << "  [after inner] tableCards=" << tableCards.size()
                  << " roundWinner=" << (roundWinner ? roundWinner->name : "null")
                  << " opponent=" << opponent->name
                  << " first.hand=" << first.hand.size()
                  << " second.hand=" << second.hand.size() << std::endl;

        Player* winner = roundWinner ? roundWinner : opponent;
        settleScoreCards(*winner, tableCards);
        tableCards.clear();
        lastPlay.type = CardType::Invalid;
        lastPlay.cards.clear();
        lastPlay.keyPoint.clear();

        if (!deck.cards.empty()) {
            refillToFive(*winner, deck);
            refillToFive(*(winner == &first ? &second : &first), deck);
        }

        std::cout << "  [after refill] deck=" << deck.cards.size()
                  << " first.hand=" << first.hand.size()
                  << " second.hand=" << second.hand.size()
                  << " score=" << first.totalScore << ":" << second.totalScore
                  << std::endl;

        if (deck.cards.empty() && first.hand.empty() && second.hand.empty()) {
            std::cout << "  GAME END naturally\n";
            break;
        }

        current = winner;
        opponent = (current == &first) ? &second : &first;
    }

    std::cout << "Final: rounds=" << roundCount
              << " score=" << first.totalScore << ":" << second.totalScore << std::endl;
    return 0;
}