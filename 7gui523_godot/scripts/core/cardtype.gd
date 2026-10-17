class_name CardTypeResult
extends RefCounted

## 牌型枚举
enum CardType {
	PASS,
	SINGLE,
	PAIR,
	TRIPLE,
	TRIPLE_WITH_ONE,
	TRIPLE_WITH_TWO,
	BOMB,
	ROCKET,
	FIVE_OF_A_KIND,
	GUI523,
	INVALID,
}


# ═══════════════════════════════════════════════════════════════
# CardTypeResult — 牌型识别结果
# ═══════════════════════════════════════════════════════════════

## 牌型等级（值越大越强，用于 can_beat 第一层比较）
const TYPE_RANK := {
	CardType.PASS:            -1,
	CardType.INVALID:         -2,
	CardType.SINGLE:          0,
	CardType.PAIR:            1,
	CardType.TRIPLE:          2,
	CardType.TRIPLE_WITH_ONE: 3,
	CardType.TRIPLE_WITH_TWO: 4,
	CardType.BOMB:            5,
	CardType.ROCKET:          6,
	CardType.FIVE_OF_A_KIND:  7,
	CardType.GUI523:          999,
}

var type: CardType = CardType.INVALID
var key_point: String = ""       # 主比较点数
var kicker: String = ""          # 三带一/三带二的带的牌点数
var cards: Array[Card] = []      # 原始卡牌
var bonus_score: int = 0         # 压迫加分
var count: int = 0               # 同点张数（炸弹4、五同点5+）


# ═══════════════════════════════════════════════════════════════
# 底层谓词 — 每个函数独立判断一个条件
# ═══════════════════════════════════════════════════════════════

## 鬼牌判断（委托 Card.is_joker）
static func is_joker(c: Card) -> bool:
	return c.is_joker()


## 是否特殊牌型（炸弹/王炸/五同点/7gui523）→ 允许跨牌型压制
static func _is_special(t: CardType) -> bool:
	return t == CardType.BOMB or t == CardType.ROCKET \
		or t == CardType.FIVE_OF_A_KIND or t == CardType.GUI523


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


# ═══════════════════════════════════════════════════════════════
# 牌型解析器
# ═══════════════════════════════════════════════════════════════

## 主解析：按张数分支判断牌型
static func parse_card_type(cards: Array[Card], ban_gui523: bool = false) -> CardTypeResult:
	var result := CardTypeResult.new()
	result.cards = cards
	var size := cards.size()

	if size == 1:
		result.type = CardType.SINGLE
		result.key_point = cards[0].point

	elif size == 2:
		if is_rocket(cards):
			result.type = CardType.ROCKET
			result.key_point = "王炸"
		elif is_same_point(cards) and not is_joker(cards[0]):
			result.type = CardType.PAIR
			result.key_point = cards[0].point

	elif size == 3:
		if is_same_point(cards) and not is_joker(cards[0]):
			result.type = CardType.TRIPLE
			result.key_point = cards[0].point

	elif size == 4:
		if is_same_point(cards) and not is_joker(cards[0]):
			result.type = CardType.BOMB
			result.key_point = cards[0].point
			result.count = 4
		else:
			var triple_point := has_triple(cards)
			if not triple_point.is_empty():
				result.type = CardType.TRIPLE_WITH_ONE
				result.key_point = triple_point
				for c in cards:
					if c.point != triple_point:
						result.kicker = c.point
						break

	elif size >= 5:
		if is_gui523(cards):
			result.type = CardType.GUI523
			result.key_point = "7gui523"
			if ban_gui523:
				result.type = CardType.INVALID
				result.key_point = ""
			return result

		if size == 5:
			var triple_point := has_triple(cards)
			if not triple_point.is_empty():
				var pair_point := has_pair(cards, triple_point)
				if not pair_point.is_empty():
					result.type = CardType.TRIPLE_WITH_TWO
					result.key_point = triple_point
					result.kicker = pair_point

	return result


