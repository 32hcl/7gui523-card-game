class_name Shop
extends RefCounted

# ═══════════════════════════════════════════════════════════════
# Shop — 商店系统
#
# 一级商店：回复药水 / 卡牌（分区，不可刷新区关末自动补货）
#   不可刷新区 — 回复 + TO_HAND（每关结束自动补货）
#   可刷新区   — TO_PLAY_DECK / TO_HIDDEN（花筹码手动刷新）
#
# 二级附魔商店：本关消费 ≥ 32 筹码解锁
#   每关结束自动补货  |  每次展示 2 件  |  不可重复购买
#   附魔永久存在于小丑牌，跨关继承
# ═══════════════════════════════════════════════════════════════


# ═══════════════════════════════════════════════════════════════
# 一级商店 — 枚举
# ═══════════════════════════════════════════════════════════════

enum Section { FIXED, REFRESHABLE }

enum ItemType {
	HEAL_SMALL, HEAL_LARGE,
	CARD_5, CARD_2, CARD_3,
	CARD_A, CARD_K, CARD_Q, CARD_J,
	CARD_10, CARD_9, CARD_8, CARD_7, CARD_6, CARD_4,
}

enum CardDest {
	TO_HAND,
	TO_PLAY_DECK,
	TO_HIDDEN,
	TO_ANY,          ## 整副牌堆随机（游戏+隐藏合并区间）
}


# ═══════════════════════════════════════════════════════════════
# 一级商店 — 数据结构
# ═══════════════════════════════════════════════════════════════

class ShopItem:
	var type: ItemType
	var name: String
	var desc: String
	var price: int = 0
	var stock: int = 0
	var max_stock: int = 0
	var section: Section = Section.FIXED
	var dest: CardDest = CardDest.TO_PLAY_DECK


class PurchaseResult:
	var success := false
	var message: String = ""
	var card: Card = null
	var dest: CardDest = CardDest.TO_PLAY_DECK
	var hp_healed := 0


# ═══════════════════════════════════════════════════════════════
# 一级商店 — 价格/点数映射
# ═══════════════════════════════════════════════════════════════

const SUITS := ["黑桃", "红桃", "梅花", "方块"]

const BASE_PRICE := {
	ItemType.HEAL_SMALL: 30,
	ItemType.HEAL_LARGE: 60,
	ItemType.CARD_5: 40,
	ItemType.CARD_2: 30,
	ItemType.CARD_3: 25,
	ItemType.CARD_A: 35,
	ItemType.CARD_K: 30,
	ItemType.CARD_Q: 28,
	ItemType.CARD_J: 26,
	ItemType.CARD_10: 35,
	ItemType.CARD_9: 18,
	ItemType.CARD_8: 18,
	ItemType.CARD_7: 15,
	ItemType.CARD_6: 15,
	ItemType.CARD_4: 12,
}

const POINT_FOR := {
	ItemType.CARD_A: "A",
	ItemType.CARD_K: "K",
	ItemType.CARD_Q: "Q",
	ItemType.CARD_J: "J",
	ItemType.CARD_10: "10",
	ItemType.CARD_9: "9",
	ItemType.CARD_8: "8",
	ItemType.CARD_7: "7",
	ItemType.CARD_6: "6",
	ItemType.CARD_4: "4",
	ItemType.CARD_5: "5",
	ItemType.CARD_2: "2",
	ItemType.CARD_3: "3",
}

const DEST_LABEL := {
	CardDest.TO_HAND:      "进入下关手牌",
	CardDest.TO_PLAY_DECK: "随机洗入游玩牌堆",
	CardDest.TO_HIDDEN:    "随机洗入隐藏牌堆",
	CardDest.TO_ANY:       "随机洗入整副牌堆",
}

const REFRESH_COST := 30
const ENCHANT_UNLOCK_SPEND := 32
const ENCHANT_SLOTS := 2


# ═══════════════════════════════════════════════════════════════
# 一级商店 — 实例
# ═══════════════════════════════════════════════════════════════

var items: Array[ShopItem] = []
var level_spent_chips := 0


func reset_level_spending() -> void:
	level_spent_chips = 0


