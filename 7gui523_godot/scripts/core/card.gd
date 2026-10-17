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

## 点数，如 "3" "10" "J" "大鬼" "小鬼"
var point: String

## 花色，如 "黑桃" "红桃" "梅花" "方块"；鬼牌为空字符串
var suit: String

## 分值：5→5, 10→10, K→20, 其他→0
var score: int

var is_original_deck: bool = true


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

static var ALL_54: Dictionary = {}
static var _deck_ready := false

static func _ensure_deck() -> void:
	if _deck_ready:
		return
	_deck_ready = true

	var suits := ["黑桃", "红桃", "梅花", "方块"]
	var ranks := ["A","2","3","4","5","6","7","8","9","10","J","Q","K"]
	var idx := 0

	for suit in suits:
		for rank in ranks:
			var c := Card.new()
			c.suit = suit
			c.point = rank
			c.score = Card.score_from_point(rank)
			ALL_54[idx] = c
			idx += 1

	# 小鬼
	var c52 := Card.new()
	c52.point = "小鬼"
	ALL_54[52] = c52

	# 大鬼
	var c53 := Card.new()
	c53.point = "大鬼"
	ALL_54[53] = c53


## 按 id 返回一个全新副本（可安全修改 seq/score/suit 等）
static func from_id(id: int) -> Card:
	_ensure_deck()
	var src: Card = ALL_54.get(id, null)
	if not src:
		push_error("Card.from_id: 无效 id=%d" % id)
		return null
	var c := Card.new()
	c.point = src.point
	c.suit = src.suit
	c.score = src.score
	return c


## 生成整副 54 张牌的副本列表
static func full_deck_copy() -> Array[Card]:
	_ensure_deck()
	var arr: Array[Card] = []
	for i in 54:
		arr.append(from_id(i))
	return arr


## 反向查 id（花色+点数 → id），鬼牌只靠点数查
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