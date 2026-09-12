#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <map>

#include "core/deck.h"
#include "core/player.h"
#include "core/cardtype.h"
#include "core/ai.h"
#include "core/score.h"
#include "core/special.h"
#include "core/cardtracker.h"

struct DecisionRecord {
    int round = 0;
    std::string player;
    int handSize = 0;
    int opponentHandSize = 0;
    int tableScore = 0;
    int deckRemaining = 0;
    std::string lastPlayType;
    std::vector<Card> chosenPlay;
    std::string playType;
    DecisionBreakdown breakdown;
};

struct GameRecord {
    int gameId = 0;
    std::string firstAI;
    std::string secondAI;
    std::vector<Card> firstHandInitial;
    std::vector<Card> secondHandInitial;
    int winner = 0;
    int firstScore = 0;
    int secondScore = 0;
    int rounds = 0;
    bool specialWin = false;
    std::vector<DecisionRecord> decisions;
};

static std::string levelName(AILevel lv) {
    switch (lv) {
        case AILevel::AI1_Simple:  return "AI1";
        case AILevel::AI2_Rule:    return "AI2";
        case AILevel::AI3_Tracker: return "AI3";
    }
    return "?";
}

static std::string cardsToShort(const std::vector<Card>& cards) {
    std::string s;
    for (size_t i = 0; i < cards.size(); ++i) {
        if (i > 0) s += " ";
        s += cards[i].suit + cards[i].point;
    }
    return s;
}

static std::string cardTypeToStringJP(CardType t) {
    switch (t) {
        case CardType::Invalid:       return "Invalid";
        case CardType::Single:        return "Single";
        case CardType::Pair:          return "Pair";
        case CardType::Triple:        return "Triple";
        case CardType::TripleWithOne: return "T3+1";
        case CardType::TripleWithTwo: return "T3+2";
        case CardType::Bomb:          return "Bomb";
        case CardType::Rocket:        return "Rocket";
    }
    return "?";
}

