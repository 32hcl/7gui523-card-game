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
var cards: Array[Card] = []      # 卡牌数组
var pressure_chips: int = 0      # 压牌额外筹码
var count: int = 0               # 同点张数（炸弹4、五同点5+）

const Rules = preload("res://scripts/core/card_rules.gd")


# ═══════════════════════════════════════════════════════════════
# 底层谓词 → 全部委托给 CardRules 策略类，方便独立新增牌型判定
# ═══════════════════════════════════════════════════════════════

static func is_joker(c: Card) -> bool:
	return Rules.is_joker(c)

static func _is_special(t: CardType) -> bool:
	return Rules.is_special(t)

static func get_rank_count(cards: Array[Card]) -> Dictionary:
	return Rules.get_rank_count(cards)

static func is_same_point(cards: Array[Card]) -> bool:
	return Rules.is_same_point(cards)

static func is_rocket(cards: Array[Card]) -> bool:
	return Rules.is_rocket(cards)

static func is_gui523(cards: Array[Card]) -> bool:
	return Rules.is_gui523(cards)

static func has_triple(cards: Array[Card]) -> String:
	return Rules.has_triple(cards)

static func has_pair(cards: Array[Card], exclude_point: String = "") -> String:
	return Rules.has_pair(cards, exclude_point)


# ═══════════════════════════════════════════════════════════════
# 牌型匹配器 — 按优先级排列，先匹配特殊牌型再到普通牌型
# 新增牌型只需：1) CardType 加枚举  2) 加一个 _match_* 函数  3) 加入此列表
# ═══════════════════════════════════════════════════════════════

## 创建带 cards 预设的 CardTypeResult
static func _new_result(cards: Array[Card]) -> CardTypeResult:
	var r := CardTypeResult.new()
	r.cards = cards
	return r


static func _match_single(cards: Array[Card]) -> CardTypeResult:
	if cards.size() != 1:
		return _new_result(cards)
	var r := _new_result(cards)
	r.type = CardType.SINGLE
	r.key_point = cards[0].point
	return r


static func _match_pair(cards: Array[Card]) -> CardTypeResult:
	if cards.size() != 2:
		return _new_result(cards)
	if is_same_point(cards) and not is_joker(cards[0]):
		var r := _new_result(cards)
		r.type = CardType.PAIR
		r.key_point = cards[0].point
		return r
	return _new_result(cards)


static func _match_triple(cards: Array[Card]) -> CardTypeResult:
	if cards.size() != 3:
		return _new_result(cards)
	if is_same_point(cards) and not is_joker(cards[0]):
		var r := _new_result(cards)
		r.type = CardType.TRIPLE
		r.key_point = cards[0].point
		return r
	return _new_result(cards)


static func _match_triple_with_one(cards: Array[Card]) -> CardTypeResult:
	if cards.size() != 4:
		return _new_result(cards)
	var triple_point := has_triple(cards)
	if not triple_point.is_empty():
		var r := _new_result(cards)
		r.type = CardType.TRIPLE_WITH_ONE
		r.key_point = triple_point
		for c in cards:
			if c.point != triple_point:
				r.kicker = c.point
				break
		return r
	return _new_result(cards)


static func _match_triple_with_two(cards: Array[Card]) -> CardTypeResult:
	if cards.size() != 5:
		return _new_result(cards)
	var triple_point := has_triple(cards)
	if not triple_point.is_empty():
		var pair_point := has_pair(cards, triple_point)
		if not pair_point.is_empty():
			var r := _new_result(cards)
			r.type = CardType.TRIPLE_WITH_TWO
			r.key_point = triple_point
			r.kicker = pair_point
			return r
	return _new_result(cards)


static func _match_bomb(cards: Array[Card]) -> CardTypeResult:
	if cards.size() != 4:
		return _new_result(cards)
	if is_same_point(cards) and not is_joker(cards[0]):
		var r := _new_result(cards)
		r.type = CardType.BOMB
		r.key_point = cards[0].point
		r.count = 4
		return r
	return _new_result(cards)


static func _match_rocket(cards: Array[Card]) -> CardTypeResult:
	if not is_rocket(cards):
		return _new_result(cards)
	var r := _new_result(cards)
	r.type = CardType.ROCKET
	r.key_point = "王炸"
	return r


static func _match_gui523(cards: Array[Card]) -> CardTypeResult:
	if not is_gui523(cards):
		return _new_result(cards)
	var r := _new_result(cards)
	r.type = CardType.GUI523
	r.key_point = "7gui523"
	return r


## 策略列表：按优先级从高到低排列（GUI523 > 王炸 > 炸弹 > 三带二 > 三带一 > 三张 > 对子 > 单张）
static var _matchers: Array[Callable] = [
	_match_gui523,
	_match_rocket,
	_match_bomb,
	_match_triple_with_two,
	_match_triple_with_one,
	_match_triple,
	_match_pair,
	_match_single,
]


# ═══════════════════════════════════════════════════════════════
# 牌型解析器
# ═══════════════════════════════════════════════════════════════

## 主解析：按策略表顺序尝试匹配，首个成功即返回
static func parse_card_type(cards: Array[Card], ban_gui523: bool = false) -> CardTypeResult:
	for matcher in _matchers:
		var result: CardTypeResult = matcher.call(cards)
		if result.type != CardType.INVALID:
			if result.type == CardType.GUI523 and ban_gui523:
				result.type = CardType.INVALID
				result.key_point = ""
			return result
	return _new_result(cards)


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


## 压牌额外筹码：
##   普通牌型 — 牌型 + 主点数都相同时，按当前每张牌的压牌值累加
##   特殊牌型（炸弹/王炸/五同点/7gui523）— 可跨牌型压制，只要压得过就按每张牌的压牌值累加
## 5/10/K 的基础筹码（card.score）不在此处，由 Score.calculate_score 单独计算。
static func calculate_pressure_chips(candidate: CardTypeResult, previous: CardTypeResult) -> int:
	if previous == null or previous.cards.is_empty() or previous.type == CardType.PASS:
		return 0
	if candidate.type == CardType.INVALID:
		return 0

	## 特殊牌型：成功压牌即可附加压牌筹码（不受牌型/主点数相同限制）
	if _is_special(candidate.type):
		if not can_beat(candidate, previous):
			return 0
		return _sum_pressure_chips(candidate.cards)

	if candidate.type != previous.type:
		return 0
	if candidate.key_point != previous.key_point:
		return 0
	return _sum_pressure_chips(candidate.cards)


## 累加一手牌每张牌的压牌值
static func _sum_pressure_chips(cards: Array[Card]) -> int:
	var total := 0
	for c in cards:
		total += c.pressure_chips
	return total


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