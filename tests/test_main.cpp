#include <iostream>
#include <cassert>

void test_parseCardType();
void test_canBeat();
void test_calculatorScore();
void test_tableScore();
void test_pressureBonus();
void test_checkSpecialVictory();
void test_createStandardDeck();
void test_drawCards();
void test_refillToFive();
void test_removeCards();
void test_drawRandom();
void test_splitDeck();
void test_removeCardsFromHand();
void test_campaignInit();
void test_campaignStartLevel();
void test_removeTable();
void test_campaignSettlement();
void test_campaignSettlementBossWins();
void test_campaignHpCap();
void test_campaignGameOver();
void test_sampledWorldConservation();
void test_searchFinalSettlement();
void test_searchTerminalUtility();
void test_trackerDualSide();
void test_variantRegistry();

static int g_total = 0;
static int g_passed = 0;

#define TEST(name)                                              \
    do {                                                        \
        ++g_total;                                              \
        std::cout << "  [" << g_total << "] " << #name << "... "; \
        test_##name();                                          \
        ++g_passed;                                             \
        std::cout << "OK" << std::endl;                         \
    } while (0)

#define GROUP(title) \
    std::cout << std::endl << "==== " << title << " ====" << std::endl;

int main()
{
    std::cout << "7gui523 Unit Tests" << std::endl;
    std::cout << "===================" << std::endl;

    GROUP("cardtype: parseCardType")
    TEST(parseCardType);

    GROUP("cardtype: canBeat")
    TEST(canBeat);

    GROUP("score: calculateScore")
    TEST(calculatorScore);

    GROUP("score: tableScore")
    TEST(tableScore);

    GROUP("score: pressureBonus")
    TEST(pressureBonus);

    GROUP("special: checkSpecialVictory")
    TEST(checkSpecialVictory);

    GROUP("deck: createStandardDeck")
    TEST(createStandardDeck);

    GROUP("deck: drawCards")
    TEST(drawCards);

    GROUP("deck: refillToFive")
    TEST(refillToFive);

    GROUP("deck: removeCards")
    TEST(removeCards);

    GROUP("deck: drawRandom")
    TEST(drawRandom);

    GROUP("deck: splitDeck")
    TEST(splitDeck);

    GROUP("player: removeCardsFromHand")
    TEST(removeCardsFromHand);

    GROUP("campaign: init")
    TEST(campaignInit);

    GROUP("campaign: startLevel")
    TEST(campaignStartLevel);

    GROUP("campaign: removeTable")
    TEST(removeTable);

    GROUP("campaign: settlement win")
    TEST(campaignSettlement);

    GROUP("campaign: settlement lose")
    TEST(campaignSettlementBossWins);

    GROUP("campaign: hp cap")
    TEST(campaignHpCap);

    GROUP("campaign: game over")
    TEST(campaignGameOver);

    GROUP("search: P0 regressions")
    TEST(sampledWorldConservation);
    TEST(searchFinalSettlement);
    TEST(searchTerminalUtility);

    GROUP("tracker: dual side")
    TEST(trackerDualSide);

    GROUP("variant: registry")
    TEST(variantRegistry);

    std::cout << std::endl;
    std::cout << "Result: " << g_passed << " / " << g_total << " passed";
    if (g_passed == g_total) {
        std::cout << "  ALL OK!" << std::endl;
    } else {
        std::cout << "  " << (g_total - g_passed) << " FAILED!" << std::endl;
    }
    return (g_passed == g_total) ? 0 : 1;
}