func reset_stock() -> void:
	reset_level_spending()
	items = [
		# ═══════ 不可刷新区 ═══════
		# 回复
		_heal(ItemType.HEAL_SMALL, "小回复药水", "回复 20 HP", 30),
		_heal(ItemType.HEAL_LARGE, "大回复药水", "回复 50 HP", 60),

		# TO_HAND
		_fixed(ItemType.CARD_5,  "5点牌",  "伤害 5HP",      40, 2, CardDest.TO_HAND),
		_fixed(ItemType.CARD_2,  "2点牌",  "大单压牌",      30, 3, CardDest.TO_HAND),
		_fixed(ItemType.CARD_3,  "3点牌",  "大单压牌",      25, 3, CardDest.TO_HAND),
		_fixed(ItemType.CARD_K,  "K点牌",  "伤害 20HP",     30, 2, CardDest.TO_HAND),
		_fixed(ItemType.CARD_10, "10点牌", "伤害 10HP",     35, 2, CardDest.TO_HAND),

		# ═══════ 可刷新区 ═══════
		# TO_PLAY_DECK
		_refresh(ItemType.CARD_A,  "A点牌",  "大单压牌",      35, 2, CardDest.TO_PLAY_DECK),
		_refresh(ItemType.CARD_Q,  "Q点牌",  "大单压牌",      28, 2, CardDest.TO_PLAY_DECK),
		_refresh(ItemType.CARD_J,  "J点牌",  "连牌用",        26, 2, CardDest.TO_PLAY_DECK),
		_refresh(ItemType.CARD_9,  "9点牌",  "连牌/顺子用",   18, 3, CardDest.TO_PLAY_DECK),
		_refresh(ItemType.CARD_8,  "8点牌",  "连牌/顺子用",   18, 3, CardDest.TO_PLAY_DECK),
		_refresh(ItemType.CARD_7,  "7点牌",  "连牌/顺子用",   15, 3, CardDest.TO_PLAY_DECK),
		_refresh(ItemType.CARD_6,  "6点牌",  "连牌/顺子用",   15, 3, CardDest.TO_PLAY_DECK),
		_refresh(ItemType.CARD_4,  "4点牌",  "连牌/顺子用",   12, 3, CardDest.TO_PLAY_DECK),

		# TO_HIDDEN
		_refresh(ItemType.CARD_5,  "5点牌",  "伤害 5HP",      40, 2, CardDest.TO_HIDDEN),
		_refresh(ItemType.CARD_A,  "A点牌",  "大单压牌",      35, 2, CardDest.TO_HIDDEN),
		_refresh(ItemType.CARD_K,  "K点牌",  "伤害 20HP",     30, 2, CardDest.TO_HIDDEN),
	]


func _heal(type: ItemType, name: String, desc: String, price: int) -> ShopItem:
	var it := ShopItem.new()
	it.type = type; it.name = name; it.desc = desc
	it.price = price; it.stock = -1; it.max_stock = -1
	it.section = Section.FIXED
	return it


func _fixed(type: ItemType, name: String, desc: String, price: int, stock: int, dest: CardDest) -> ShopItem:
	var it := ShopItem.new()
	it.type = type; it.name = name; it.desc = desc
	it.price = price; it.stock = stock; it.max_stock = stock
	it.section = Section.FIXED; it.dest = dest
	return it


func _refresh(type: ItemType, name: String, desc: String, price: int, stock: int, dest: CardDest) -> ShopItem:
	var it := ShopItem.new()
	it.type = type; it.name = name; it.desc = desc
	it.price = price; it.stock = stock; it.max_stock = stock
	it.section = Section.REFRESHABLE; it.dest = dest
	return it


func _random_suit() -> String:
	return SUITS[randi() % SUITS.size()]


# ═══════════════════════════════════════════════════════════════
# 一级商店 — 刷新
# ═══════════════════════════════════════════════════════════════

func can_refresh() -> bool:
	for it in items:
		if it.section == Section.REFRESHABLE:
			return true
	return false


func refresh(chips: int) -> bool:
	if chips < REFRESH_COST:
		return false
	for it in items:
		if it.section == Section.REFRESHABLE:
			it.stock = it.max_stock
	return true


