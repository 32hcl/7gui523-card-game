#include <iostream>
#include <vector>
#include <random>
#include <algorithm>
#include <fstream>
#include <cmath>
#include <numeric>
#include <iomanip>

#include "ai/ai.h"
#include "ai/ai_types.h"
#include "core/card/deck.h"
#include "core/player.h"
#include "core/card/cardtype.h"
#include "core/tracker/cardtracker.h"
#include "core/rule/score.h"
#include "core/rule/special.h"

struct GameResult {
    int winner;
    int scoreFirst;
    int scoreSecond;
    bool specialWin;
};

static GameResult runOneGame(AILevel levelFirst, AILevel levelSecond, std::mt19937& rng) {
    Deck deck = createStandardDeck();
    std::shuffle(deck.cards.begin(), deck.cards.end(), rng);

    Player first = createPlayer("AI4");
    first.aiLevel = levelFirst;
    Player second = createPlayer("Opp");
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

// 适应度：一组参数跑 N 局，算胜率
static double fitness(const AIParams& params, int games, std::mt19937& rng) {
    setAI4Params(params);
    int wins = 0;
    for (int i = 0; i < games; ++i) {
        GameResult r = runOneGame(AILevel::AI4_Expert, AILevel::AI3_Tracker, rng);
        if (r.winner == 1) wins++;
    }
    return (double)wins / games;
}

static void run_round(std::vector<std::vector<int>>& population,
                       int pop_size, int games_eval,
                       const std::vector<AIParams>& elite,
                       std::mt19937& rng) {
    int N = AIParams::paramCount();
    // 精英注入
    for (size_t i = 0; i < elite.size() && (int)population.size() < pop_size; ++i) {
        population.push_back(elite[i].toVector());
    }
    // 补齐种群
    AIParams base;
    while ((int)population.size() < pop_size) {
        std::vector<int> v = base.toVector();
        for (int j = 0; j < N; ++j) {
            int delta = std::uniform_int_distribution<>(-std::max(1, v[j] / 2), std::max(1, v[j] / 2))(rng);
            v[j] = std::max(1, v[j] + delta);
        }
        population.push_back(v);
    }
    // 进化 pop_size 代
    for (int gen = 0; gen < pop_size; ++gen) {
        std::vector<double> fit(pop_size);
        for (int i = 0; i < pop_size; ++i) {
            AIParams p = AIParams::fromVector(population[i]);
            fit[i] = fitness(p, games_eval, rng);
        }
        std::vector<int> order(pop_size);
        std::iota(order.begin(), order.end(), 0);
        std::sort(order.begin(), order.end(),
                  [&](int a, int b) { return fit[a] > fit[b]; });
        double avg = 0;
        for (double f : fit) avg += f;
        avg /= pop_size;
        std::cout << "  gen " << gen
                  << "  best=" << std::fixed << std::setprecision(3) << fit[order[0]]
                  << "  avg=" << std::fixed << std::setprecision(3) << avg
                  << std::endl;
        std::vector<std::vector<int>> next;
        for (int i = 0; i < 4; ++i) next.push_back(population[order[i]]);
        while ((int)next.size() < pop_size) {
            int pa = order[std::uniform_int_distribution<>(0, 5)(rng)];
            int pb = order[std::uniform_int_distribution<>(0, 5)(rng)];
            std::vector<int> child(N);
            for (int j = 0; j < N; ++j) {
                child[j] = (rng() % 2) ? population[pa][j] : population[pb][j];
                if (rng() % 100 < 12) {
                    int delta = std::uniform_int_distribution<>(-std::max(1, child[j] / 3), std::max(1, child[j] / 3))(rng);
                    child[j] = std::max(1, child[j] + delta);
                }
            }
            next.push_back(child);
        }
        population = next;
    }
}

int main() {
    const int POP_SIZE = 16;
    const int GAMES_PER_EVAL = 100;
    const double TARGET_WIN_RATE = 0.60;
    const int VERIFY_GAMES = 500;

    // 读取已有参数（如果 best_params.txt 存在）
    std::vector<AIParams> elite;
    {
        std::ifstream inf("best_params.txt");
        if (inf) {
            std::vector<int> v;
            int val;
            while (inf >> val) v.push_back(val);
            if ((int)v.size() >= AIParams::paramCount()) {
                elite.push_back(AIParams::fromVector(v));
                std::cout << "读取已有参数，作为精英种子\n";
            }
        }
    }

    std::random_device rd;
    std::mt19937 rng(rd());

    double bestRate = 0;
    int roundNum = 0;
    std::vector<int> bestVec;

    while (bestRate < TARGET_WIN_RATE) {
        roundNum++;
        std::cout << "\n========== Round " << roundNum << " ==========\n";

        std::vector<std::vector<int>> population;
        run_round(population, POP_SIZE, GAMES_PER_EVAL, elite, rng);

        // 验证最优参数
        AIParams candidate = AIParams::fromVector(population[0]);
        bestVec = population[0];
        setAI4Params(candidate);
        int verifyWins = 0;
        for (int i = 0; i < VERIFY_GAMES; ++i) {
            GameResult r = runOneGame(AILevel::AI4_Expert, AILevel::AI3_Tracker, rng);
            if (r.winner == 1) verifyWins++;
        }
        bestRate = (double)verifyWins / VERIFY_GAMES;

        // 平均分差
        long long scoreSum = 0;
        for (int i = 0; i < VERIFY_GAMES; ++i) {
            GameResult r = runOneGame(AILevel::AI4_Expert, AILevel::AI3_Tracker, rng);
            scoreSum += r.scoreFirst - r.scoreSecond;
        }
        double avgScoreDiff = (double)scoreSum / VERIFY_GAMES;

        std::cout << ">> 验证 " << VERIFY_GAMES << " 局: "
                  << "胜率=" << std::fixed << std::setprecision(3) << bestRate
                  << "  均分差=" << std::fixed << std::setprecision(1) << avgScoreDiff
                  << std::endl;

        // 保存当前最优
        std::vector<int> v = candidate.toVector();
        std::ofstream out("best_params.txt");
        for (size_t i = 0; i < v.size(); ++i) {
            out << v[i];
            if (i + 1 < v.size()) out << " ";
        }
        out.close();

        // 更新精英池
        elite.clear();
        elite.push_back(candidate);
        // 也加入前几名
        for (int i = 1; i < 3 && i < POP_SIZE; ++i) {
            elite.push_back(AIParams::fromVector(population[i]));
        }
    }

    std::cout << "\n====================================\n";
    std::cout << "达成目标！AI4 vs AI3 胜率 >= " << TARGET_WIN_RATE << "\n";

    AIParams best = AIParams::fromVector(bestVec);
    std::vector<int> v = best.toVector();
    std::cout << "最优参数向量: ";
    for (size_t i = 0; i < v.size(); ++i) {
        std::cout << v[i];
        if (i + 1 < v.size()) std::cout << " ";
    }
    std::cout << "\n参数已写入 best_params.txt\n";

    // 更新 ai_params.h 默认值
    std::cout << "\n请将以下默认值更新到 ai_params.h:\n";
    std::cout << "    int cardCountWeight = " << v[0] << ";\n";
    std::cout << "    int finishBonus = " << v[1] << ";\n";
    std::cout << "    int tableScoreWeight = " << v[2] << ";\n";
    std::cout << "    int earlyBigPenalty = " << v[3] << ";\n";
    std::cout << "    int earlyMidPenalty = " << v[4] << ";\n";
    std::cout << "    int midScorePenalty = " << v[5] << ";\n";
    std::cout << "    int stealThreshold = " << v[6] << ";\n";
    std::cout << "    int stealMultiplier = " << v[7] << ";\n";
    std::cout << "    int noConfidencePenalty = " << v[8] << ";\n";
    std::cout << "    int deckTopBonus = " << v[9] << ";\n";
    std::cout << "    int deckTopSpecialPenalty = " << v[10] << ";\n";
    std::cout << "    int splitPairPenalty = " << v[11] << ";\n";
    std::cout << "    int specialKeepBonus = " << v[12] << ";\n";
    std::cout << "    int specialMinRemaining = " << v[13] << ";\n";
    std::cout << "    int bombKeepPenalty = " << v[14] << ";\n";
    std::cout << "    int rocketKeepPenalty = " << v[15] << ";\n";
    std::cout << "    int endgameBombBonus = " << v[16] << ";\n";
    std::cout << "    int endgameRocketBonus = " << v[17] << ";\n";
    return 0;
}