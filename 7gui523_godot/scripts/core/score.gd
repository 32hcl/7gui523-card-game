class_name Score
extends RefCounted

# ═══════════════════════════════════════════════════════════════
# Score — 牌面基础筹码
#
# 数值统一后：分 = 伤害 = HP = 筹码，全部 1:1。
# 5 / 10 / K 的牌面基础筹码仍为 5 / 10 / 20；
# 压牌额外筹码由 CardDef.PRESSURE_CHIPS / CardInstance.pressure_chips 提供。
# ═══════════════════════════════════════════════════════════════


## 计算一手牌的基础筹码（只累加 card.score）
static func calculate_score(cards: Array[Card]) -> int:
	var total := 0
	for card in cards:
		total += card.score
	return total
