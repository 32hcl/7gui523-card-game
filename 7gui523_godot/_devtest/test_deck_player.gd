extends SceneTree

# ═══════════════════════════════════════════════════════════════
# TestDeckPlayer — 牌堆与玩家存储层测试（对应 C++ test_deck.cpp + test_player.cpp）
# 覆盖：Deck.DeckManager（单数组 ID 牌堆：标准54张/抽取/补牌/回收）
#       Player.PlayerData（双区补牌/精确删除/排序）
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
	failed += U.section("remove_cards_from_hand", func(): _test_remove_from_hand())
	failed += U.section("sort_hand", func(): _test_sort_hand())

	return U.finish(failed, "牌堆与玩家")


## 有序测试牌堆：ID 0-53 依序，游戏区 32 张 / 隐藏区 22 张
## （新模型 init_standard 会洗牌，确定性断言改用手动布置）
static func _make_ordered() -> Deck.DeckManager:
	var d := Deck.DeckManager.new()
	for i in 54:
		d.cards.append(i)
	d.active_count = Deck.DeckManager.DECK_CAP
	return d


# ═══════════════════════════════════════════════════════════════
# 1. createStandardDeck — 标准 54 张初始化
# ═══════════════════════════════════════════════════════════════

static func _test_create_standard() -> void:
	var d := Deck.DeckManager.new()
	d.init_standard()
	U.eq(d.total(), 54, "标准牌堆共 54 张")
	U.eq(d.active_count, Deck.DeckManager.DECK_CAP, "游戏牌堆 32 张")
	U.eq(d.total() - d.active_count, 22, "隐藏牌堆 22 张")

	# 手动布置：cards 为 ID 数组，可自由构造任意牌堆状态
	var o := _make_ordered()
	U.eq(o.total(), 54, "手动布置共 54 张")
	U.eq(o.cards[0], 0, "有序第 0 张为 0")


# ═══════════════════════════════════════════════════════════════
# 2. draw_from_active — 抽取逻辑
# ═══════════════════════════════════════════════════════════════

static func _test_draw() -> void:
	var d := _make_ordered()

	# 抽 5 张（真实移出数组）
	var drawn := d.draw_from_active(5)
	U.eq(drawn.size(), 5, "抽 5 张得到 5 张")
	U.eq(d.active_count, Deck.DeckManager.DECK_CAP - 5, "抽 5 张后游戏区减 5")
	U.eq(d.total(), 54 - 5, "抽牌真实移出，总牌数减 5")

	# 抽超过游戏区可抽取量 → 取 mini
	var before := d.active_count
	var over := d.draw_from_active(100)
	U.eq(over.size(), before, "抽超量时按剩余游戏区数量抽取")
	U.eq(d.active_count, 0, "游戏区抽空为 0")

	# 空牌堆抽取 → 空结果
	var e := Deck.DeckManager.new()
	var none := e.draw_from_active(5)
	U.eq(none.size(), 0, "空牌堆抽取返回空")

	# 抽 0 张 → 空结果，数量不变
	var z := _make_ordered()
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
	U.eq(d3.active_count, Deck.DeckManager.DECK_CAP, "满手不抽，游戏区不变")

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
	var d := _make_ordered()
	# 抽掉部分游戏区
	d.draw_from_active(10)
	U.eq(d.active_count, Deck.DeckManager.DECK_CAP - 10, "抽 10 后游戏区 22")
	d.refill_active()
	U.eq(d.active_count, Deck.DeckManager.DECK_CAP, "补牌回 32")

	# 隐藏区不足时，补到最大可用
	var d2 := _make_ordered()
	d2.cards = d2.cards.slice(0, 10)   # 人为裁剪到 10 张
	d2.active_count = 5
	d2.refill_active()
	U.eq(d2.active_count, 10, "隐藏不足时补到全部可用")


# ═══════════════════════════════════════════════════════════════
# 5. add_to_hidden — 出牌回收进隐藏区
# ═══════════════════════════════════════════════════════════════

static func _test_add_hidden() -> void:
	var d := _make_ordered()
	var before_total := d.total()
	## 回收契约要求 card.id >= 0（真实牌局的牌都来自 Card.from_id），
	## 用 U.card() 造牌时 id 默认 -1，会走"非法牌"分支
	var ok := d.add_to_hidden(Card.from_id(Card.id_for("7", "黑桃")))
	U.ok(ok, "合法有效牌可回收")
	U.eq(d.total(), before_total + 1, "回收后总数 +1")
	U.eq(d.active_count, Deck.DeckManager.DECK_CAP, "回收进隐藏区不改变游戏区")

	# 无效牌（无法映射 id）返回 false
	var bad := d.add_to_hidden(U.card("不存在点"))
	U.ok(not bad, "非法牌回收失败")


# ═══════════════════════════════════════════════════════════════
# 6. remove_cards_from_hand — 从手牌精确删除
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
# 7. sort_hand — 手牌排序
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
