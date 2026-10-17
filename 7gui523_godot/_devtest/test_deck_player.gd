extends SceneTree

# ═══════════════════════════════════════════════════════════════
# TestDeckPlayer — 牌堆与玩家存储层测试（对应 C++ test_deck.cpp + test_player.cpp）
# 覆盖：Deck.DeckManager（标准54张/抽取/补牌/回收/商店销毁）
#       Player.PlayerData（补牌到5/精确删除/返回值）
#
# 命令行：
#   godot --headless --path . --script _devtest/test_deck_player.gd
# ═══════════════════════════════════════════════════════════════

const U := preload("res://_devtest/test_util.gd")


func _init() -> void:
	var failed := _run()
	quit(1 if failed > 0 else 0)


static func _run() -> int:
	var failed := 0
	print("=".repeat(60))
	print("  牌堆与玩家存储测试 — test_deck_player.gd")
	print("=".repeat(60))

	failed += U.section("createStandardDeck", func(): _test_create_standard())
	failed += U.section("draw_from_active", func(): _test_draw())
	failed += U.section("refill_to_five (普通区)", func(): _test_refill_to_five())
	failed += U.section("refill_active (牌堆内补牌)", func(): _test_refill_active())
	failed += U.section("add_to_hidden (回收)", func(): _test_add_hidden())
	failed += U.section("shop_destroy_flow", func(): _test_shop_destroy())
	failed += U.section("remove_cards_from_hand", func(): _test_remove_from_hand())
	failed += U.section("sort_hand", func(): _test_sort_hand())

	return U.finish(failed, "牌堆与玩家")


# ═══════════════════════════════════════════════════════════════
# 1. createStandardDeck — 标准 54 张初始化
# ═══════════════════════════════════════════════════════════════

static func _test_create_standard() -> void:
	var d := Deck.DeckManager.new()
	d.init_standard()
	U.eq(d.total(), 54, "标准牌堆共 54 张")
	U.eq(d.active_size(), Deck.DeckManager.DECK_CAP, "游戏牌堆 32 张")
	U.eq(d.total() - d.active_size(), 22, "隐藏牌堆 22 张")

	# 有序模式：总计不变，且顺序已知（0-53）
	var o := Deck.DeckManager.new()
	o.init_ordered()
	U.eq(o.total(), 54, "有序牌堆共 54 张")
	U.eq(o.cards[0], 0, "有序第 0 张为 0")


# ═══════════════════════════════════════════════════════════════
# 2. draw_from_active — 抽取逻辑
# ═══════════════════════════════════════════════════════════════

static func _test_draw() -> void:
	var d := Deck.DeckManager.new()
	d.init_ordered()

	# 抽 5 张（真实移出数组）
	var drawn := d.draw_from_active(5)
	U.eq(drawn.size(), 5, "抽 5 张得到 5 张")
	U.eq(d.active_size(), Deck.DeckManager.DECK_CAP - 5, "抽 5 张后游戏区减 5")
	U.eq(d.total(), 54 - 5, "抽牌真实移出，总牌数减 5")

	# 抽超过游戏区可抽取量 → 取 mini
	var before := d.active_size()
	var over := d.draw_from_active(100)
	U.eq(over.size(), before, "抽超量时按剩余游戏区数量抽取")
	U.eq(d.active_size(), 0, "游戏区抽空为 0")

	# 空牌堆抽取 → 空结果
	var e := Deck.DeckManager.new()
	var none := e.draw_from_active(5)
	U.eq(none.size(), 0, "空牌堆抽取返回空")

	# 抽 0 张 → 空结果，数量不变
	var z := Deck.DeckManager.new()
	z.init_ordered()
	var zero := z.draw_from_active(0)
	U.eq(zero.size(), 0, "抽 0 张返回空")
	U.eq(z.total(), 54, "抽 0 张不改变总数")


# ═══════════════════════════════════════════════════════════════
# 3. refill_to_five — 玩家普通区补到 5 张
# ═══════════════════════════════════════════════════════════════

static func _test_refill_to_five() -> void:
	# 3 张补到 5
	var d := Deck.DeckManager.new()
	d.init_standard()
	var p := Player.PlayerData.new()
	d.fill_player_normal(p)
	U.eq(p.normal_hand.size(), 5, "首次补牌到 5 张")

	# 手牌不满时补满
	p.normal_hand.resize(3)
	d.fill_player_normal(p)
	U.eq(p.normal_hand.size(), 5, "不满补到 5 张")

	# 牌堆无牌时不补
	var empty_deck := Deck.DeckManager.new()
	var p2 := Player.PlayerData.new()
	p2.normal_hand = [U.card("7"), U.card("5")]
	empty_deck.fill_player_normal(p2)
	U.eq(p2.normal_hand.size(), 2, "空牌堆不补，手牌保持")

	# 手牌已满（5 张）不抽取，牌堆数量不变
	var d3 := Deck.DeckManager.new()
	d3.init_standard()
	var p3 := Player.PlayerData.new()
	for i in 5:
		p3.normal_hand.append(U.card("A"))
	d3.fill_player_normal(p3)
	U.eq(p3.normal_hand.size(), 5, "手牌已满不重复补")
	U.eq(d3.active_size(), Deck.DeckManager.DECK_CAP, "满手不抽，游戏区不变")

	# 立刻补区（instant）独立上限 2
	var d4 := Deck.DeckManager.new()
	d4.init_standard()
	var p4 := Player.PlayerData.new()
	d4.fill_player_instant(p4)
	U.eq(p4.instant_hand.size(), 2, "立刻补区抽到 2 张")


