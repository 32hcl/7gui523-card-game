#include <cassert>
#include "core/card/deck.h"
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

void test_removeCards()
{
    Deck full = createStandardDeck();

    {
        auto r = removeCards(full.cards, {});
        assert(r.size() == 54);
    }
    {
        auto r = removeCards(full.cards, {"4"});
        assert(r.size() == 53);
    }
    {
        auto r = removeCards(full.cards, {"4", "4"});
        assert(r.size() == 52);
    }
    {
        auto r = removeCards(full.cards, {"4", "4", "4", "4", "6", "6", "6", "6", "8", "8", "8", "8"});
        assert(r.size() == 42);
    }
    {
        auto r = removeCards(full.cards, {"大鬼"});
        assert(r.size() == 53);
    }
}

void test_drawRandom()
{
    std::vector<Card> pool = createStandardDeck().cards;

    {
        auto drawn = drawRandom(pool, 10);
        assert(drawn.size() == 10);
        assert(pool.size() == 44);
    }
    {
        auto drawn = drawRandom(pool, 100);
        assert(drawn.size() == 44);
        assert(pool.size() == 0);
    }
    {
        auto drawn = drawRandom(pool, 5);
        assert(drawn.size() == 0);
    }
}

void test_splitDeck()
{
    std::vector<Card> deck = createStandardDeck().cards;
    std::vector<Card> player, ai;

    splitDeck(deck, player, ai);

    assert(player.size() == 27);
    assert(ai.size() == 27);
    assert(deck.empty());
}