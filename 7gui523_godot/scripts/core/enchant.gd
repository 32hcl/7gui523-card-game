class_name Enchant
extends RefCounted

# ═══════════════════════════════════════════════════════════════
# Enchant — 附魔数据模型（从 Shop 迁出的纯数据层）
#
# Shop 负责商品/库存/存档/价格；本文件只定义附魔的类型与数据载体，
# 供 Shop 与 UI 共享，不再反向依赖商店。
# ═══════════════════════════════════════════════════════════════


enum EnchantType {
	SCORE_UP,
	SHIELD,
	DOUBLE,
	LIFESTEAL,
	PIERCE,
	LUCKY,
	COMBO_DRAW,
	RANK_UP,
	TAUNT,
}


class Enchantment:
	var type: EnchantType
	var name: String
	var desc: String
	var level: int = 1
	var cost_chips: int = 0


	const LEVEL_TABLE := {
		EnchantType.SCORE_UP:   [0, 3, 5, 8],
		EnchantType.LIFESTEAL:  [0, 2, 4, 6],
		EnchantType.LUCKY:      [0, 4, 7, 10],
		EnchantType.RANK_UP:    [0, 1, 2, 3],
		EnchantType.COMBO_DRAW: [0, 1, 1, 2],
	}


	func effect_value() -> int:
		if LEVEL_TABLE.has(type):
			return LEVEL_TABLE[type][level]
		return level


class EnchantItem:
	var enchant: Enchantment
	var sold_out := false

	func display_name() -> String:
		return enchant.name + " Lv.%d" % enchant.level

	func display_price() -> String:
		return "💰%d 筹码" % enchant.cost_chips


class EnchantPurchaseResult:
	var success := false
	var message: String = ""
	var enchant: Enchantment = null


class EnchantedCard:
	var card: Card
	var enchants: Array[Enchantment] = []

	func has_enchant(t: EnchantType) -> bool:
		for e in enchants:
			if e.type == t:
				return true
		return false

	func get_effect(t: EnchantType) -> int:
		var total := 0
		for e in enchants:
			if e.type == t:
				total += e.effect_value()
		return total