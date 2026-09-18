#pragma once
#include <string>
#include <vector>

struct ShopItem {
    std::string id;
    std::string name;
    int price = 0;
};

class Shop {
public:
    Shop() = default;

    std::vector<ShopItem> listItems() const;
    bool buy(const std::string& itemId);

private:
    std::vector<ShopItem> m_items;
};