#pragma once

#include "cfr_types.h"
#include <unordered_map>
#include <vector>
#include <string>

class StrategyStore {
public:
    void setStrategy(const InfoSetKey& key, const std::vector<double>& avgStrategy);

    std::vector<double> query(const InfoSetKey& key) const;

    bool save(const std::string& filepath) const;
    bool load(const std::string& filepath);

    size_t size() const { return table_.size(); }

    void clear() { table_.clear(); }

    void importFromNodes(
        const std::unordered_map<InfoSetKey, CFRNode, InfoSetKeyHash>& nodes
    );

private:
    std::unordered_map<InfoSetKey, std::vector<double>, InfoSetKeyHash> table_;
};