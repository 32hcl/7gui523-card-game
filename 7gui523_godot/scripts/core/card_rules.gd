class_name CardRules
extends RefCounted

## 牌型判定策略类 — 所有"这副牌是什么"的谓词/rules 独立于此
## 新增牌型时只需在此添加判定函数 + CardType 枚举新增条目，无需改动
## parse_card_type / can_beat / calculate_pressure_chips 等主逻辑

## 鬼牌判断（委托 Card.is_joker）
static func is_joker(c: Card) -> bool:
	return c.is_joker()


## 频率统计：返回 {点数: 张数}
static func get_rank_count(cards: Array[Card]) -> Dictionary:
	var count := {}
	for c in cards:
		count[c.point] = count.get(c.point, 0) + 1
	return count


## 是否全同点（不含鬼牌排除，由调用方决定）
static func is_same_point(cards: Array[Card]) -> bool:
	if cards.is_empty():
		return false
	var first := cards[0].point
	for c in cards:
		if c.point != first:
			return false
	return true


## 是否王炸：恰好2张 = 大鬼 + 小鬼
static func is_rocket(cards: Array[Card]) -> bool:
	if cards.size() != 2:
		return false
	var has_big := false
	var has_small := false
	for c in cards:
		if c.point == "大鬼":
			has_big = true
		elif c.point == "小鬼":
			has_small = true
	return has_big and has_small


## 是否 7gui523：5张 = 7 + 鬼(大/小) + 5 + 2 + 3
static func is_gui523(cards: Array[Card]) -> bool:
	if cards.size() != 5:
		return false
	var has_joker := false
	var has_7 := false
	var has_5 := false
	var has_2 := false
	var has_3 := false
	for c in cards:
		if c.point == "大鬼" or c.point == "小鬼":
			has_joker = true
		elif c.point == "7":
			has_7 = true
		elif c.point == "5":
			has_5 = true
		elif c.point == "2":
			has_2 = true
		elif c.point == "3":
			has_3 = true
		else:
			return false
	return has_joker and has_7 and has_5 and has_2 and has_3


## 是否有三张（排除鬼牌），返回点数
static func has_triple(cards: Array[Card]) -> String:
	var count_map := get_rank_count(cards)
	for point in count_map:
		if point == "大鬼" or point == "小鬼":
			continue
		if count_map[point] >= 3:
			return point
	return ""


## 是否有对子（排除鬼牌和 exclude_point），返回点数
static func has_pair(cards: Array[Card], exclude_point: String = "") -> String:
	var count_map := get_rank_count(cards)
	for point in count_map:
		if point == "大鬼" or point == "小鬼":
			continue
		if point == exclude_point:
			continue
		if count_map[point] >= 2:
			return point
	return ""


## 是否特殊牌型（炸弹/王炸/五同点/7gui523）
## 特殊牌型允许跨牌型压制普通牌型
static func is_special(t: int) -> bool:
	return t == CardTypeResult.CardType.BOMB or t == CardTypeResult.CardType.ROCKET \
		or t == CardTypeResult.CardType.FIVE_OF_A_KIND or t == CardTypeResult.CardType.GUI523