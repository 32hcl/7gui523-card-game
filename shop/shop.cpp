#include "shop.h"
#include <algorithm>

std::vector<ShopItem> Shop::listItems() const {
    return m_items;
}

bool Shop::buy(const std::string& itemId) {
    (void)itemId;
    return false;
}