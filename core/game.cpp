#include "game.h"
#include "score.h"
#include "ai.h"
#include "special.h"
#include <random>
#include <sstream>

RoundResult playRound(Player& first, Player& second, Deck& deck,
                      CardTracker* tracker) {
    RoundResult result;
    result.winnerName = "";
    result.finishedPlayerName = "";
    result.specialVictory = false;
    result.handEmptied = false;
    result.tableCards.clear();

    Player* current = &first;
    Player* opponent = &second;
    Player* lastPlayer = nullptr;
    CardTypeResult lastPlay;
    std::vector<Card> tableCards;
    int tableBonus = 0;

    while (true) {
        int tableScore = calculateScore(tableCards);

        std::vector<Card> play;
        if (current->isHuman) {
            play = humanChoosePlay(*current, lastPlay);
        } else {
            CardTracker fallback;
            const CardTracker& tk = (tracker != nullptr) ? *tracker : fallback;
            play = aiChoosePlay(*current, *opponent, lastPlay, deck, tableScore, tk);
        }

        if (play.empty()) {
            if (lastPlay.cards.empty()) {
                // 先手无牌可出——防御兜底
                std::cout << current->name << " 无牌可出！" << std::endl;
                if (current->hand.empty()) {
                    result.winnerName = current->name;
                    result.handEmptied = true;
                    return result;
                }
                play = {current->hand[0]};
                std::cout << current->name << " 自动出牌: ";
                printCard(play[0]);
                std::cout << std::endl;
            } else {
                // 正常"不要"结束本回合
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

        CardTypeResult parsed = parseCardType(play);
        if (parsed.type == CardType::Invalid) {
            std::cout << current->name << " 非法牌型！" << std::endl;
            std::swap(current, opponent);
            continue;
        }
        if (!lastPlay.cards.empty() && !canBeat(parsed, lastPlay)) {
            std::cout << current->name << " 无法压过！" << std::endl;
            std::swap(current, opponent);
            continue;
        }

        // 特殊胜利检测（移牌前）
        if (parsed.type == CardType::Special523) {
            std::cout << current->name << " 打出 Special523，直接获胜！" << std::endl;
            settleScoreCards(*current, tableCards);
            tableCards.clear();
            result.winnerName = current->name;
            result.finishedPlayerName = current->name;
            result.specialVictory = true;
            result.handEmptied = true;
            return result;
        }

        // 压分奖励
        int bonus = calculatePressureBonus(parsed, lastPlay);
        parsed.bonusScore = bonus;
        tableBonus += bonus;

        // 移牌
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

        // 出完检查
        if (current->hand.empty()) {
            std::cout << current->name << " 出完手牌！" << std::endl;
            result.winnerName = current->name;
            result.handEmptied = true;
            result.tableCards = tableCards;
            return result;
        }

        std::swap(current, opponent);
    }
    return result;
}

void finalSettlement(Player& finisher, Player& opponent, std::vector<Card>& tableCards) {
    std::cout << std::endl;
    std::cout << "--- 终局结算开始 ---" << std::endl;
    std::cout << "出完牌者: " << finisher.name << std::endl;

    int tableScore = calculateScore(tableCards);
    if (!tableCards.empty()) {
        std::cout << "桌面牌 (共 " << tableCards.size() << " 张): ";
        for (size_t i = 0; i < tableCards.size(); ++i) {
            printCard(tableCards[i]);
            std::cout << "(" << tableCards[i].score << "分)";
            if (i < tableCards.size() - 1) std::cout << " ";
        }
        std::cout << std::endl;
    }
    std::cout << "桌面分值: " << tableScore << " 分" << std::endl;
    if (tableScore > 0) std::cout << finisher.name << " 获得桌面分值 " << tableScore << " 分" << std::endl;
    settleScoreCards(finisher, tableCards);

    int opponentScore = calculateScore(opponent.hand);
    if (!opponent.hand.empty()) {
        std::cout << opponent.name << " 剩余手牌 (共 " << opponent.hand.size() << " 张): ";
        for (size_t i = 0; i < opponent.hand.size(); ++i) {
            printCard(opponent.hand[i]);
            std::cout << "(" << opponent.hand[i].score << "分)";
            if (i < opponent.hand.size() - 1) std::cout << " ";
        }
        std::cout << std::endl;
    }
    std::cout << opponent.name << " 手牌分值: " << opponentScore << " 分" << std::endl;
    if (opponentScore > 0) std::cout << finisher.name << " 获得 " << opponent.name << " 手牌分值 " << opponentScore << " 分" << std::endl;
    settleScoreCards(finisher, opponent.hand);

    int totalObtained = tableScore + opponentScore;
    std::cout << finisher.name << " 本次终局共获得 " << totalObtained << " 分";
    std::cout << " (桌面 " << tableScore << " 分 + " << opponent.name << " 手牌 " << opponentScore << " 分)" << std::endl;

    opponent.hand.clear();
    tableCards.clear();
    std::cout << "--- 终局结算结束 ---" << std::endl;
    std::cout << std::endl;
}

void compareAndAnnounce(const Player& a, const Player& b) {
    std::cout << "========== 游戏结束 ==========" << std::endl;
    std::cout << a.name << " 总分: " << a.totalScore << std::endl;
    std::cout << b.name << " 总分: " << b.totalScore << std::endl;
    if (a.totalScore > b.totalScore) std::cout << "最终胜者: " << a.name << std::endl;
    else if (b.totalScore > a.totalScore) std::cout << "最终胜者: " << b.name << std::endl;
    else std::cout << "最终结果: 平局！" << std::endl;
}

void printPlayerStatus(const Player& player) {
    std::cout << player.name << " 状态 —— ";
    std::cout << "手牌 " << player.hand.size() << " 张, ";
    std::cout << "总分 " << player.totalScore << " 分" << std::endl;
}

void runBasicDataTest() {
    std::cout << "========== 7鬼523斗地主变体（两人计分版）——基础数据层测试 ==========" << std::endl;
    std::cout << std::endl;

    Deck deck = createStandardDeck();
    std::cout << "【1】创建标准牌堆" << std::endl;
    std::cout << "牌堆总牌数: " << deck.cards.size() << "（应为 54）" << std::endl << std::endl;

    std::cout << "【2】洗牌" << std::endl;
    shuffleDeck(deck);
    std::cout << "洗牌完成。" << std::endl << std::endl;

    std::cout << "【3】创建玩家" << std::endl;
    Player playerA = createPlayer("玩家A");
    Player playerB = createPlayer("玩家B");
    std::cout << "已创建两名玩家" << std::endl << std::endl;

    std::cout << "【4】发牌" << std::endl;
    dealCards(playerA, deck, 5);
    dealCards(playerB, deck, 5);
    std::cout << std::endl;

    std::cout << "【5】打印手牌" << std::endl;
    printHand(playerA);
    printHand(playerB);
    std::cout << "牌堆剩余数量: " << deck.cards.size() << std::endl << std::endl;

    std::cout << "【6】移牌补牌" << std::endl;
    if (playerA.hand.size() >= 2) {
        Card removed1 = playerA.hand.back(); playerA.hand.pop_back();
        Card removed2 = playerA.hand.back(); playerA.hand.pop_back();
        std::cout << "移除的牌: "; printCard(removed1); std::cout << " 、 "; printCard(removed2); std::cout << std::endl;
    }
    refillToFive(playerA, deck);
    printHand(playerA);
    std::cout << "牌堆剩余数量: " << deck.cards.size() << " 张" << std::endl << std::endl;

    std::cout << "【7】结算分值卡" << std::endl;
    std::vector<Card> scoreCards;
    auto makeCard = [](std::string p, std::string s, int sc) {
        Card c; c.point = p; c.suit = s; c.score = sc; return c;
    };
    scoreCards.push_back(makeCard("5", "红桃", 5));
    scoreCards.push_back(makeCard("10", "黑桃", 10));
    scoreCards.push_back(makeCard("K", "梅花", 20));
    scoreCards.push_back(makeCard("A", "方块", 0));

    for (size_t i = 0; i < scoreCards.size(); ++i) {
        printCard(scoreCards[i]);
        std::cout << "(" << scoreCards[i].score << "分)";
        if (i < scoreCards.size() - 1) std::cout << " ";
    }
    std::cout << std::endl;
    std::cout << "该牌组总分值: " << calculateScore(scoreCards) << " 分" << std::endl;
    settleScoreCards(playerA, scoreCards);
    std::cout << "玩家A的总分: " << playerA.totalScore << " 分" << std::endl;
    std::cout << "收集分值卡 " << playerA.collected.size() << " 张" << std::endl << std::endl;

    std::cout << "===========================================================" << std::endl;
    std::cout << "基础数据层测试完成" << std::endl;
    std::cout << "===========================================================" << std::endl;
}

static void runParseTest(const std::string& desc, const std::vector<Card>& cards,
                         CardType expType, const std::string& expKey) {
    auto r = parseCardType(cards);
    std::cout << "测试：" << desc << std::endl;
    std::cout << "解析结果：" << cardTypeToString(r.type);
    if (!r.keyPoint.empty()) std::cout << "，关键点数：" << r.keyPoint;
    std::cout << " | 预期：" << cardTypeToString(expType);
    if (!expKey.empty()) std::cout << "，关键点数：" << expKey;
    std::cout << " | " << ((r.type == expType && r.keyPoint == expKey) ? "通过" : "不通过") << std::endl;
}

static void runCompareTest(const std::string& desc, const std::vector<Card>& cand,
                           const std::vector<Card>& prev, bool expected) {
    auto c = parseCardType(cand);
    auto p = parseCardType(prev);
    bool r = canBeat(c, p);
    std::cout << "测试：" << desc << " | 结果：" << (r ? "能压" : "不能压")
              << " | 预期：" << (expected ? "能压" : "不能压")
              << " | " << (r == expected ? "通过" : "不通过") << std::endl;
}

void runCardTypeTest() {
    std::cout << std::endl << "========== 牌型解析与比较测试 ==========" << std::endl << std::endl;

    auto mk = [](std::string p, std::string s, int sc) {
        Card c; c.point = p; c.suit = s; c.score = sc; return c;
    };

    std::cout << "--- 单张比较 ---" << std::endl;
    runCompareTest("单张7 vs 单张5", {mk("7","红桃",0)}, {mk("5","黑桃",5)}, true);
    runCompareTest("单张4 vs 单张6", {mk("4","方块",0)}, {mk("6","梅花",0)}, false);
    runCompareTest("单张大鬼 vs 单张小鬼", {mk("大鬼","",0)}, {mk("小鬼","",0)}, true);
    runCompareTest("单张7 vs 单张7", {mk("7","红桃",0)}, {mk("7","黑桃",0)}, true);
    runCompareTest("单张7 vs 单张5", {mk("7","红桃",0)}, {mk("5","黑桃",5)}, true);

    std::cout << "--- 对子比较 ---" << std::endl;
    runCompareTest("对子5 vs 对子2", {mk("5","红桃",5),mk("5","黑桃",5)}, {mk("2","梅花",0),mk("2","方块",0)}, true);
    runCompareTest("对子3 vs 对子A", {mk("3","红桃",0),mk("3","黑桃",0)}, {mk("A","梅花",0),mk("A","方块",0)}, true);
    runCompareTest("对子4 vs 对子6", {mk("4","红桃",0),mk("4","黑桃",0)}, {mk("6","梅花",0),mk("6","方块",0)}, false);
    runCompareTest("对子5 vs 对子5", {mk("5","红桃",5),mk("5","黑桃",5)}, {mk("5","梅花",5),mk("5","方块",5)}, true);

    std::cout << "--- 三张比较 ---" << std::endl;
    runCompareTest("三张7 vs 三张5", {mk("7","红桃",0),mk("7","黑桃",0),mk("7","梅花",0)},
                   {mk("5","方块",5),mk("5","红桃",5),mk("5","黑桃",5)}, true);
    runCompareTest("三张K vs 三张K", {mk("K","红桃",5),mk("K","黑桃",5),mk("K","梅花",5)},
                   {mk("K","方块",5),mk("K","红桃",5),mk("K","黑桃",5)}, true);

    std::cout << "--- 三带一比较 ---" << std::endl;
    runCompareTest("三张5+4 vs 三张2+7",
                   {mk("5","红桃",5),mk("5","黑桃",5),mk("5","梅花",5),mk("4","方块",0)},
                   {mk("2","红桃",0),mk("2","黑桃",0),mk("2","梅花",0),mk("7","方块",0)}, true);

    std::cout << "--- 炸弹比较 ---" << std::endl;
    runCompareTest("炸弹7 vs 炸弹5",
                   {mk("7","红桃",0),mk("7","黑桃",0),mk("7","梅花",0),mk("7","方块",0)},
                   {mk("5","红桃",5),mk("5","黑桃",5),mk("5","梅花",5),mk("5","方块",5)}, true);
    runCompareTest("炸弹5 vs 炸弹7",
                   {mk("5","红桃",5),mk("5","黑桃",5),mk("5","梅花",5),mk("5","方块",5)},
                   {mk("7","红桃",0),mk("7","黑桃",0),mk("7","梅花",0),mk("7","方块",0)}, false);

    std::cout << "--- 炸弹压非炸弹 ---" << std::endl;
    runCompareTest("炸弹4 vs 单张7",
                   {mk("4","红桃",0),mk("4","黑桃",0),mk("4","梅花",0),mk("4","方块",0)},
                   {mk("7","红桃",0)}, true);

    std::cout << "--- 非炸弹不能压炸弹 ---" << std::endl;
    runCompareTest("单张7 vs 炸弹4",
                   {mk("7","红桃",0)},
                   {mk("4","红桃",0),mk("4","黑桃",0),mk("4","梅花",0),mk("4","方块",0)}, false);

    std::cout << "--- 王炸最大 ---" << std::endl;
    runCompareTest("王炸 vs 炸弹7",
                   {mk("大鬼","",0),mk("小鬼","",0)},
                   {mk("7","红桃",0),mk("7","黑桃",0),mk("7","梅花",0),mk("7","方块",0)}, true);
    runCompareTest("王炸 vs 王炸",
                   {mk("大鬼","",0),mk("小鬼","",0)},
                   {mk("大鬼","",0),mk("小鬼","",0)}, false);

    std::cout << "--- 不同类型不能比较 ---" << std::endl;
    runCompareTest("对子7 vs 单张5",
                   {mk("7","红桃",0),mk("7","黑桃",0)},
                   {mk("5","梅花",5)}, false);

    std::cout << "--- 非法牌型 ---" << std::endl;
    runParseTest("3张不同点数", {mk("3","红桃",0),mk("5","黑桃",5),mk("7","梅花",0)}, CardType::Invalid, "");
    runParseTest("5张牌", {mk("3","红桃",0),mk("5","黑桃",5),mk("7","梅花",0),mk("9","方块",0),mk("J","红桃",0)}, CardType::Invalid, "");
    runParseTest("两张不同点数非鬼", {mk("3","红桃",0),mk("7","黑桃",0)}, CardType::Invalid, "");

    std::cout << "===========================================================" << std::endl;
    std::cout << "牌型解析与比较测试完成" << std::endl;
    std::cout << "===========================================================" << std::endl;
}

void runAIVsAI() {
    std::cout << std::endl << "========== AI vs AI 自动对战 ==========" << std::endl;
    std::cout << "========== 游戏开始 ==========" << std::endl;

    Deck deck = createStandardDeck();
    shuffleDeck(deck);
    Player playerA = createPlayer("玩家A");
    Player playerB = createPlayer("玩家B");
    dealCards(playerA, deck, 5);
    dealCards(playerB, deck, 5);

    std::cout << "玩家A 初始手牌: ";
    for (size_t i = 0; i < playerA.hand.size(); ++i) {
        printCard(playerA.hand[i]); if (i < playerA.hand.size()-1) std::cout << " ";
    }
    std::cout << std::endl << "玩家B 初始手牌: ";
    for (size_t i = 0; i < playerB.hand.size(); ++i) {
        printCard(playerB.hand[i]); if (i < playerB.hand.size()-1) std::cout << " ";
    }
    std::cout << std::endl;

    bool finalPhase = false;

    std::random_device rd; std::mt19937 g(rd());
    std::uniform_int_distribution<> dist(0, 1);
    bool aFirst = (dist(g) == 0);
    std::cout << "随机先手: " << (aFirst ? "玩家A" : "玩家B") << std::endl;
    std::cout << "牌堆剩余: " << deck.cards.size() << " 张" << std::endl << std::endl;

    Player* first = aFirst ? &playerA : &playerB;
    Player* second = aFirst ? &playerB : &playerA;
    int roundNum = 0;

    while (true) {
        roundNum++;
        std::cout << "---------- 第 " << roundNum << " 回合 ----------" << std::endl;
        RoundResult rr = playRound(*first, *second, deck);

        // 情况 C：特殊胜利 → 立即终局
        if (rr.specialVictory) {
            Player* winner = (rr.winnerName == playerA.name) ? &playerA : &playerB;
            Player* loser = (winner == &playerA) ? &playerB : &playerA;
            std::cout << std::endl << "特殊胜利: " << winner->name << std::endl;
            finalSettlement(*winner, *loser, rr.tableCards);
            compareAndAnnounce(playerA, playerB);
            return;
        }

        Player* winner = (rr.winnerName == playerA.name) ? &playerA : &playerB;
        Player* loser = (winner == &playerA) ? &playerB : &playerA;

        // 情况 B：出完手牌
        if (rr.handEmptied) {
            std::cout << std::endl << "出完牌者: " << winner->name << std::endl;
            if (deck.cards.empty() || finalPhase) {
                // 牌堆已空 → 游戏终局
                finalSettlement(*winner, *loser, rr.tableCards);
                compareAndAnnounce(playerA, playerB);
                return;
            } else {
                // 牌堆未空 → 补牌继续
                refillToFive(*winner, deck);
                refillToFive(*loser, deck);
                if (deck.cards.empty()) {
                    finalPhase = true;
                    std::cout << "牌堆已空，进入终局阶段。" << std::endl;
                }
                printPlayerStatus(playerA);
                printPlayerStatus(playerB);
                std::cout << "牌堆剩余: " << deck.cards.size() << " 张" << std::endl;
                first = winner;
                second = loser;
                std::cout << std::endl;
                continue;
            }
        }

        // 情况 A：正常"不要"结束回合
        std::cout << "本回合胜方: " << winner->name << std::endl;
        printPlayerStatus(playerA);
        printPlayerStatus(playerB);

        // 检查是否进入终局阶段
        if (deck.cards.empty()) {
            finalPhase = true;
        }

        if (finalPhase) {
            std::cout << "终局阶段，不补牌。" << std::endl;
            // 终局阶段：检查是否有玩家手牌已空
            if (winner->hand.empty()) {
                finalSettlement(*winner, *loser);
                compareAndAnnounce(playerA, playerB);
                return;
            }
            if (loser->hand.empty()) {
                finalSettlement(*loser, *winner);
                compareAndAnnounce(playerA, playerB);
                return;
            }
            printPlayerStatus(playerA);
            printPlayerStatus(playerB);
        } else {
            // 正常补牌
            int bA = (int)playerA.hand.size(), bB = (int)playerB.hand.size();
            refillToFive(*winner, deck);
            refillToFive(*loser, deck);
            int aA = (int)playerA.hand.size(), aB = (int)playerB.hand.size();
            if (aA > bA) std::cout << "玩家A 补牌 " << (aA-bA) << " 张 → 现在 " << aA << " 张" << std::endl;
            if (aB > bB) std::cout << "玩家B 补牌 " << (aB-bB) << " 张 → 现在 " << aB << " 张" << std::endl;
            std::cout << "牌堆剩余: " << deck.cards.size() << " 张" << std::endl;
            if (deck.cards.empty()) {
                finalPhase = true;
                std::cout << "牌堆已空，进入终局阶段。" << std::endl;
            }
        }

        first = winner;
        second = loser;
        std::cout << std::endl;
    }
}

void runHumanVsAI() {
    std::cout << std::endl << "========== 人类 vs AI 对局 ==========" << std::endl;
    std::cout << "========== 游戏开始 ==========" << std::endl;

    Deck deck = createStandardDeck();
    shuffleDeck(deck);
    Player playerA = createPlayer("玩家A");
    playerA.isHuman = true;
    Player playerB = createPlayer("玩家B");

    dealCards(playerA, deck, 5);
    dealCards(playerB, deck, 5);

    std::cout << "玩家A 初始手牌: ";
    for (size_t i = 0; i < playerA.hand.size(); ++i) {
        printCard(playerA.hand[i]); if (i < playerA.hand.size()-1) std::cout << " ";
    }
    std::cout << std::endl << "玩家B 初始手牌: ";
    for (size_t i = 0; i < playerB.hand.size(); ++i) {
        printCard(playerB.hand[i]); if (i < playerB.hand.size()-1) std::cout << " ";
    }
    std::cout << std::endl;

    bool finalPhase = false;

    std::random_device rd; std::mt19937 g(rd());
    std::uniform_int_distribution<> dist(0, 1);
    bool aFirst = (dist(g) == 0);
    std::cout << "随机先手: " << (aFirst ? "玩家A" : "玩家B") << std::endl;
    std::cout << "牌堆剩余: " << deck.cards.size() << " 张" << std::endl << std::endl;

    Player* first = aFirst ? &playerA : &playerB;
    Player* second = aFirst ? &playerB : &playerA;
    int roundNum = 0;

    while (true) {
        roundNum++;
        std::cout << "---------- 第 " << roundNum << " 回合 ----------" << std::endl;
        RoundResult rr = playRound(*first, *second, deck);

        // 情况 C：特殊胜利 → 立即终局
        if (rr.specialVictory) {
            Player* winner = (rr.winnerName == playerA.name) ? &playerA : &playerB;
            Player* loser = (winner == &playerA) ? &playerB : &playerA;
            std::cout << std::endl << "特殊胜利: " << winner->name << std::endl;
            finalSettlement(*winner, *loser, rr.tableCards);
            compareAndAnnounce(playerA, playerB);
            return;
        }

        Player* winner = (rr.winnerName == playerA.name) ? &playerA : &playerB;
        Player* loser = (winner == &playerA) ? &playerB : &playerA;

        // 情况 B：出完手牌
        if (rr.handEmptied) {
            std::cout << std::endl << "出完牌者: " << winner->name << std::endl;
            if (deck.cards.empty() || finalPhase) {
                // 牌堆已空 → 游戏终局
                finalSettlement(*winner, *loser, rr.tableCards);
                compareAndAnnounce(playerA, playerB);
                return;
            } else {
                // 牌堆未空 → 补牌继续
                refillToFive(*winner, deck);
                refillToFive(*loser, deck);
                if (deck.cards.empty()) {
                    finalPhase = true;
                    std::cout << "牌堆已空，进入终局阶段。" << std::endl;
                }
                printPlayerStatus(playerA);
                printPlayerStatus(playerB);
                std::cout << "牌堆剩余: " << deck.cards.size() << " 张" << std::endl;
                first = winner;
                second = loser;
                std::cout << std::endl;
                continue;
            }
        }

        // 情况 A：正常"不要"结束回合
        std::cout << "本回合胜方: " << winner->name << std::endl;
        printPlayerStatus(playerA);
        printPlayerStatus(playerB);

        // 检查是否进入终局阶段
        if (deck.cards.empty()) {
            finalPhase = true;
        }

        if (finalPhase) {
            std::cout << "终局阶段，不补牌。" << std::endl;
            if (winner->hand.empty()) {
                finalSettlement(*winner, *loser);
                compareAndAnnounce(playerA, playerB);
                return;
            }
            if (loser->hand.empty()) {
                finalSettlement(*loser, *winner);
                compareAndAnnounce(playerA, playerB);
                return;
            }
            printPlayerStatus(playerA);
            printPlayerStatus(playerB);
        } else {
            int bA = (int)playerA.hand.size(), bB = (int)playerB.hand.size();
            refillToFive(*winner, deck);
            refillToFive(*loser, deck);
            int aA = (int)playerA.hand.size(), aB = (int)playerB.hand.size();
            if (aA > bA) std::cout << "玩家A 补牌 " << (aA-bA) << " 张 → 现在 " << aA << " 张" << std::endl;
            if (aB > bB) std::cout << "玩家B 补牌 " << (aB-bB) << " 张 → 现在 " << aB << " 张" << std::endl;
            std::cout << "牌堆剩余: " << deck.cards.size() << " 张" << std::endl;
            if (deck.cards.empty()) {
                finalPhase = true;
                std::cout << "牌堆已空，进入终局阶段。" << std::endl;
            }
        }

        first = winner;
        second = loser;
        std::cout << std::endl;
    }
}