## 五同点解析：≥5张全同点
static func parse_five_of_a_kind(cards: Array[Card]) -> CardTypeResult:
	var result := CardTypeResult.new()
	result.cards = cards
	var size := cards.size()
	if size >= 5 and is_same_point(cards) and not is_joker(cards[0]):
		result.type = CardType.FIVE_OF_A_KIND
		result.key_point = cards[0].point
		result.count = size
	return result


## 统一解析入口：先标准解析，失败回退五同点
static func parse_any_card_type(cards: Array[Card], ban_gui523: bool = false) -> CardTypeResult:
	var result := parse_card_type(cards, ban_gui523)
	if result.type == CardType.INVALID:
		result = parse_five_of_a_kind(cards)
	return result


# ═══════════════════════════════════════════════════════════════
# 牌型比较
# ═══════════════════════════════════════════════════════════════

## A 能否压过 B（candidate 打 previous）
## 两阶段比较：先比牌型等级，同牌型再比点数+kicker
static func can_beat(candidate: CardTypeResult, previous: CardTypeResult) -> bool:
	if previous.cards.is_empty() or previous.type == CardType.PASS:
		return candidate.type != CardType.INVALID
	if candidate.type == CardType.INVALID:
		return false

	var rank_a: int = TYPE_RANK.get(candidate.type, -99)
	var rank_b: int = TYPE_RANK.get(previous.type, -99)

	# 特殊牌型（炸弹/王炸/五同点/7gui523）允许跨牌型压制；其余普通牌型必须同牌型
	var special_a: bool = _is_special(candidate.type)
	var special_b: bool = _is_special(previous.type)

	# 第一阶段：普通牌型必须同牌型才能压过；特殊牌型可跨型压制
	if rank_a != rank_b:
		# 普通 vs 普通 且 牌型不同 → 压不过（对子不能压单张、三张不能压对子…）
		if not special_a and not special_b:
			return false
		# 特殊 vs 普通 → 特殊压普通
		if special_a and not special_b:
			return true
		# 普通 vs 特殊 → 普通压不过特殊
		if not special_a and special_b:
			return false
		# 特殊 vs 特殊 → 等级高者胜
		if special_a and special_b:
			return rank_a > rank_b

	# 第二阶段：同牌型 → 比 key_point
	var cmp := Card.get_rank(candidate.key_point) - Card.get_rank(previous.key_point)
	if cmp > 0:
		return true
	if cmp < 0:
		return false

	# key_point 也相同 → 三带一/三带二加比 kicker
	if candidate.type == CardType.TRIPLE_WITH_ONE or candidate.type == CardType.TRIPLE_WITH_TWO:
		if not candidate.kicker.is_empty() and not previous.kicker.is_empty():
			return Card.get_rank(candidate.kicker) >= Card.get_rank(previous.kicker)

	# 五同点/炸弹 key_point 相同 → 比张数
	if candidate.type == CardType.FIVE_OF_A_KIND or candidate.type == CardType.BOMB:
		return candidate.count >= previous.count

	return true


## 压迫加分：同牌型、同 key_point、上家全是分牌、当前也有分 → 累加
static func calculate_pressure_bonus(candidate: CardTypeResult, previous: CardTypeResult) -> int:
	if previous == null or previous.cards.is_empty() or previous.type == CardType.PASS:
		return 0
	if candidate.type != previous.type:
		return 0
	if candidate.key_point != previous.key_point:
		return 0

	# 上家必须全是分牌
	for c in previous.cards:
		if c.score == 0:
			return 0

	# 当前出牌的分牌累加
	var curr_score := 0
	for c in candidate.cards:
		if c.score > 0:
			curr_score += c.score
	if curr_score == 0:
		return 0

	return curr_score


# ═══════════════════════════════════════════════════════════════
# 调试输出
# ═══════════════════════════════════════════════════════════════

func _to_string() -> String:
	var s := CardTypeNames.name_for(type)
	if not key_point.is_empty():
		s += " " + key_point
	s += " ("
	for i in range(cards.size()):
		s += cards[i]._to_string()
		if i < cards.size() - 1:
			s += " "
	s += ")"
	return s