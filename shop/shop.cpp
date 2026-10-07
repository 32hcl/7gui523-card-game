#include "shop/shop.h"
#include <random>

static const std::string kSuits[] = {"黑桃", "红桃", "梅花", "方块"};

static std::string randomSuit(std::mt19937& rng) {
    std::uniform_int_distribution<int> dist(0, 3);
    return kSuits[dist(rng)];
}

static CardDestination randomDest(std::mt19937& rng) {
    std::uniform_int_distribution<int> dist(0, 2);
    switch (dist(rng)) {
        case 0: return CardDestination::ToPlayDeck;
        case 1: return CardDestination::ToHiddenDeck;
        default: return CardDestination::ToHand;
    }
}

static std::string destLabel(CardDestination d) {
    switch (d) {
        case CardDestination::ToPlayDeck:  return "洗入游玩牌堆";
        case CardDestination::ToHiddenDeck: return "进入隐藏牌堆";
        case CardDestination::ToHand:      return "进入下关手牌";
    }
    return "";
}

Shop::Shop() {
    resetStock();
}

int Shop::basePrice(ShopItemType type) {
    switch (type) {
        case ShopItemType::HealSmall:      return 30;
        case ShopItemType::HealLarge:      return 60;
        case ShopItemType::CardJokerBig:   return 100;
        case ShopItemType::CardJokerSmall: return 80;
        case ShopItemType::Card5:          return 40;
        case ShopItemType::Card2:          return 30;
        case ShopItemType::Card3:          return 25;
        case ShopItemType::CardA:          return 35;
        case ShopItemType::CardK:          return 30;
        case ShopItemType::CardQ:          return 28;
        case ShopItemType::CardJ:          return 26;
        case ShopItemType::Card10:         return 35;
        case ShopItemType::Card9:          return 18;
        case ShopItemType::Card8:          return 18;
        case ShopItemType::Card7:          return 15;
        case ShopItemType::Card6:          return 15;
        case ShopItemType::Card4:          return 12;
    }
    return 0;
}

static std::string pointForType(ShopItemType type) {
    switch (type) {
        case ShopItemType::CardA:    return "A";
        case ShopItemType::CardK:    return "K";
        case ShopItemType::CardQ:    return "Q";
        case ShopItemType::CardJ:    return "J";
        case ShopItemType::Card10:   return "10";
        case ShopItemType::Card9:    return "9";
        case ShopItemType::Card8:    return "8";
        case ShopItemType::Card7:    return "7";
        case ShopItemType::Card6:    return "6";
        case ShopItemType::Card4:    return "4";
        default: return "";
    }
}

