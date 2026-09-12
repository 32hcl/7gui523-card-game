#include <cassert>
#include "core/cardtype.h"
#include "core/score.h"

static Card makeCard(const std::string& point, const std::string& suit = "")
{
    Card c;
    c.point = point;
    c.suit = suit;
    if (point == "5")       c.score = 5;
    else if (point == "10") c.score = 10;
    else if (point == "K")  c.score = 20;
    else                    c.score = 0;
    return c;
}

void test_calculatorScore()
{
    assert(calculateScore({makeCard("5")}) == 5);
    assert(calculateScore({makeCard("10")}) == 10);
    assert(calculateScore({makeCard("K")}) == 20);
    assert(calculateScore({makeCard("A")}) == 0);
    assert(calculateScore({
        makeCard("5"), makeCard("10"), makeCard("K")
    }) == 35);

    assert(calculateScore({
        makeCard("5"), makeCard("10"), makeCard("K"),
        makeCard("5"), makeCard("K")
    }) == 60);

    assert(calculateScore({}) == 0);
}

void test_tableScore()
{
    assert(calculateTableScore({makeCard("5")}, 0) == 5);
    assert(calculateTableScore({makeCard("5")}, 10) == 15);
    assert(calculateTableScore({}, 20) == 20);
    assert(calculateTableScore({
        makeCard("5"), makeCard("10"), makeCard("K")
    }, 0) == 35);
}

void test_pressureBonus()
{
    {
        CardTypeResult empty;
        empty.type = CardType::Invalid;
        auto single5 = parseCardType({makeCard("5")});
        assert(calculatePressureBonus(single5, empty) == 0);
    }

    {
        auto s1 = parseCardType({makeCard("5")});
        auto s2 = parseCardType({makeCard("5")});
        assert(calculatePressureBonus(s2, s1) == 5);
    }

    {
        auto s1 = parseCardType({makeCard("10")});
        auto s2 = parseCardType({makeCard("10")});
        assert(calculatePressureBonus(s2, s1) == 10);
    }

    {
        auto s1 = parseCardType({makeCard("K")});
        auto s2 = parseCardType({makeCard("K")});
        assert(calculatePressureBonus(s2, s1) == 20);
    }

    {
        auto p1 = parseCardType({makeCard("5"), makeCard("5")});
        auto p2 = parseCardType({makeCard("5"), makeCard("5")});
        assert(calculatePressureBonus(p2, p1) == 10);
    }

    {
        auto p1 = parseCardType({makeCard("10"), makeCard("10")});
        auto p2 = parseCardType({makeCard("10"), makeCard("10")});
        assert(calculatePressureBonus(p2, p1) == 20);
    }

    {
        auto p1 = parseCardType({makeCard("K"), makeCard("K")});
        auto p2 = parseCardType({makeCard("K"), makeCard("K")});
        assert(calculatePressureBonus(p2, p1) == 40);
    }

    {
        auto s1 = parseCardType({makeCard("5")});
        auto s2 = parseCardType({makeCard("10")});
        assert(calculatePressureBonus(s2, s1) == 0);
    }

    {
        auto s1 = parseCardType({makeCard("7")});
        auto s2 = parseCardType({makeCard("7")});
        assert(calculatePressureBonus(s2, s1) == 0);
    }

    {
        auto t1 = parseCardType({
            makeCard("7"), makeCard("7"), makeCard("7"),
            makeCard("5"), makeCard("5")
        });
        auto t2 = parseCardType({
            makeCard("7"), makeCard("7"), makeCard("7"),
            makeCard("5"), makeCard("5")
        });
        assert(calculatePressureBonus(t2, t1) == 0);
    }

    {
        auto s5 = parseCardType({makeCard("5")});
        auto p5 = parseCardType({makeCard("5"), makeCard("5")});
        assert(calculatePressureBonus(p5, s5) == 0);
    }
}