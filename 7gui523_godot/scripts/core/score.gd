class_name Score
extends RefCounted

# ═══════════════════════════════════════════════════════════════
# Score — 计分模块
# ═══════════════════════════════════════════════════════════════


## 计算一手牌的原始分数（只累加 card.score）
static func calculate_score(cards: Array[Card]) -> int:
	var total := 0
	for card in cards:
		total += card.score
	return total


## 计算桌上总分 = 原始分 + 奖励分
static func calculate_table_score(cards: Array[Card], bonus: int) -> int:
	return calculate_score(cards) + bonus


## 从结算牌中挑出分牌，返回总分和分牌列表
static func settle_score_cards(cards: Array[Card]) -> Dictionary:
	var total := 0
	var collected: Array[Card] = []

	for card in cards:
		if card.score > 0:
			total += card.score
			collected.append(card)

	return {"total": total, "collected": collected}