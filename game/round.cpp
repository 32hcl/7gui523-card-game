#include "game/round.h"
#include "core/rule/score.h"
#include "core/card/cardtype.h"
#include "ai/ai.h"
#include "ai/plugins/cardtracker.h"
#include "core/rule/special.h"
#include "game/event.h"
#include <iostream>

RoundResult playRoundDualDeck(Player& first, Player& second,
                              Deck& firstDeck, Deck& secondDeck,
                              CardTracker* tracker) {
    RoundResult result;
    result.winnerName = "";
    result.finishedPlayerName = "";
    result.specialVictory = false;
    result.handEmptied = false;
    result.tableCards.clear();

    Player* current = &first;
    Player* opponent = &second;
    Deck* currentDeck = &firstDeck;
    Deck* opponentDeck = &secondDeck;
    Player* lastPlayer = nullptr;
    CardTypeResult lastPlay;
    std::vector<Card> tableCards;
    int tableBonus = 0;

    while (true) {
        int tableScore = calculateTableScore(tableCards, tableBonus);

        std::vector<Card> play;
        if (current->isHuman) {
            play = humanChoosePlay(*current, lastPlay);
        } else {
            CardTracker fallback;
            const CardTracker& tk = (tracker != nullptr) ? *tracker : fallback;
            // 维护搜索模型视角字段（否则 AI3/AI4 终局判断恒真，模型失真）
            currentDeck->myRemaining = static_cast<int>(currentDeck->cards.size());
            opponentDeck->oppRemaining = static_cast<int>(opponentDeck->cards.size());
            play = aiChoosePlay(*current, *opponent, lastPlay, *currentDeck, tableScore, tk);
        }

        if (play.empty()) {
            if (lastPlay.cards.empty()) {
                std::cout << current->name << " 无牌可出！" << std::endl;
                if (current->hand.empty()) {
                    if (currentDeck->cards.empty()) {
                        result.winnerName = current->name;
                        result.finishedPlayerName = current->name;
                        result.handEmptied = true;
                        result.tableCards = tableCards;
                        return result;
                    }
                    refillToFive(*current, *currentDeck);
                }
                play = {current->hand[0]};
                std::cout << current->name << " 自动出牌: ";
                printCard(play[0]);
                std::cout << std::endl;
            } else {
                std::cout << current->name << " 不要" << std::endl;
                settleScoreCards(*lastPlayer, tableCards);
                lastPlayer->totalScore += tableBonus;
                int ts = calculateTableScore(tableCards, tableBonus);
                if (ts > 0) std::cout << "桌面分值: " << ts << " 分" << std::endl;
                result.winnerName = lastPlayer->name;
                tableCards.clear();
                return result;
            }
        }

        CardTypeResult parsed = parseAnyCardType(play);
        if (parsed.type == CardType::Invalid) {
            std::cout << current->name << " 非法牌型！" << std::endl;
            std::swap(current, opponent);
            std::swap(currentDeck, opponentDeck);
            continue;
        }
        if (!lastPlay.cards.empty() && !canBeat(parsed, lastPlay)) {
            std::cout << current->name << " 无法压过！" << std::endl;
            std::swap(current, opponent);
            std::swap(currentDeck, opponentDeck);
            continue;
        }

        if (parsed.type == CardType::Special523) {
            std::cout << current->name << " 打出 Special523，直接获胜！" << std::endl;
            settleScoreCards(*current, tableCards);
            current->totalScore += tableBonus;
            tableCards.clear();
            result.winnerName = current->name;
            result.finishedPlayerName = current->name;
            result.specialVictory = true;
            result.handEmptied = true;
            return result;
        }

        int bonus = calculatePressureBonus(parsed, lastPlay);
        parsed.bonusScore = bonus;
        tableBonus += bonus;

        removeCardsFromHand(*current, play);
        tableCards.insert(tableCards.end(), play.begin(), play.end());
        if (tracker) tracker->recordPlayed(play);
        lastPlay = parsed;
        lastPlayer = current;

        std::cout << current->name << " 出牌: ";
        for (size_t i = 0; i < play.size(); ++i) {
            printCard(play[i]);
            if (i < play.size() - 1) std::cout << " ";
        }
        std::cout << " (" << cardTypeToString(parsed.type);
        if (!parsed.keyPoint.empty()) std::cout << " " << parsed.keyPoint;
        if (parsed.bonusScore > 0) std::cout << " 压分+" << parsed.bonusScore;
        std::cout << ")" << std::endl;

        if (current->hand.empty()) {
            std::cout << current->name << " 出完手牌！" << std::endl;
            if (currentDeck->cards.empty()) {
                result.winnerName = current->name;
                result.finishedPlayerName = current->name;
                result.handEmptied = true;
                result.tableCards = tableCards;
                return result;
            }
            refillToFive(*current, *currentDeck);
        }

        std::swap(current, opponent);
        std::swap(currentDeck, opponentDeck);
    }
    return result;
}