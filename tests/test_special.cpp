#include <cassert>
#include "core/special.h"

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

void test_checkSpecialVictory()
{
    {
        Player p = createPlayer("test");
        p.hand = {
            makeCard("7"), makeCard("大鬼"), makeCard("5"),
            makeCard("2"), makeCard("3")
        };
        assert(checkSpecialVictory(p));
    }

    {
        Player p = createPlayer("test");
        p.hand = {
            makeCard("7"), makeCard("小鬼"), makeCard("5"),
            makeCard("2"), makeCard("3")
        };
        assert(checkSpecialVictory(p));
    }

    {
        Player p = createPlayer("test");
        p.hand = {
            makeCard("7"), makeCard("大鬼"), makeCard("5"),
            makeCard("2")
        };
        assert(!checkSpecialVictory(p));
    }

    {
        Player p = createPlayer("test");
        p.hand = {
            makeCard("7"), makeCard("A"), makeCard("5"),
            makeCard("2"), makeCard("3")
        };
        assert(!checkSpecialVictory(p));
    }

    {
        Player p = createPlayer("test");
        assert(!checkSpecialVictory(p));
    }

    {
        Player p = createPlayer("test");
        p.hand = {
            makeCard("7"), makeCard("大鬼"), makeCard("5"),
            makeCard("2"), makeCard("3"), makeCard("4")
        };
        assert(checkSpecialVictory(p));
    }
}