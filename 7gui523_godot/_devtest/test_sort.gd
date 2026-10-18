extends SceneTree

# ═══════════════════════════════════════════════════════════════
# TestSort — 手牌排序 + 布局位置测试
# 覆盖：Card.sorted_order 排序规则 / _hand_metrics 位置计算逻辑
#
# 命令行：
#   godot --headless --path . --script _devtest/test_sort.gd
# ═══════════════════════════════════════════════════════════════

const U := preload("res://_devtest/test_util.gd")

const OVERLAP_SAME_RANK := 0.45
const OVERLAP_DIFF_RANK := 0.60


func _init() -> void:
	var failed := _run()
	quit(1 if failed > 0 else 0)


static func _run() -> int:
	print("=".repeat(60))
	print("  手牌排序与布局测试 — test_sort.gd")
	print("=".repeat(60))

	var failed := 0
	failed += U.section("sorted_order — rank 降序", func(): _test_sorted_order_rank())
	failed += U.section("sorted_order — 同 rank 花色升序", func(): _test_sorted_order_suit())
	failed += U.section("sorted_order — 边界", func(): _test_sorted_order_edge())
	failed += U.section("x 位置 — 同点紧 / 异点松", func(): _test_xpos_overlap())
	failed += U.section("x 位置 — normal_by_pos 映射", func(): _test_normal_by_pos())
	failed += U.section("x 位置 — 手牌居中偏移", func(): _test_center_offset())

	return U.finish(failed, "手牌排序与布局")


# ═══════════════════════════════════════════════════════════════
# 1. Card.sorted_order — rank 降序
# ═══════════════════════════════════════════════════════════════

static func _test_sorted_order_rank() -> void:
	## 按战力排序：7(15) > 大鬼(14) > 小鬼(13) > 5(12) > 2(11)
	var hand := [
		U.card("2"),
		U.card("7"),
		U.card("5"),
		U.card("大鬼"),
		U.card("小鬼"),
	]
	var order := Card.sorted_order(hand)
	var sorted_names: Array[String] = []
	for i in order:
		sorted_names.append(hand[i].point)
	U.eq(sorted_names, ["7", "大鬼", "小鬼", "5", "2"], "战力降序排序")

	## 续：3(10) > A(9) > K(8) > Q(7) > J(6) > 10(5) > 9(4) > 8(3) > 6(2) > 4(1)
	var hand2 := [
		U.card("4"),
		U.card("10"),
		U.card("A"),
		U.card("3"),
		U.card("K"),
	]
	var order2 := Card.sorted_order(hand2)
	var sorted2: Array[String] = []
	for i in order2:
		sorted2.append(hand2[i].point)
	U.eq(sorted2, ["3", "A", "K", "10", "4"], "后段战力降序")


# ═══════════════════════════════════════════════════════════════
# 2. Card.sorted_order — 同 rank 花色升序
# ═══════════════════════════════════════════════════════════════

static func _test_sorted_order_suit() -> void:
	## 花色 Unicode：方块(♦) < 梅花(♣) < 红桃(♥) < 黑桃(♠)
	var hand := [
		U.card("K", "黑桃"),
		U.card("K", "红桃"),
		U.card("K", "梅花"),
		U.card("K", "方块"),
	]
	var order := Card.sorted_order(hand)
	var suits: Array[String] = []
	for i in order:
		suits.append(hand[i].suit)
	U.eq(suits, ["方块", "梅花", "红桃", "黑桃"], "同 K 花色升序")

	## 跨 rank 时 rank 优先，花色只影响同 rank 内部
	var hand2 := [
		U.card("7", "梅花"),
		U.card("K", "黑桃"),
		U.card("7", "方块"),
		U.card("K", "红桃"),
	]
	var order2 := Card.sorted_order(hand2)
	var pts: Array[String] = []
	for i in order2:
		pts.append(hand2[i].point + hand2[i].suit)
	U.eq(pts, ["7方块", "7梅花", "K红桃", "K黑桃"], "7>K，同 rank 内花色升序")