# ═══════════════════════════════════════════════════════════════
# 一级商店 — 购买
# ═══════════════════════════════════════════════════════════════

func buy(type: ItemType, current_chips: int, current_hp: int, max_hp: int) -> PurchaseResult:
	var res := PurchaseResult.new()
	var price: int = BASE_PRICE.get(type, 0)

	var item: ShopItem = null
	for it in items:
		if it.type == type:
			item = it
			break
	if item == null:
		res.message = "物品不存在！"
		return res

	if item.stock == 0:
		res.message = "已售罄！"
		return res

	if current_chips < price:
		res.message = "筹码不足！"
		return res

	match type:
		ItemType.HEAL_SMALL:
			if current_hp >= max_hp:
				res.message = "HP 已满，无需回复！"
				return res
			res.success = true
			res.hp_healed = 20
			res.message = "回复了 20 HP！"

		ItemType.HEAL_LARGE:
			if current_hp >= max_hp:
				res.message = "HP 已满，无需回复！"
				return res
			res.success = true
			res.hp_healed = 50
			res.message = "回复了 50 HP！"

		_:
			var pt: String = POINT_FOR.get(type, "")
			if pt.is_empty():
				res.message = "未知物品！"
				return res
			var suit := _random_suit()
			var id := Card.id_for(pt, suit)
			var c := Card.from_id(id)
			c.is_original_deck = false
			res.card = c
			res.dest = item.dest
			res.success = true
			res.message = "获得 %s%s！ → %s" % [suit, pt, DEST_LABEL[item.dest]]

	if res.success and item.stock > 0:
		item.stock -= 1

	if res.success:
		level_spent_chips += price

	return res


# ═══════════════════════════════════════════════════════════════
# 一级商店 — 关末 / 查询
# ═══════════════════════════════════════════════════════════════

func on_level_end() -> void:
	for it in items:
		if it.section == Section.FIXED and it.max_stock > 0:
			it.stock = it.max_stock
	reset_enchant_stock()


func can_enter_enchant_shop() -> bool:
	return level_spent_chips >= ENCHANT_UNLOCK_SPEND


func is_empty() -> bool:
	for it in items:
		if it.stock < 0:
			return false
		if it.stock > 0:
			return false
	return true


func get_items_by_section(sec: Section) -> Array[ShopItem]:
	var out: Array[ShopItem] = []
	for it in items:
		if it.section == sec:
			out.append(it)
	return out


func get_fixed_items() -> Array[ShopItem]:
	return get_items_by_section(Section.FIXED)


func get_refreshable_items() -> Array[ShopItem]:
	return get_items_by_section(Section.REFRESHABLE)


# ═══════════════════════════════════════════════════════════════
# ═══════════════════════════════════════════════════════════════
# 二级附魔商店
# ═══════════════════════════════════════════════════════════════
# ═══════════════════════════════════════════════════════════════

var enchant_items: Array[Enchant.EnchantItem] = []
var enchanted_list: Array[Enchant.EnchantedCard] = []

var enchant_pool: Array[Enchant.Enchantment] = []
var bought_ids: Array[String] = []

const SAVE_PATH := "user://enchants.save"


func _init() -> void:
	reset_stock()
	_build_enchant_pool()
	load_enchants()
	reset_enchant_stock()


func _build_enchant_pool() -> void:
	enchant_pool = _enchant_pool().duplicate(true)


func _enchant_id(e: Enchant.Enchantment) -> String:
	return "%d_%d" % [e.type, e.level]


## 关末/关始刷新：未售出退回池子，从池子抽 2 件新品
func reset_enchant_stock() -> void:
	for ei in enchant_items:
		if not ei.sold_out:
			enchant_pool.append(ei.enchant)
	enchant_items.clear()

	var available := enchant_pool.duplicate()
	available.shuffle()
	var chosen := available.slice(0, mini(ENCHANT_SLOTS, available.size()))
	for e in chosen:
		enchant_pool.erase(e)
		var ei := Enchant.EnchantItem.new()
		ei.enchant = e
		enchant_items.append(ei)


