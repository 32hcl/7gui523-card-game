#include "variant_registry.h"

namespace VariantRegistry {
    static std::map<int, Card> s_registry;

    void record(int seq, const Card& originalCard) {
        s_registry[seq] = originalCard;
    }

    Card lookup(int seq) {
        auto it = s_registry.find(seq);
        if (it != s_registry.end()) return it->second;
        return Card{};
    }

    bool has(int seq) {
        return s_registry.find(seq) != s_registry.end();
    }

    void clear() {
        s_registry.clear();
    }

    std::vector<Card> lookupBatch(const std::vector<Card>& cards) {
        std::vector<Card> result;
        result.reserve(cards.size());
        for (const Card& c : cards) {
            auto it = s_registry.find(c.seq);
            if (it != s_registry.end()) {
                result.push_back(it->second);
            } else {
                result.push_back(c);
            }
        }
        return result;
    }
}