GameRecord runOneGameWithLog(AILevel levelFirst, AILevel levelSecond,
                             int gameId, std::mt19937& rng,
                             bool collectDecisions) {
    GameRecord rec;
    rec.gameId = gameId;
    rec.firstAI = levelName(levelFirst);
    rec.secondAI = levelName(levelSecond);

    Deck deck = createStandardDeck();
    std::shuffle(deck.cards.begin(), deck.cards.end(), rng);

    Player first = createPlayer("first");
    first.aiLevel = levelFirst;
    Player second = createPlayer("second");
    second.aiLevel = levelSecond;

    dealCards(first, deck, 5);
    dealCards(second, deck, 5);
    rec.firstHandInitial = first.hand;
    rec.secondHandInitial = second.hand;

    CardTracker tracker;

    if (checkSpecialVictory(first))  { rec.winner = 1; rec.specialWin = true; return rec; }
    if (checkSpecialVictory(second)) { rec.winner = 2; rec.specialWin = true; return rec; }

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
        if (roundCount > 300) break;

        Player* roundWinner = nullptr;

        while (true) {
            int opponentHandSize = (int)opponent->hand.size();
            int tableScore = calculateScore(tableCards);

            if (lastPlay.type == CardType::Invalid && current->hand.empty()) {
                break;
            }

            DecisionBreakdown bd;
            std::vector<Card> play = aiChoosePlayWithBreakdown(
                *current, *opponent, lastPlay, deck, tableScore, tracker,
                collectDecisions ? &bd : nullptr);

            if (play.empty()) {
                if (lastPlay.type == CardType::Invalid) break;
                break;
            }

            CardTypeResult parsed = parseCardType(play);
            if (parsed.type == CardType::Invalid) break;
            if (lastPlay.type != CardType::Invalid && !canBeat(parsed, lastPlay)) break;

            if (collectDecisions) {
                DecisionRecord dr;
                dr.round = roundCount;
                dr.player = (current == &first) ? "first" : "second";
                dr.handSize = (int)current->hand.size();
                dr.opponentHandSize = opponentHandSize;
                dr.tableScore = tableScore;
                dr.deckRemaining = (int)deck.cards.size();
                dr.lastPlayType = (lastPlay.type == CardType::Invalid)
                                  ? "-" : cardTypeToStringJP(lastPlay.type);
                dr.chosenPlay = play;
                dr.playType = cardTypeToStringJP(parsed.type);
                dr.breakdown = bd;
                rec.decisions.push_back(dr);
            }

            for (const Card& c : play) {
                auto it = std::find_if(current->hand.begin(), current->hand.end(),
                    [&](const Card& h){ return h.point == c.point && h.suit == c.suit; });
                if (it != current->hand.end()) current->hand.erase(it);
            }
            for (const Card& c : play) tableCards.push_back(c);
            tracker.recordPlayed(play);
            lastPlay = parsed;

            if (checkSpecialVictory(*current)) {
                rec.winner = (current == &first) ? 1 : 2;
                rec.specialWin = true;
                return rec;
            }

            if (current->hand.empty()) {
                Player* responder  = opponent;
                Player* finisher   = current;
                int respHandSize   = (int)finisher->hand.size();
                int respTableScore = calculateScore(tableCards);
                DecisionBreakdown respBd;
                std::vector<Card> response = aiChoosePlayWithBreakdown(
                    *responder, *finisher, lastPlay, deck, respTableScore, tracker,
                    collectDecisions ? &respBd : nullptr);

                if (!response.empty()) {
                    CardTypeResult respParsed = parseCardType(response);
                    bool valid  = respParsed.type != CardType::Invalid;
                    bool beats  = lastPlay.type == CardType::Invalid
                                  || canBeat(respParsed, lastPlay);
                    if (valid && beats) {
                        if (collectDecisions) {
                            DecisionRecord dr;
                            dr.round = roundCount;
                            dr.player = (responder == &first) ? "first" : "second";
                            dr.handSize = (int)responder->hand.size();
                            dr.opponentHandSize = 0;
                            dr.tableScore = respTableScore;
                            dr.deckRemaining = (int)deck.cards.size();
                            dr.lastPlayType = cardTypeToStringJP(lastPlay.type);
                            dr.chosenPlay = response;
                            dr.playType = cardTypeToStringJP(respParsed.type);
                            dr.breakdown = respBd;
                            rec.decisions.push_back(dr);
                        }
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
                settleScoreCards(*finisher, tableCards);
                tableCards.clear();
                roundWinner = finisher;
                break;
            }

            std::swap(current, opponent);
        }

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

        if (checkSpecialVictory(first)) {
            rec.winner = 1; rec.specialWin = true; return rec;
        }
        if (checkSpecialVictory(second)) {
            rec.winner = 2; rec.specialWin = true; return rec;
        }

        if (deck.cards.empty() && first.hand.empty() && second.hand.empty()) break;

        current = winner;
        opponent = (current == &first) ? &second : &first;
    }

    rec.firstScore = first.totalScore;
    rec.secondScore = second.totalScore;
    if (first.totalScore > second.totalScore) rec.winner = 1;
    else if (second.totalScore > first.totalScore) rec.winner = 2;
    else rec.winner = 0;
    rec.rounds = roundCount;
    return rec;
}

int main(int argc, char* argv[]) {
    int gamesPerCombo = 1000;
    if (argc > 1) gamesPerCombo = std::atoi(argv[1]);

    std::random_device rd;
    std::mt19937 rng(rd());

    std::vector<AILevel> levels = {
        AILevel::AI1_Simple,
        AILevel::AI2_Rule,
        AILevel::AI3_Tracker
    };

    std::ofstream csv("battle_log.csv");
    csv << "game_id,first_ai,second_ai,winner,first_score,second_score,"
        << "rounds,special_win\n";

    std::ofstream detail("battle_detail.txt");

    int gameId = 0;
    int totalGames = 0;

    for (AILevel lf : levels) {
        for (AILevel ls : levels) {
            for (int i = 0; i < gamesPerCombo; ++i) {
                gameId++;
                totalGames++;
                bool collect = (lf != AILevel::AI1_Simple
                                || ls != AILevel::AI1_Simple);
                GameRecord rec = runOneGameWithLog(lf, ls, gameId, rng, collect);

                csv << rec.gameId << ","
                    << rec.firstAI << ","
                    << rec.secondAI << ","
                    << rec.winner << ","
                    << rec.firstScore << ","
                    << rec.secondScore << ","
                    << rec.rounds << ","
                    << (rec.specialWin ? 1 : 0) << "\n";

                if (collect) {
                    detail << "=== Game " << gameId
                           << "  " << rec.firstAI << " vs " << rec.secondAI
                           << " ===\n";
                    detail << "first initial: " << cardsToShort(rec.firstHandInitial) << "\n";
                    detail << "second initial: " << cardsToShort(rec.secondHandInitial) << "\n";
                    detail << "result: "
                           << (rec.winner == 1 ? "first win"
                               : rec.winner == 2 ? "second win" : "draw")
                           << "  " << rec.firstScore << ":" << rec.secondScore
                           << "  rounds=" << rec.rounds
                           << (rec.specialWin ? "  [special]" : "") << "\n";
                    for (const auto& d : rec.decisions) {
                        detail << "  R" << d.round << " " << d.player
                               << " hand" << d.handSize
                               << " opp" << d.opponentHandSize
                               << " T" << d.tableScore
                               << " D" << d.deckRemaining
                               << " prev[" << d.lastPlayType << "]"
                               << " play[" << cardsToShort(d.chosenPlay) << "]"
                               << " (" << d.playType << ")"
                               << " val=" << d.breakdown.total
                               << " {B" << d.breakdown.base
                               << ",S" << d.breakdown.scoreCard
                               << ",Sp" << d.breakdown.special
                               << ",E" << d.breakdown.endgame
                               << ",C" << d.breakdown.comboBreak
                               << ",T" << d.breakdown.tableScore
                               << ",Df" << d.breakdown.defensive
                               << ",Tr" << d.breakdown.tracker << "}\n";
                    }
                    detail << "\n";
                }

                if (totalGames % 1000 == 0) {
                    std::cout << "completed " << totalGames << " games\n";
                }
            }
        }
    }

    std::cout << "all done: " << totalGames << " games\n";
    std::cout << "output: battle_log.csv, battle_detail.txt\n";
    return 0;
}