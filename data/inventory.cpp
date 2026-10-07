#include "inventory.h"
#include <algorithm>

void Inventory::add(const std::string& itemId) {
    m_items.push_back(itemId);
}

bool Inventory::remove(const std::string& itemId) {
    auto it = std::find(m_items.begin(), m_items.end(), itemId);
    if (it == m_items.end()) return false;
    m_items.erase(it);
    return true;
}

bool Inventory::has(const std::string& itemId) const {
    return std::find(m_items.begin(), m_items.end(), itemId) != m_items.end();
}

std::vector<std::string> Inventory::allItems() const {
    return m_items;
}