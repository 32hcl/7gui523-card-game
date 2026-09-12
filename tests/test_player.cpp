#include <cassert>
#include "core/player.h"
#include "core/deck.h"

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

void test_removeCardsFromHand()
{
    {
        Player p = createPlayer("test");
        p.hand = {makeCard("7"), makeCard("5"), makeCard("K")};
        removeCardsFromHand(p, {makeCard("5")});
        assert(p.hand.size() == 2);
    }

    {
        Player p = createPlayer("test");
        p.hand = {makeCard("7"), makeCard("5")};
        removeCardsFromHand(p, {makeCard("A")});
        assert(p.hand.size() == 2);
    }

    {
        Player p = createPlayer("test");
        p.hand = {makeCard("7"), makeCard("5", "黑桃"), makeCard("5", "红桃")};
        removeCardsFromHand(p, {makeCard("5", "红桃")});
        assert(p.hand.size() == 2);
    }

    {
        Player p = createPlayer("test");
        p.hand = {};
        removeCardsFromHand(p, {makeCard("A")});
        assert(p.hand.empty());
    }
}