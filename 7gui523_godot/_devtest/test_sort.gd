extends SceneTree

# ═══════════════════════════════════════════════════════════════
# TestSort — 手牌排序 + 布局位置测试
# 覆盖：Card.sorted_order 排序规则 / _hand_metrics 位置计算逻辑
#
# 命令行：
#   godot --headless --path . --script _devtest/test_sort.gd
# ═══════════════════════════════════════════════════════════════

const U := preload("res://_devtest/test_util.gd")

const MIN_GAP := 14.0
const MAX_GAP := 40.0


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
	failed += U.section("x 位置 — 均匀平铺不重叠", func(): _test_xpos_spread())
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
# 4. x 位置 — 均匀平铺不重叠
# ═══════════════════════════════════════════════════════════════

static func _compute_xpos(hand: Array, card_w: float, hand_w: float) -> Array:
	## 模拟 _hand_metrics 的均匀平铺：间距夹取 [MIN_GAP, MAX_GAP]，整体居中，绝不重叠
	var n := hand.size()
	var by_pos := Card.sorted_order(hand)
	var xpos: Array = []
	xpos.resize(n)
	if n == 0:
		return xpos
	var avail: float = maxf(hand_w - 24.0, card_w * n)
	var gap := MIN_GAP
	if n > 1:
		gap = clampf((avail - card_w * n) / float(n - 1), MIN_GAP, MAX_GAP)
	var total_w: float = card_w * n + gap * float(n - 1)
	var x0 := (hand_w - total_w) * 0.5
	for p in n:
		xpos[by_pos[p]] = x0 + float(p) * (card_w + gap)
	return xpos


static func _test_xpos_spread() -> void:
	const CW := 100.0
	const W := 600.0

	## 同 rank 间距均匀一致，不再按点数分组
	var same_rank := [
		U.card("7", "方块"),
		U.card("7", "梅花"),
		U.card("7", "红桃"),
	]
	var xp := _compute_xpos(same_rank, CW, W)
	var bp := Card.sorted_order(same_rank)
	var g01: float = xp[bp[1]] - xp[bp[0]]
	var g12: float = xp[bp[2]] - xp[bp[1]]
	U.ok(abs(g01 - g12) < 0.01, "同 rank 间距均匀一致")
	U.ok(g01 >= CW + MIN_GAP - 0.01, "同 rank 不重叠")

	## 混合手牌：所有相邻间距统一
	var mixed := [
		U.card("7", "方块"),   # rank 15
		U.card("7", "梅花"),   # rank 15
		U.card("K", "方块"),   # rank 8
		U.card("K", "梅花"),   # rank 8
		U.card("4", "方块"),   # rank 1
	]
	var xp3 := _compute_xpos(mixed, CW, W)
	var bp3 := Card.sorted_order(mixed)
	U.eq(_order_point_names(mixed, bp3), ["方块7", "梅花7", "方块K", "梅花K", "方块4"], "混合排序")
	var expect_gap := clampf((W - 24.0 - CW * 5.0) / 4.0, MIN_GAP, MAX_GAP)  # =19
	for i in range(4):
		var g: float = xp3[bp3[i + 1]] - xp3[bp3[i]]
		U.ok(abs(g - (CW + expect_gap)) < 0.01, "混合手牌间距统一 %d" % i)

	## 极窄容器：间距夹到 MIN_GAP，仍然不重叠
	var xn := _compute_xpos(mixed, CW, 300.0)
	var gn: float = xn[bp3[1]] - xn[bp3[0]]
	U.ok(abs(gn - (CW + MIN_GAP)) < 0.01, "窄容器夹到最小间距")

	## 宽容器：间距夹到 MAX_GAP，保持聚拢感
	var xw := _compute_xpos(mixed, CW, 2000.0)
	var gw: float = xw[bp3[1]] - xw[bp3[0]]
	U.ok(abs(gw - (CW + MAX_GAP)) < 0.01, "宽容器夹到最大间距")


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
	var xp := _compute_xpos(hand, CW, 600.0)
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
	const W := 600.0

	## 整手牌居中：x0 = (hand_w - total_w) * 0.5，间距先经夹取
	var hand := [
		U.card("7", "方块"),
		U.card("K", "方块"),
		U.card("4", "方块"),
	]
	var xp := _compute_xpos(hand, CW, W)
	var bp := Card.sorted_order(hand)
	var gap := clampf((W - 24.0 - CW * 3.0) / 2.0, MIN_GAP, MAX_GAP)  # =138 → 夹到 40
	var total_w := CW * 3.0 + gap * 2.0
	var x0 := (W - total_w) * 0.5
	U.ok(abs(xp[bp[0]] - x0) < 0.01, "整手牌居中")
	U.ok(x0 > 0.0, "居中左边距为正")

	## 单张牌居中
	var one := _compute_xpos([U.card("7", "方块")], CW, W)
	U.ok(abs(one[0] - (W - CW) * 0.5) < 0.01, "单张牌居中")


# ═══════════════════════════════════════════════════════════════
# 辅助
# ═══════════════════════════════════════════════════════════════

static func _order_point_names(hand: Array, order: Array) -> Array:
	var a: Array[String] = []
	for i in order:
		var c: Card = hand[i]
		a.append(c.suit + c.point if not c.suit.is_empty() else c.point)
	return a