# ═══════════════════════════════════════════════════════════════
# 3. Card.sorted_order — 边界
# ═══════════════════════════════════════════════════════════════

static func _test_sorted_order_edge() -> void:
	## 空牌
	U.eq(Card.sorted_order([]), [], "空手牌=空")

	## 单张
	var single := [U.card("7")]
	U.eq(Card.sorted_order(single), [0], "单张=[0]")

	## 全同 rank + 不同花色
	var same := [
		U.card("A", "黑桃"),
		U.card("A", "红桃"),
		U.card("A", "方块"),
	]
	var o := Card.sorted_order(same)
	var ss: Array[String] = []
	for i in o:
		ss.append(same[i].suit)
	U.eq(ss, ["方块", "红桃", "黑桃"], "全 A 花色升序")


# ═══════════════════════════════════════════════════════════════
# 4. x 位置 — 同点紧 / 异点松
# ═══════════════════════════════════════════════════════════════

static func _compute_xpos(hand: Array, card_w: float) -> Array:
	## 模拟 _hand_metrics 的 x 位置计算（不含偏移）
	var n := hand.size()
	var by_pos := Card.sorted_order(hand)
	var xpos: Array = []
	xpos.resize(n)
	var cur := 0.0
	for p in n:
		var ri: int = by_pos[p]
		xpos[ri] = cur
		if p < n - 1:
			var next_ri: int = by_pos[p + 1]
			var same := Card.get_rank(hand[ri].point) == Card.get_rank(hand[next_ri].point)
			cur += card_w * (OVERLAP_SAME_RANK if same else OVERLAP_DIFF_RANK)
	return xpos


static func _test_xpos_overlap() -> void:
	const CW := 100.0

	## 同 rank → 紧密
	var same_rank := [
		U.card("7", "方块"),
		U.card("7", "梅花"),
		U.card("7", "红桃"),
	]
	var xp := _compute_xpos(same_rank, CW)
	var bp := Card.sorted_order(same_rank)
	# 按排序顺序取 x 差
	var gap01: float = abs(xp[bp[1]] - xp[bp[0]])
	var gap12: float = abs(xp[bp[2]] - xp[bp[1]])
	U.ok(abs(gap01 - CW * OVERLAP_SAME_RANK) < 0.01, "同 rank 间距=OVERLAP_SAME_RANK")
	U.ok(abs(gap12 - CW * OVERLAP_SAME_RANK) < 0.01, "同 rank 间距一致")

	## 异 rank → 松
	var diff_rank := [
		U.card("7", "方块"),
		U.card("K", "方块"),
		U.card("4", "方块"),
	]
	var xp2 := _compute_xpos(diff_rank, CW)
	var bp2 := Card.sorted_order(diff_rank)
	var g01: float = abs(xp2[bp2[1]] - xp2[bp2[0]])
	var g12: float = abs(xp2[bp2[2]] - xp2[bp2[1]])
	U.ok(abs(g01 - CW * OVERLAP_DIFF_RANK) < 0.01, "异 rank 间距=OVERLAP_DIFF_RANK")
	U.ok(abs(g12 - CW * OVERLAP_DIFF_RANK) < 0.01, "异 rank 间距一致")

	## 混合：同点紧，异点松
	var mixed := [
		U.card("7", "方块"),   # rank 15
		U.card("7", "梅花"),   # rank 15
		U.card("K", "方块"),   # rank 8
		U.card("K", "梅花"),   # rank 8
		U.card("4", "方块"),   # rank 1
	]
	var xp3 := _compute_xpos(mixed, CW)
	var bp3 := Card.sorted_order(mixed)
	# 排序：[7方块(0), 7梅花(1), K方块(2), K梅花(3), 4方块(4)]
	U.eq(_order_point_names(mixed, bp3), ["方块7", "梅花7", "方块K", "梅花K", "方块4"], "混合排序")

	# 7方块→7梅花：同 rank → SAME
	U.ok(abs(abs(xp3[bp3[1]] - xp3[bp3[0]]) - CW * OVERLAP_SAME_RANK) < 0.01, "7-7 同点紧")
	# 7梅花→K方块：异 rank → DIFF
	U.ok(abs(abs(xp3[bp3[2]] - xp3[bp3[1]]) - CW * OVERLAP_DIFF_RANK) < 0.01, "7-K 异点松")
	# K方块→K梅花：同 rank → SAME
	U.ok(abs(abs(xp3[bp3[3]] - xp3[bp3[2]]) - CW * OVERLAP_SAME_RANK) < 0.01, "K-K 同点紧")
	# K梅花→4方块：异 rank → DIFF
	U.ok(abs(abs(xp3[bp3[4]] - xp3[bp3[3]]) - CW * OVERLAP_DIFF_RANK) < 0.01, "K-4 异点松")


