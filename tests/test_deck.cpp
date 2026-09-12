#include <cassert>
#include "core/deck.h"
#include "core/player.h"

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

void test_createStandardDeck()
{
    Deck d = createStandardDeck();
    assert(d.cards.size() == 54);
}

void test_drawCards()
{
    {
        Deck d = createStandardDeck();
        auto drawn = drawCards(d, 5);
        assert(drawn.size() == 5);
        assert(d.cards.size() == 49);
    }

    {
        Deck d = createStandardDeck();
        drawCards(d, 50);
        assert(d.cards.size() == 4);
        auto drawn = drawCards(d, 10);
        assert(drawn.size() == 4);
        assert(d.cards.empty());
    }

    {
        Deck d;
        auto drawn = drawCards(d, 5);
        assert(drawn.empty());
    }

    {
        Deck d = createStandardDeck();
        auto drawn = drawCards(d, 0);
        assert(drawn.empty());
        assert(d.cards.size() == 54);
    }
}

void test_refillToFive()
{
    {
        Deck d = createStandardDeck();
        Player p = createPlayer("test");
        dealCards(p, d, 3);
        assert(p.hand.size() == 3);
        refillToFive(p, d);
        assert(p.hand.size() == 5);
    }

    {
        Deck d;
        Player p = createPlayer("test");
        p.hand = {makeCard("7"), makeCard("5")};
        refillToFive(p, d);
        assert(p.hand.size() == 2);
    }

    {
        Deck d = createStandardDeck();
        Player p = createPlayer("test");
        p.hand = {makeCard("A"), makeCard("A"), makeCard("A"), makeCard("A"), makeCard("A")};
        refillToFive(p, d);
        assert(p.hand.size() == 5);
        assert(d.cards.size() == 54);
    }
}