#pragma once

#include <map>
#include <vector>
#include "core/card/card.h"

namespace VariantRegistry {
    void record(int seq, const Card& originalCard);
    Card lookup(int seq);
    bool has(int seq);
    void clear();
    std::vector<Card> lookupBatch(const std::vector<Card>& cards);
}