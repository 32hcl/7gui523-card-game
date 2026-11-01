class_name Card
extends CardInstance

## Card 是对外统一的卡牌类，内部数据委托给
## CardDef（不可变静态定义）+ CardInstance（运行时实例）。

## 牌点大小映射：委托到 CardDef.RANK_MAP
const RANK_MAP := CardDef.RANK_MAP


# ═══════════════════════════════════════════════════════════════
# 静态工具方法（转发到 CardDef）
# ═══════════════════════════════════════════════════════════════

## 根据点数返回分值
static func score_from_point(p: String) -> int:
	return CardDef.score_from_point(p)


## 查牌点大小排名
static func get_rank(p: String) -> int:
	return CardDef.get_rank(p)


# ═══════════════════════════════════════════════════════════════
# 手牌排序
# ═══════════════════════════════════════════════════════════════

## 排序比较函数（静态，只创建一次，避免每帧分配 Callable）
## 参数顺序即 sort_custom 的调用顺序；hand 由 bind 追加在末尾
static func _hand_compare(a_idx: int, b_idx: int, hand: Array[Card]) -> bool:
	var ra := CardDef.get_rank(hand[a_idx].point)
	var rb := CardDef.get_rank(hand[b_idx].point)
	if ra != rb:
		return ra > rb
	return hand[a_idx].suit < hand[b_idx].suit


## 手牌排序（rank 降序 → 花色 Unicode 升序）
## 返回索引数组，order[0] = 应排最左的元素在 hand 中的下标
static func sorted_order(hand: Array[Card]) -> Array[int]:
	var order: Array[int] = []
	for i in hand.size():
		order.append(i)
	order.sort_custom(_hand_compare.bind(hand))
	return order


# ═══════════════════════════════════════════════════════════════
# 实例工厂
# ═══════════════════════════════════════════════════════════════

## 按 id 返回一个全新的 Card 实例（安全修改临时点/色/分）
static func from_id(id: int) -> Card:
	var d := CardDef.from_id(id)
	if d == null:
		return null
	var c := Card.new()
	c.def_id = d.id
	return c


## 反向查 id：按点数 + 花色定位标准牌；找不到返回 -1
static func id_for(p: String, s: String = "") -> int:
	return CardDef.id_for(p, s)