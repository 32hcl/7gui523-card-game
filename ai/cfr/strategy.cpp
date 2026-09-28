#include "strategy.h"
#include <fstream>

void StrategyStore::setStrategy(const InfoSetKey& key, const std::vector<double>& avgStrategy) {
    table_[key] = avgStrategy;
}

std::vector<double> StrategyStore::query(const InfoSetKey& key) const {
    auto it = table_.find(key);
    if (it == table_.end()) return {};
    return it->second;
}

bool StrategyStore::save(const std::string& filepath) const {
    std::ofstream ofs(filepath, std::ios::binary);
    if (!ofs) return false;

    size_t sz = table_.size();
    ofs.write((const char*)&sz, sizeof(sz));

    for (const auto& kv : table_) {
        const InfoSetKey& key = kv.first;
        const auto& strat = kv.second;

        ofs.write((const char*)&key, sizeof(key));

        size_t stratSize = strat.size();
        ofs.write((const char*)&stratSize, sizeof(stratSize));
        ofs.write((const char*)strat.data(), stratSize * sizeof(double));
    }
    return true;
}

bool StrategyStore::load(const std::string& filepath) {
    std::ifstream ifs(filepath, std::ios::binary);
    if (!ifs) return false;

    table_.clear();
    size_t sz = 0;
    ifs.read((char*)&sz, sizeof(sz));

    for (size_t i = 0; i < sz; ++i) {
        InfoSetKey key;
        ifs.read((char*)&key, sizeof(key));

        size_t stratSize = 0;
        ifs.read((char*)&stratSize, sizeof(stratSize));

        std::vector<double> strat(stratSize);
        ifs.read((char*)strat.data(), stratSize * sizeof(double));
        table_[key] = strat;
    }
    return true;
}

void StrategyStore::importFromNodes(
    const std::unordered_map<InfoSetKey, CFRNode, InfoSetKeyHash>& nodes)
{
    table_.clear();
    for (const auto& kv : nodes) {
        const auto& node = kv.second;
        int n = (int)node.strategySum.size();
        if (n == 0) continue;

        std::vector<double> avgStrat(n, 0.0);
        double sum = 0.0;
        for (double s : node.strategySum) sum += s;
        if (sum > 0.0) {
            for (int i = 0; i < n; ++i) avgStrat[i] = node.strategySum[i] / sum;
        } else {
            double u = 1.0 / n;
            for (int i = 0; i < n; ++i) avgStrat[i] = u;
        }
        table_[kv.first] = avgStrat;
    }
}