class_name Card
extends RefCounted

## 牌点大小映射（值越大牌越大）
const RANK_MAP := {
	"7": 15,
	"大鬼": 14,
	"小鬼": 13,
	"5": 12,
	"2": 11,
	"3": 10,
	"A": 9,
	"K": 8,
	"Q": 7,
	"J": 6,
	"10": 5,
	"9": 4,
	"8": 3,
	"6": 2,
	"4": 1,
}



# ═══════════════════════════════════════════════════════════════
# Card 属性
# ═══════════════════════════════════════════════════════════════

## 点数，如 "3" "10" "J" "大鬼" "小鬼" — 可通过 revert() 从 ALL_54 复原
var point: String

## 花色，如 "黑桃" "红桃" "梅花" "方块"；鬼牌为空字符串 — 可通过 revert() 复原
var suit: String

## 分值：5→5, 10→10, K→20, 大小鬼→0, 其他→0 — 可通过 revert() 复原
var score: int

## 不可变身份锚：0=黑桃A, …, 51=方块K, 52=小鬼, 53=大鬼, -1=非标准牌
## 一旦 from_id() 写入，此字段仅可读，不应再被任何代码修改
var id: int = -1

## 副本标记：0=牌库初始牌，1+=商店等途径产生的额外复制品
## 同一 id 的牌通过 copy_mark 区分实例，不可变
var copy_mark: int = 0


# ═══════════════════════════════════════════════════════════════
# 静态工具方法
# ═══════════════════════════════════════════════════════════════

## 根据点数返回分值
static func score_from_point(p: String) -> int:
	match p:
		"5":
			return 5
		"10":
			return 10
		"K":
			return 20
		_:
			return 0


## 查牌点大小排名
static func get_rank(p: String) -> int:
	if p == "王炸":
		return 100
	if p == "7gui523":
		return 999
	return RANK_MAP.get(p, 0)


# ═══════════════════════════════════════════════════════════════
# 手牌排序

## 排序比较函数（静态，只创建一次，避免每帧分配 Callable）
## 参数顺序即 sort_custom 的调用顺序；hand 由 bind 追加在末尾
static func _hand_compare(a_idx: int, b_idx: int, hand: Array) -> bool:
	var ra := get_rank(hand[a_idx].point)
	var rb := get_rank(hand[b_idx].point)
	if ra != rb:
		return ra > rb
	return hand[a_idx].suit < hand[b_idx].suit

## 手牌排序（rank 降序 → 花色 Unicode 升序）
## 返回索引数组，order[0] = 应排最左的元素在 hand 中的原始下标
static func sorted_order(hand: Array) -> Array:
	var order := range(hand.size())
	order.sort_custom(_hand_compare.bind(hand))
	return order


# ═══════════════════════════════════════════════════════════════
# 实例方法
# ═══════════════════════════════════════════════════════════════

## 判断是否为鬼牌
func is_joker() -> bool:
	return point == "大鬼" or point == "小鬼"


func _to_string() -> String:
	if suit.is_empty():
		return point
	return suit + point


# ═══════════════════════════════════════════════════════════════
# 主牌库 — 54 张标准牌（id 0-53）
#
# 花色区段: 黑桃 0-12 / 红桃 13-25 / 梅花 26-38 / 方块 39-51
# 鬼牌:  小鬼 52 / 大鬼 53
#
# 用法:
#   var c = Card.from_id(0)      # 黑桃A（新副本，可安全修改）
#   var ref = Card.ALL_54[0]     # 只读引用
# ═══════════════════════════════════════════════════════════════

static var ALL_54: Array[Card] = []   # 54张模板牌，下标即 id
static var _deck_ready := false

static func _ensure_deck() -> void:
	if _deck_ready:
		return
	_deck_ready = true

	var suits := ["黑桃", "红桃", "梅花", "方块"]
	var ranks := ["A","2","3","4","5","6","7","8","9","10","J","Q","K"]

	for suit in suits:
		for rank in ranks:
			var c := Card.new()
			c.id = ALL_54.size()
			c.suit = suit
			c.point = rank
			c.score = Card.score_from_point(rank)
			ALL_54.append(c)

	# 小鬼
	var c52 := Card.new()
	c52.id = 52
	c52.point = "小鬼"
	ALL_54.append(c52)

	# 大鬼
	var c53 := Card.new()
	c53.id = 53
	c53.point = "大鬼"
	ALL_54.append(c53)


## 按 id 返回一个全新副本（可安全修改 seq/score/suit 等）
static func from_id(id: int) -> Card:
	_ensure_deck()
	if id < 0 or id >= ALL_54.size():
		push_error("Card.from_id: 无效 id=%d" % id)
		return null
	var src: Card = ALL_54[id]
	var c := Card.new()
	c.id = id
	c.point = src.point
	c.suit = src.suit
	c.score = src.score
	return c


## 将 point/suit/score 复位到 ALL_54 模板中的默认值
## id < 0 时（非标准牌）无操作
func revert() -> void:
	if id < 0 or id >= Card.ALL_54.size():
		return
	var src: Card = Card.ALL_54[id]
	point = src.point
	suit = src.suit
	score = src.score


## 反向查 id：如今 Card 自带 id，优先直接返回；作为静态方法回退仍扫描 ALL_54
static func id_for(p: String, s: String = "") -> int:
	_ensure_deck()
	if p == "大鬼":
		return 53
	if p == "小鬼":
		return 52
	for id in 52:
		var c: Card = ALL_54[id]
		if c.point == p and c.suit == s:
			return id
	return -1