# ═══════════════════════════════════════════════════════════════
# 4. refill_active — 牌堆内补牌（分界线右移）
# ═══════════════════════════════════════════════════════════════

static func _test_refill_active() -> void:
	var d := Deck.DeckManager.new()
	d.init_ordered()
	# 抽掉部分游戏区
	d.draw_from_active(10)
	U.eq(d.active_size(), Deck.DeckManager.DECK_CAP - 10, "抽 10 后游戏区 22")
	d.refill_active()
	U.eq(d.active_size(), Deck.DeckManager.DECK_CAP, "补牌回 32")

	# 隐藏区不足时，补到最大可用
	var d2 := Deck.DeckManager.new()
	d2.init_ordered()
	d2.cards = d2.cards.slice(0, 10)   # 人为裁剪到 10 张
	d2.active_count = 5
	d2.refill_active()
	U.eq(d2.active_size(), 10, "隐藏不足时补到全部可用")


# ═══════════════════════════════════════════════════════════════
# 5. add_to_hidden — 出牌回收进隐藏区
# ═══════════════════════════════════════════════════════════════

static func _test_add_hidden() -> void:
	var d := Deck.DeckManager.new()
	d.init_ordered()
	var before_total := d.total()
	var ok := d.add_to_hidden(U.card("7", "黑桃"))
	U.ok(ok, "合法有效牌可回收")
	U.eq(d.total(), before_total + 1, "回收后总数 +1")
	U.eq(d.active_size(), Deck.DeckManager.DECK_CAP, "回收进隐藏区不改变游戏区")

	# 无效牌（无法映射 id）返回 false
	var bad := d.add_to_hidden(U.card("不存在点"))
	U.ok(not bad, "非法牌回收失败")


# ═══════════════════════════════════════════════════════════════
# 6. shop_destroy_flow — 商店销毁 7 张流程
# ═══════════════════════════════════════════════════════════════

static func _test_shop_destroy() -> void:
	var d := Deck.DeckManager.new()
	d.init_standard()
	var res := d.shop_destroy_flow(0)
	U.eq(res["shown"].size(), 7, "展示 7 张")
	U.ok(res["destroyed"] != null, "选中第 0 张被销毁")
	U.eq(res["remaining"].size(), 6, "剩余 6 张放回")
	U.eq(d.active_size(), Deck.DeckManager.DECK_CAP, "销毁后补回游戏区容量")

	# 空牌堆 → 空结果
	var e := Deck.DeckManager.new()
	var empty_res := e.shop_destroy_flow(0)
	U.eq(empty_res["shown"].size(), 0, "空牌堆销毁返回空")
	U.eq(empty_res["destroyed"], null, "空牌堆无销毁对象")


# ═══════════════════════════════════════════════════════════════
# 7. remove_cards_from_hand — 从手牌精确删除
# ═══════════════════════════════════════════════════════════════

static func _test_remove_from_hand() -> void:
	# 删除存在的牌
	var p := Player.PlayerData.new()
	p.normal_hand = [U.card("7"), U.card("5"), U.card("K")]
	p.remove_cards_from_hand([U.card("5")])
	U.eq(p.normal_hand.size(), 2, "删存在牌后剩 2")

	# 删除不存在的牌 → 无变化
	var p2 := Player.PlayerData.new()
	p2.normal_hand = [U.card("7"), U.card("5")]
	p2.remove_cards_from_hand([U.card("A")])
	U.eq(p2.normal_hand.size(), 2, "删不存在牌不变")

	# 按 pair+suit 精确匹配（同名不同花只删目标那张）
	var p3 := Player.PlayerData.new()
	p3.normal_hand = [U.card("7"), U.card("5", "黑桃"), U.card("5", "红桃")]
	p3.remove_cards_from_hand([U.card("5", "红桃")])
	U.eq(p3.normal_hand.size(), 2, "删精确花色那张后剩 2")
	# 剩余应仍含 黑桃5
	var has_sp := p3.normal_hand.any(func(c: Card): return c.point == "5" and c.suit == "黑桃")
	U.ok(has_sp, "另一个同点不同花未被动")

	# 空手删除不报错
	var p4 := Player.PlayerData.new()
	p4.remove_cards_from_hand([U.card("A")])
	U.eq(p4.normal_hand.size(), 0, "空手删除保持空")


# ═══════════════════════════════════════════════════════════════
# 8. sort_hand — 手牌排序
# ═══════════════════════════════════════════════════════════════

static func _test_sort_hand() -> void:
	var p := Player.PlayerData.new()
	p.normal_hand = [U.card("K"), U.card("4"), U.card("大鬼"), U.card("7")]
	p.sort_hand()
	# 等级降序：7(15) > 大鬼(14) > K(8) > 4(1)
	U.eq(p.normal_hand[0].point, "7", "排序后第 0 张 7（最大）")
	U.eq(p.normal_hand[1].point, "大鬼", "排序后第 1 张大鬼")
	U.eq(p.normal_hand[2].point, "K", "排序后第 2 张 K")
	U.eq(p.normal_hand[3].point, "4", "排序后最后 4")