func enchant_buy(index: int, chips: int, target_card: Card) -> Enchant.EnchantPurchaseResult:
	var res := Enchant.EnchantPurchaseResult.new()

	if index < 0 or index >= enchant_items.size():
		res.message = "物品不存在！"
		return res

	var ei := enchant_items[index]
	if ei.sold_out:
		res.message = "已售罄！"
		return res

	if target_card == null:
		res.message = "请选择要附魔的卡牌！"
		return res

	if not target_card.is_joker():
		res.message = "只有小丑牌（大鬼/小鬼）可以附魔！"
		return res

	if chips < ei.enchant.cost_chips:
		res.message = "筹码不足！（需要 %d）" % ei.enchant.cost_chips
		return res

	for ec in enchanted_list:
		if ec.card.suit == target_card.suit and ec.card.point == target_card.point:
			if ec.has_enchant(ei.enchant.type):
				res.message = "该卡牌已拥有此附魔类型！"
				return res

	var found := false
	for existing in enchanted_list:
		if existing.card.suit == target_card.suit and existing.card.point == target_card.point:
			existing.enchants.append(ei.enchant)
			found = true
			break
	if not found:
		var ec := Enchant.EnchantedCard.new()
		ec.card = target_card
		ec.enchants.append(ei.enchant)
		enchanted_list.append(ec)

	ei.sold_out = true
	bought_ids.append(_enchant_id(ei.enchant))
	res.success = true
	res.enchant = ei.enchant
	res.message = "附魔成功！%s → %s" % [target_card.suit + target_card.point, ei.display_name()]
	save_enchants()
	return res


# ═══════════════════════════════════════════════════════════════
# 二级商店 — 存档 / 读档
# ═══════════════════════════════════════════════════════════════

func save_enchants() -> void:
	var data := []
	for ec in enchanted_list:
		var ench_arr := []
		for e in ec.enchants:
			ench_arr.append({
				"type": e.type,
				"level": e.level,
			})
		data.append({
			"suit": ec.card.suit,
			"point": ec.card.point,
			"enchants": ench_arr,
		})

	var save_data := {
		"enchanted": data,
		"bought_ids": bought_ids.duplicate(),
	}

	var file := FileAccess.open(SAVE_PATH, FileAccess.WRITE)
	if file:
		file.store_string(JSON.stringify(save_data))
		file.close()


func load_enchants() -> void:
	if not FileAccess.file_exists(SAVE_PATH):
		return

	var file := FileAccess.open(SAVE_PATH, FileAccess.READ)
	if not file:
		return

	var text := file.get_as_text()
	file.close()

	var json := JSON.new()
	var err := json.parse(text)
	if err != OK:
		return

	var save_data = json.get_data()
	if save_data == null:
		return

	bought_ids.clear()
	enchanted_list.clear()

	if save_data.has("bought_ids"):
		for id_str in save_data["bought_ids"]:
			bought_ids.append(id_str)
		_rebuild_pool_from_bought()

	if save_data.has("enchanted"):
		for entry in save_data["enchanted"]:
			var id := Card.id_for(entry["point"], entry.get("suit", ""))
			var card: Card
			if id >= 0:
				card = Card.from_id(id)
			else:
				card = Card.new()
				card.point = entry["point"]
				card.suit = entry.get("suit", "")
			var ec := Enchant.EnchantedCard.new()
			ec.card = card
			for e_data in entry["enchants"]:
				var e := Enchant.Enchantment.new()
				e.type = e_data["type"] as Enchant.EnchantType
				e.level = e_data["level"]
				e.name = _enchant_name_for(e.type)
				e.desc = _enchant_desc_for(e.type)
				e.cost_chips = _enchant_cost_for(e.type, e.level)
				ec.enchants.append(e)
			enchanted_list.append(ec)


func _rebuild_pool_from_bought() -> void:
	enchant_pool.clear()
	for e in _enchant_pool():
		if not bought_ids.has(_enchant_id(e)):
			enchant_pool.append(e)


