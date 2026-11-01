class_name CardDef
extends RefCounted

## 不可变卡牌静态定义。
## 所有 CardDef 实例只应来自 ALL_54，运行期不应修改任何字段。
## 动态变化（临时改点、附魔、副本标记）一律放在 CardInstance。

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
# CardDef 属性（只读约定）
# ═══════════════════════════════════════════════════════════════

## 不可变身份锚：0=黑桃A, …, 51=方块K, 52=小鬼, 53=大鬼, -1=非标准牌
var id: int = -1

## 点数，如 "3" "10" "J" "大鬼" "小鬼"
var point: String = ""

## 花色，如 "黑桃" "红桃" "梅花" "方块"；鬼牌为空字符串
var suit: String = ""

## 分值：5→5, 10→10, K→20, 大小鬼→0, 其他→0
var score: int = 0


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
# 压牌筹码 — 同牌型 + 同点数压牌时，每张牌额外增加的桌面筹码
#
# 鬼只定义一个键 "鬼"，大小鬼查询时统一映射到它，不分开写。
# ═══════════════════════════════════════════════════════════════

const PRESSURE_CHIPS := {
	"2": 15,
	"3": 15,
	"4": 4,
	"5": 15,
	"6": 6,
	"7": 15,
	"8": 8,
	"9": 9,
	"10": 10,
	"J": 11,
	"Q": 12,
	"K": 13,
	"A": 14,
	"鬼": 20,
}


## 按点数取压牌筹码；大小鬼统一按 "鬼" 处理
static func pressure_chips_from_point(p: String) -> int:
	if p == "大鬼" or p == "小鬼":
		return PRESSURE_CHIPS["鬼"]
	return PRESSURE_CHIPS.get(p, 0)


# ═══════════════════════════════════════════════════════════════
# 主牌库 — 54 张标准牌（id 0-53）
#
# 花色区段: 黑桃 0-12 / 红桃 13-25 / 梅花 26-38 / 方块 39-51
# 鬼牌:  小鬼 52 / 大鬼 53
# ═══════════════════════════════════════════════════════════════

static var ALL_54: Array[CardDef] = []
static var _deck_ready := false

static func _ensure_deck() -> void:
	if _deck_ready:
		return
	_deck_ready = true

	var suits := ["黑桃", "红桃", "梅花", "方块"]
	var ranks := ["A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K"]

	for suit in suits:
		for rank in ranks:
			var c := CardDef.new()
			c.id = ALL_54.size()
			c.suit = suit
			c.point = rank
			c.score = score_from_point(rank)
			ALL_54.append(c)

	# 小鬼
	var c52 := CardDef.new()
	c52.id = 52
	c52.point = "小鬼"
	ALL_54.append(c52)

	# 大鬼
	var c53 := CardDef.new()
	c53.id = 53
	c53.point = "大鬼"
	ALL_54.append(c53)


## 按 id 返回静态定义本身（只读引用，不复制）
static func from_id(id: int) -> CardDef:
	_ensure_deck()
	if id < 0 or id >= ALL_54.size():
		push_error("CardDef.from_id: 无效 id=%d" % id)
		return null
	return ALL_54[id]


## 反向查 id：按点数 + 花色定位标准牌；找不到返回 -1
static func id_for(p: String, s: String = "") -> int:
	_ensure_deck()
	if p == "大鬼":
		return 53
	if p == "小鬼":
		return 52
	for i in 52:
		var c: CardDef = ALL_54[i]
		if c.point == p and c.suit == s:
			return i
	return -1