ShopPurchaseResult Shop::buy(ShopItemType type, int currentGold, int currentHp, int maxHp) {
    ShopPurchaseResult res;
    int price = basePrice(type);

    // 查找物品
    ShopItem* item = nullptr;
    for (auto& it : m_items) {
        if (it.type == type) { item = &it; break; }
    }
    if (!item) {
        res.message = "物品不存在！";
        return res;
    }

    // 检查库存（-1 无限）
    if (item->stock == 0) {
        res.message = "已售罄！";
        return res;
    }

    if (currentGold < price) {
        res.message = "筹码不足！";
        return res;
    }

    switch (type) {
        case ShopItemType::HealSmall: {
            if (currentHp >= maxHp) {
                res.message = "HP 已满，无需回复！";
                return res;
            }
            int heal = 20;
            res.success = true;
            res.hpHealed = heal;
            res.message = "回复了 20 HP！";
            break;
        }
        case ShopItemType::HealLarge: {
            if (currentHp >= maxHp) {
                res.message = "HP 已满，无需回复！";
                return res;
            }
            int heal = 50;
            res.success = true;
            res.hpHealed = heal;
            res.message = "回复了 50 HP！";
            break;
        }
        case ShopItemType::CardJokerBig: {
            res.card = Card{"大鬼", "", 0, 0};
            res.dest = item->dest;
            res.success = true;
            res.message = "获得 大王！ → " + destLabel(res.dest);
            break;
        }
        case ShopItemType::CardJokerSmall: {
            res.card = Card{"小鬼", "", 0, 0};
            res.dest = item->dest;
            res.success = true;
            res.message = "获得 小王！ → " + destLabel(res.dest);
            break;
        }
        case ShopItemType::Card5: {
            res.card = Card{"5", randomSuit(m_rng), 0, 0};
            res.dest = item->dest;
            res.success = true;
            res.message = "获得 5点牌！ → " + destLabel(res.dest);
            break;
        }
        case ShopItemType::Card2: {
            res.card = Card{"2", randomSuit(m_rng), 0, 0};
            res.dest = item->dest;
            res.success = true;
            res.message = "获得 2点牌！ → " + destLabel(res.dest);
            break;
        }
        case ShopItemType::Card3: {
            res.card = Card{"3", randomSuit(m_rng), 0, 0};
            res.dest = item->dest;
            res.success = true;
            res.message = "获得 3点牌！ → " + destLabel(res.dest);
            break;
        }
        default: {
            std::string pt = pointForType(type);
            if (pt.empty()) { res.message = "未知物品！"; return res; }
            res.card = Card{pt, randomSuit(m_rng), 0, 0};
            res.dest = item->dest;
            res.success = true;
            res.message = "获得 " + pt + "点牌！ → " + destLabel(res.dest);
            break;
        }
    }

    // 扣减库存（-1 表示无限，不扣）
    if (res.success && item->stock > 0) {
        item->stock--;
    }

    return res;
}

void Shop::resetStock() {
    m_items = {
        // ── 回血物品 ──
        { ShopItemType::HealSmall,  "小回复药水", "回复 20 HP",   30, -1 },
        { ShopItemType::HealLarge,  "大回复药水", "回复 50 HP",   60, -1 },
        // ── 鬼牌 ──
        { ShopItemType::CardJokerBig,   "大王", "最大的单张牌",     100, 1, randomDest(m_rng) },
        { ShopItemType::CardJokerSmall, "小王", "第二大的单张牌",    80, 1, randomDest(m_rng) },
        // ── 计分牌 ──
        { ShopItemType::Card5,  "随机5点牌", "计分牌×5",   40, 2, randomDest(m_rng) },
        { ShopItemType::Card2,  "随机2点牌", "计分牌×2",   30, 3, randomDest(m_rng) },
        { ShopItemType::Card3,  "随机3点牌", "计分牌×3",   25, 3, randomDest(m_rng) },
        // ── 新增大牌 ──
        { ShopItemType::CardA,  "随机A点牌", "大单牌+连牌",  35, 2, randomDest(m_rng) },
        { ShopItemType::CardK,  "随机K点牌", "10分计分牌",   30, 2, randomDest(m_rng) },
        { ShopItemType::CardQ,  "随机Q点牌", "10分计分牌",   28, 2, randomDest(m_rng) },
        { ShopItemType::CardJ,  "随机J点牌", "计分牌×1",    26, 2, randomDest(m_rng) },
        { ShopItemType::Card10, "随机10点牌", "10分核心计分",  35, 2, randomDest(m_rng) },
        { ShopItemType::Card9,  "随机9点牌", "连牌/顺子用",    18, 3, randomDest(m_rng) },
        { ShopItemType::Card8,  "随机8点牌", "连牌/顺子用",    18, 3, randomDest(m_rng) },
        { ShopItemType::Card7,  "随机7点牌", "连牌/顺子用",    15, 3, randomDest(m_rng) },
        { ShopItemType::Card6,  "随机6点牌", "连牌/顺子用",    15, 3, randomDest(m_rng) },
        { ShopItemType::Card4,  "随机4点牌", "连牌/顺子用",    12, 3, randomDest(m_rng) },
    };
}

int Shop::stockOf(ShopItemType type) const {
    for (const auto& it : m_items) {
        if (it.type == type) return it.stock;
    }
    return 0;
}