func _enchant_name_for(t: Enchant.EnchantType) -> String:
	match t:
		Enchant.EnchantType.SCORE_UP: return "刻印·伤"
		Enchant.EnchantType.SHIELD: return "护盾"
		Enchant.EnchantType.DOUBLE: return "双倍"
		Enchant.EnchantType.LIFESTEAL: return "吸血"
		Enchant.EnchantType.PIERCE: return "穿刺"
		Enchant.EnchantType.LUCKY: return "幸运"
		Enchant.EnchantType.COMBO_DRAW: return "连击"
		Enchant.EnchantType.RANK_UP: return "升阶"
		Enchant.EnchantType.TAUNT: return "嘲讽"
	return ""


func _enchant_desc_for(t: Enchant.EnchantType) -> String:
	match t:
		Enchant.EnchantType.SCORE_UP: return "额外 +N 点伤害"
		Enchant.EnchantType.SHIELD: return "不能被对手的压迫伤害抢走"
		Enchant.EnchantType.DOUBLE: return "该牌伤害 ×2"
		Enchant.EnchantType.LIFESTEAL: return "造成伤害时回复等量 HP"
		Enchant.EnchantType.PIERCE: return "无视对手护盾"
		Enchant.EnchantType.LUCKY: return "50% 概率额外 +N 伤害"
		Enchant.EnchantType.COMBO_DRAW: return "该牌赢下回合时补 N 张牌"
		Enchant.EnchantType.RANK_UP: return "出牌比较时 rank+N"
		Enchant.EnchantType.TAUNT: return "迫使对手优先压此牌组合"
	return ""


func _enchant_cost_for(t: Enchant.EnchantType, lv: int) -> int:
	match t:
		Enchant.EnchantType.SHIELD:     return [0, 30, 70][lv]
		Enchant.EnchantType.LIFESTEAL:  return [0, 40, 90, 160][lv]
		Enchant.EnchantType.DOUBLE:     return [0, 80, 180][lv]
		Enchant.EnchantType.PIERCE:     return [0, 35, 75][lv]
		Enchant.EnchantType.RANK_UP:    return [0, 35, 80, 150][lv]
		Enchant.EnchantType.COMBO_DRAW: return [0, 45, 95, 170][lv]
		Enchant.EnchantType.TAUNT:      return [0, 25, 55][lv]
		Enchant.EnchantType.SCORE_UP:   return [0, 20, 50, 100][lv]
		Enchant.EnchantType.LUCKY:      return [0, 25, 60, 110][lv]
	return 0


# ═══════════════════════════════════════════════════════════════
# 二级商店 — 附魔池
# ═══════════════════════════════════════════════════════════════

