#include <cassert>
#include "core/cardtype.h"

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

void test_parseCardType()
{
    {
        CardType type = parseCardType({makeCard("大鬼")}).type;
        assert(type == CardType::Single);
    }
    {
        CardType type = parseCardType({makeCard("K")}).type;
        assert(type == CardType::Single);
    }

    {
        CardType result = parseCardType({makeCard("5"), makeCard("5")}).type;
        assert(result == CardType::Pair);
    }
    {
        CardType result = parseCardType({makeCard("K"), makeCard("K")}).type;
        assert(result == CardType::Pair);
    }

    {
        CardType result = parseCardType({makeCard("5"), makeCard("6")}).type;
        assert(result == CardType::Invalid);
    }

    {
        CardType result = parseCardType({makeCard("大鬼"), makeCard("小鬼")}).type;
        assert(result == CardType::Rocket);
    }

    {
        CardType result = parseCardType({makeCard("大鬼"), makeCard("大鬼")}).type;
        assert(result == CardType::Invalid);
    }

    {
        CardType result = parseCardType({
            makeCard("7"), makeCard("7"), makeCard("7")
        }).type;
        assert(result == CardType::Triple);
    }

    {
        auto r = parseCardType({
            makeCard("7", "黑桃"), makeCard("7", "红桃"), makeCard("7", "梅花"),
            makeCard("3")
        });
        assert(r.type == CardType::TripleWithOne);
        assert(r.keyPoint == "7");
    }

    {
        auto r = parseCardType({
            makeCard("7"), makeCard("7"), makeCard("7"),
            makeCard("5"), makeCard("5")
        });
        assert(r.type == CardType::TripleWithTwo);
        assert(r.keyPoint == "7");
    }

    {
        CardType t = parseCardType({
            makeCard("7"), makeCard("7"), makeCard("7"),
            makeCard("5"), makeCard("6")
        }).type;
        assert(t == CardType::Invalid);
    }

    {
        CardType t = parseCardType({
            makeCard("7"), makeCard("7"),
            makeCard("5"), makeCard("5"),
            makeCard("3")
        }).type;
        assert(t == CardType::Invalid);
    }

    {
        CardType t = parseCardType({
            makeCard("7"), makeCard("7"), makeCard("7"), makeCard("7"),
            makeCard("3")
        }).type;
        assert(t == CardType::Invalid);
    }

    {
        CardType t = parseCardType({
            makeCard("7"), makeCard("7"), makeCard("7"), makeCard("7")
        }).type;
        assert(t == CardType::Bomb);
    }

    {
        CardType t = parseCardType({makeCard("大鬼"), makeCard("小鬼")}).type;
        assert(t == CardType::Rocket);
    }
}

void test_canBeat()
{
    {
        auto single7 = parseCardType({makeCard("7")});
        auto single5 = parseCardType({makeCard("5")});
        assert(canBeat(single7, single5));
        assert(!canBeat(single5, single7));
    }

    {
        auto prev = parseCardType({makeCard("7")});
        assert(canBeat(prev, prev));
    }

    {
        auto bigJoker  = parseCardType({makeCard("大鬼")});
        auto smallJoker = parseCardType({makeCard("小鬼")});
        assert(canBeat(bigJoker, smallJoker));
        assert(!canBeat(smallJoker, bigJoker));
    }

    {
        auto pair = parseCardType({makeCard("7"), makeCard("7")});
        auto single5 = parseCardType({makeCard("5")});
        assert(!canBeat(pair, single5));
    }

    {
        auto bomb = parseCardType({
            makeCard("4"), makeCard("4"), makeCard("4"), makeCard("4")
        });
        assert(canBeat(bomb, parseCardType({makeCard("7")})));
        assert(canBeat(bomb, parseCardType({makeCard("7"), makeCard("7")})));
        assert(canBeat(bomb, parseCardType({
            makeCard("7"), makeCard("7"), makeCard("7")
        })));
    }

    {
        auto rocket = parseCardType({makeCard("大鬼"), makeCard("小鬼")});
        auto bomb7 = parseCardType({
            makeCard("7"), makeCard("7"), makeCard("7"), makeCard("7")
        });
        assert(canBeat(rocket, bomb7));
        assert(!canBeat(bomb7, rocket));
    }

    {
        auto rocket = parseCardType({makeCard("大鬼"), makeCard("小鬼")});
        assert(!canBeat(rocket, rocket));
    }

    {
        auto tripleWithTwo = parseCardType({
            makeCard("8"), makeCard("8"), makeCard("8"),
            makeCard("4"), makeCard("4")
        });
        auto bomb = parseCardType({
            makeCard("3"), makeCard("3"), makeCard("3"), makeCard("3")
        });
        assert(canBeat(bomb, tripleWithTwo));
    }
}