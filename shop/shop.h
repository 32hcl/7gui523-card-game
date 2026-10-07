#pragma once
#include <string>
#include <vector>
#include <random>
#include "core/card/card.h"

// 商店物品类型
enum class ShopItemType {
    HealSmall,      // 小回复药水
    HealLarge,      // 大回复药水
    CardJokerBig,   // 大王
    CardJokerSmall, // 小王
    Card5,          // 5点牌（随机花色）
    Card2,          // 2点牌（随机花色）
    Card3,          // 3点牌（随机花色）
    CardA,          // A点牌（随机花色）
    CardK,          // K点牌（随机花色）
    CardQ,          // Q点牌（随机花色）
    CardJ,          // J点牌（随机花色）
    Card10,         // 10点牌（随机花色）
    Card9,          // 9点牌（随机花色）
    Card8,          // 8点牌（随机花色）
    Card7,          // 7点牌（随机花色）
    Card6,          // 6点牌（随机花色）
    Card4,          // 4点牌（随机花色）
};

// 购买卡牌的插入目标
enum class CardDestination {
    ToHand,       // 进入下一关手牌
    ToPlayDeck,   // 洗入游玩牌堆（本关可抽）
    ToHiddenDeck,  // 进入隐藏牌堆（保留，扣血换牌）
};

// 商店单件物品
struct ShopItem {
    ShopItemType type;
    std::string  name;
    std::string  desc;
    int          price   = 0;
    int          stock   = 0;   // -1 表示无限库存
    CardDestination dest = CardDestination::ToPlayDeck;  // 默认洗入牌堆
};

// 购买结果
struct ShopPurchaseResult {
    bool       success  = false;
    std::string message;
    Card        card;          // 购买卡牌时返回的牌
    CardDestination dest = CardDestination::ToPlayDeck;
    int         hpHealed = 0;  // 购买回血时返回的回复量
};

// 商店数据层：物品列表、购买逻辑
class Shop {
public:
    Shop();

    const std::vector<ShopItem>& items() const { return m_items; }

    // 购买物品（会扣减库存，每关刷新后库存复原）
    ShopPurchaseResult buy(ShopItemType type, int currentGold, int currentHp, int maxHp);

    // 查询剩余库存（-1 表示无限）
    int stockOf(ShopItemType type) const;

    // 重置所有物品库存到初始值（刷新商店时调用）
    void resetStock();

    // 每级商品基础价格
    static int basePrice(ShopItemType type);

private:
    std::vector<ShopItem> m_items;
    mutable std::mt19937 m_rng{std::random_device{}()};
};