func _enchant_pool() -> Array[Enchant.Enchantment]:
	var pool: Array[Enchant.Enchantment] = []
	var e: Enchant.Enchantment

	e = Enchant.Enchantment.new()
	e.type = Enchant.EnchantType.SHIELD; e.name = "护盾"; e.desc = "不能被对手的压迫伤害抢走"
	e.level = 1; e.cost_chips = 30
	pool.append(e)
	e = Enchant.Enchantment.new()
	e.type = Enchant.EnchantType.SHIELD; e.name = "护盾"; e.desc = "不能被对手的压迫伤害抢走"
	e.level = 2; e.cost_chips = 70
	pool.append(e)

	e = Enchant.Enchantment.new()
	e.type = Enchant.EnchantType.LIFESTEAL; e.name = "吸血"; e.desc = "造成伤害时回复等量 HP"
	e.level = 1; e.cost_chips = 40
	pool.append(e)
	e = Enchant.Enchantment.new()
	e.type = Enchant.EnchantType.LIFESTEAL; e.name = "吸血"; e.desc = "造成伤害时回复等量 HP"
	e.level = 2; e.cost_chips = 90
	pool.append(e)
	e = Enchant.Enchantment.new()
	e.type = Enchant.EnchantType.LIFESTEAL; e.name = "吸血"; e.desc = "造成伤害时回复等量 HP"
	e.level = 3; e.cost_chips = 160
	pool.append(e)

	e = Enchant.Enchantment.new()
	e.type = Enchant.EnchantType.DOUBLE; e.name = "双倍"; e.desc = "该牌伤害 ×2"
	e.level = 1; e.cost_chips = 80
	pool.append(e)
	e = Enchant.Enchantment.new()
	e.type = Enchant.EnchantType.DOUBLE; e.name = "双倍"; e.desc = "该牌伤害 ×2"
	e.level = 2; e.cost_chips = 180
	pool.append(e)

	e = Enchant.Enchantment.new()
	e.type = Enchant.EnchantType.PIERCE; e.name = "穿刺"; e.desc = "无视对手护盾"
	e.level = 1; e.cost_chips = 35
	pool.append(e)
	e = Enchant.Enchantment.new()
	e.type = Enchant.EnchantType.PIERCE; e.name = "穿刺"; e.desc = "无视对手护盾"
	e.level = 2; e.cost_chips = 75
	pool.append(e)

	e = Enchant.Enchantment.new()
	e.type = Enchant.EnchantType.RANK_UP; e.name = "升阶"; e.desc = "出牌比较时 rank+N"
	e.level = 1; e.cost_chips = 35
	pool.append(e)
	e = Enchant.Enchantment.new()
	e.type = Enchant.EnchantType.RANK_UP; e.name = "升阶"; e.desc = "出牌比较时 rank+N"
	e.level = 2; e.cost_chips = 80
	pool.append(e)
	e = Enchant.Enchantment.new()
	e.type = Enchant.EnchantType.RANK_UP; e.name = "升阶"; e.desc = "出牌比较时 rank+N"
	e.level = 3; e.cost_chips = 150
	pool.append(e)

	e = Enchant.Enchantment.new()
	e.type = Enchant.EnchantType.COMBO_DRAW; e.name = "连击"; e.desc = "该牌赢下回合时补 N 张牌"
	e.level = 1; e.cost_chips = 45
	pool.append(e)
	e = Enchant.Enchantment.new()
	e.type = Enchant.EnchantType.COMBO_DRAW; e.name = "连击"; e.desc = "该牌赢下回合时补 N 张牌"
	e.level = 2; e.cost_chips = 95
	pool.append(e)
	e = Enchant.Enchantment.new()
	e.type = Enchant.EnchantType.COMBO_DRAW; e.name = "连击"; e.desc = "该牌赢下回合时补 N 张牌"
	e.level = 3; e.cost_chips = 170
	pool.append(e)

	e = Enchant.Enchantment.new()
	e.type = Enchant.EnchantType.TAUNT; e.name = "嘲讽"; e.desc = "迫使对手优先压此牌组合"
	e.level = 1; e.cost_chips = 25
	pool.append(e)
	e = Enchant.Enchantment.new()
	e.type = Enchant.EnchantType.TAUNT; e.name = "嘲讽"; e.desc = "迫使对手优先压此牌组合"
	e.level = 2; e.cost_chips = 55
	pool.append(e)

	e = Enchant.Enchantment.new()
	e.type = Enchant.EnchantType.SCORE_UP; e.name = "刻印·伤"; e.desc = "额外 +N 点伤害"
	e.level = 1; e.cost_chips = 20
	pool.append(e)
	e = Enchant.Enchantment.new()
	e.type = Enchant.EnchantType.SCORE_UP; e.name = "刻印·伤"; e.desc = "额外 +N 点伤害"
	e.level = 2; e.cost_chips = 50
	pool.append(e)
	e = Enchant.Enchantment.new()
	e.type = Enchant.EnchantType.SCORE_UP; e.name = "刻印·伤"; e.desc = "额外 +N 点伤害"
	e.level = 3; e.cost_chips = 100
	pool.append(e)

	e = Enchant.Enchantment.new()
	e.type = Enchant.EnchantType.LUCKY; e.name = "幸运"; e.desc = "50% 概率额外 +N 伤害"
	e.level = 1; e.cost_chips = 25
	pool.append(e)
	e = Enchant.Enchantment.new()
	e.type = Enchant.EnchantType.LUCKY; e.name = "幸运"; e.desc = "50% 概率额外 +N 伤害"
	e.level = 2; e.cost_chips = 60
	pool.append(e)
	e = Enchant.Enchantment.new()
	e.type = Enchant.EnchantType.LUCKY; e.name = "幸运"; e.desc = "50% 概率额外 +N 伤害"
	e.level = 3; e.cost_chips = 110
	pool.append(e)

	return pool