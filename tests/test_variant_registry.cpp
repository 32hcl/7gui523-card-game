#include <cassert>
#include "core/variant/variant_registry.h"

static Card makeCard(const std::string& point, int seq)
{
    Card c;
    c.point = point;
    c.seq = seq;
    return c;
}

void test_variantRegistry()
{
    using namespace VariantRegistry;

    Card orig = makeCard("4", 100);
    Card variant = makeCard("A", 100);

    record(100, orig);
    assert(has(100));
    assert(!has(200));

    Card looked = lookup(100);
    assert(looked.point == "4");
    assert(looked.seq == 100);

    Card notFound = lookup(999);
    assert(notFound.point == "");

    clear();
    assert(!has(100));

    record(10, makeCard("5", 10));
    record(20, makeCard("10", 20));
    record(30, makeCard("K", 30));

    std::vector<Card> input = {
        makeCard("A", 10),
        makeCard("A", 20),
        makeCard("A", 30),
        makeCard("A", 40),
    };
    auto batch = lookupBatch(input);
    assert(batch.size() == 4);
    assert(batch[0].point == "5");
    assert(batch[1].point == "10");
    assert(batch[2].point == "K");
    assert(batch[3].point == "A");
}