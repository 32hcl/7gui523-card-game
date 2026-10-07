#pragma once
#include <string>
#include <vector>

class Inventory {
public:
    Inventory() = default;

    void add(const std::string& itemId);
    bool remove(const std::string& itemId);
    bool has(const std::string& itemId) const;
    std::vector<std::string> allItems() const;

private:
    std::vector<std::string> m_items;
};