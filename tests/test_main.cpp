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
void test_removeCardsFromHand();

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

    GROUP("player: removeCardsFromHand")
    TEST(removeCardsFromHand);

    std::cout << std::endl;
    std::cout << "Result: " << g_passed << " / " << g_total << " passed";
    if (g_passed == g_total) {
        std::cout << "  ALL OK!" << std::endl;
    } else {
        std::cout << "  " << (g_total - g_passed) << " FAILED!" << std::endl;
    }
    return (g_passed == g_total) ? 0 : 1;
}