# ═══════════════════════════════════════════════════════════════
# 5. x 位置 — normal_by_pos 映射
# ═══════════════════════════════════════════════════════════════

static func _test_normal_by_pos() -> void:
	## 验证 normal_by_pos 是原手牌下标的一个排列，且排序后的 x 单调递增
	var hand := [
		U.card("K", "黑桃"),   # idx 0, rank 8
		U.card("7", "方块"),   # idx 1, rank 15
		U.card("A", "红桃"),   # idx 2, rank 9
		U.card("4", "梅花"),   # idx 3, rank 1
	]
	var bp := Card.sorted_order(hand)
	# 排序结果: 7方块(1) > A红桃(2) > K黑桃(0) > 4梅花(3)
	U.eq(bp, [1, 2, 0, 3], "sorted_order=[7,A,K,4]的原始下标")

	const CW := 100.0
	var xp := _compute_xpos(hand, CW)
	# 验证 x 在排序后单调递增
	var x_sorted: Array[float] = []
	for i in bp:
		x_sorted.append(xp[i])
	U.ok(x_sorted[0] < x_sorted[1], "x 排序后递增 0→1")
	U.ok(x_sorted[1] < x_sorted[2], "x 排序后递增 1→2")
	U.ok(x_sorted[2] < x_sorted[3], "x 排序后递增 2→3")

	## 验证 xp 按原始下标正确：原下标 1(7) 最左
	U.ok(xp[1] < xp[2], "7(原idx1) 在 A(原idx2) 左边")
	U.ok(xp[2] < xp[0], "A(原idx2) 在 K(原idx0) 左边")


# ═══════════════════════════════════════════════════════════════
# 6. x 位置 — 手牌居中偏移
# ═══════════════════════════════════════════════════════════════

static func _test_center_offset() -> void:
	const CW := 100.0

	## 偏移计算：offset = max((hand_w - total_w) * 0.5, edge_pad)
	var hand := [
		U.card("7", "方块"),
		U.card("K", "方块"),
		U.card("4", "方块"),
	]
	var xp := _compute_xpos(hand, CW)
	var bp := Card.sorted_order(hand)

	# total_w = 最后一个 x + card_w
	# 3 cards, 2 gaps: CW * (1 + 2 * DIFF_RANK) = CW * 2.2
	var total_w := CW + 2.0 * CW * OVERLAP_DIFF_RANK   # 100 + 120 = 220

	const HAND_W := 600.0
	const EDGE := 8.0
	var offset := maxf((HAND_W - total_w) * 0.5, EDGE)
	var expected_offset := (600.0 - 220.0) * 0.5   # 190

	U.ok(abs(offset - expected_offset) < 0.01, "手牌宽<容器宽时居中")
	U.ok(offset > EDGE, "居中偏移 > edge_pad")

	## 当 total_w > hand_w 时退化到 edge_pad
	const NARROW := 100.0
	var offset2 := maxf((NARROW - total_w) * 0.5, EDGE)
	U.eq(offset2, EDGE, "手牌超宽时退化为 edge_pad")


# ═══════════════════════════════════════════════════════════════
# 辅助
# ═══════════════════════════════════════════════════════════════

static func _order_point_names(hand: Array, order: Array) -> Array:
	var a: Array[String] = []
	for i in order:
		var c: Card = hand[i]
		a.append(c.suit + c.point if not c.suit.is_empty() else c.point)
	return a