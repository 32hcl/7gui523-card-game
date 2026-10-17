class_name EnchantEffect
extends RefCounted

# ═══════════════════════════════════════════════════════════════
# EnchantEffect — 附魔战斗结算（从 Shop 迁出的纯函数层）
#
# 目前尚未接入战斗流程（round/score），保留为待接线的结算入口：
# 伤害加成/吸血/护盾/穿刺/升阶/连击补牌。
# ═══════════════════════════════════════════════════════════════


static func apply_enchant_damage(card: Card, base_damage: int, enchanted_list: Array[Enchant.EnchantedCard]) -> int:
	var dmg := base_damage

	for ec in enchanted_list:
		if ec.card != card:
			continue
		for e in ec.enchants:
			match e.type:
				Enchant.EnchantType.SCORE_UP:
					dmg += e.effect_value()
				Enchant.EnchantType.DOUBLE:
					dmg *= 2
				Enchant.EnchantType.LUCKY:
					if randf() < 0.5:
						dmg += e.effect_value()

	return dmg


static func apply_lifesteal(card: Card, enchanted_list: Array[Enchant.EnchantedCard]) -> int:
	for ec in enchanted_list:
		if ec.card == card:
			return ec.get_effect(Enchant.EnchantType.LIFESTEAL)
	return 0


static func has_shield(card: Card, enchanted_list: Array[Enchant.EnchantedCard]) -> bool:
	for ec in enchanted_list:
		if ec.card == card and ec.has_enchant(Enchant.EnchantType.SHIELD):
			return true
	return false


static func has_pierce(cards: Array[Card], enchanted_list: Array[Enchant.EnchantedCard]) -> bool:
	for c in cards:
		for ec in enchanted_list:
			if ec.card == c and ec.has_enchant(Enchant.EnchantType.PIERCE):
				return true
	return false


static func get_rank_bonus(card: Card, enchanted_list: Array[Enchant.EnchantedCard]) -> int:
	for ec in enchanted_list:
		if ec.card == card:
			return ec.get_effect(Enchant.EnchantType.RANK_UP)
	return 0


static func get_combo_draw(cards: Array[Card], enchanted_list: Array[Enchant.EnchantedCard]) -> int:
	var total := 0
	for c in cards:
		for ec in enchanted_list:
			if ec.card == c:
				total += ec.get_effect(Enchant.EnchantType.COMBO_DRAW)
	return total
