extends Object

# ═══════════════════════════════════════════════════════════════
# TestChips — 负筹码（负债）/ 压牌 / 第 9 关 / BOSS 清空惩罚
#
# 规则：
#   出牌下注 = 基础筹码（5/10/K）+ 压牌筹码（同牌型同点，特殊牌型压过即算）
#             + 立刻补区加注（出 1 张 +1，一次出 2 张 +3）
#   小回合底注：双方各 10，上一小回合赢家额外 +5（共 15）
#   小回合结束：桌面给赢家（第 9 关固定给 BOSS）
#   结算前记下双方负债，再按 2× 从筹码中扣除；结算后仍为负 = 还不起
# ═══════════════════════════════════════════════════════════════

const U := preload("res://_devtest/test_util.gd")


static func _run() -> int:
	var failed := 0
	failed += U.section("基础下注与桌面", func(): _test_base())
	failed += U.section("小回合开局底注", func(): _test_round_ante())
	failed += U.section("压牌筹码", func(): _test_pressure())
	failed += U.section("立刻补区加注", func(): _test_instant_stake())
	failed += U.section("负债双倍偿还 → 还不起", func(): _test_debt())
	failed += U.section("赢家带债结算", func(): _test_winner_debt())
	failed += U.section("第9关桌面归BOSS", func(): _test_level9_pot_to_boss())
	failed += U.section("BOSS清空 3N 惩罚", func(): _test_boss_finish_penalty())
	failed += U.section("7gui523 只赢小回合", func(): _test_gui523_small_round())
	failed += U.section("付得起双倍 / 不借钱", func(): _test_pay_exact())
	failed += U.section("桌面刚好抵双倍", func(): _test_pot_covers_debt())
	return U.finish(failed, "筹码结算")


static func _make_round(hand_a: Array[Card], hand_b: Array[Card],
		chips_a: int = 500, chips_b: int = 500,
		boss: Player.PlayerData = null) -> Dictionary:
	var deck_a := Deck.DeckManager.new()
	var deck_b := Deck.DeckManager.new()
	deck_a.init_standard()
	deck_b.init_standard()
	var a := Player.PlayerData.new()
	var b := Player.PlayerData.new()
	a.name = "A"
	b.name = "B"
	a.reset_battle_chips(chips_a)
	b.reset_battle_chips(chips_b)
	a.normal_hand.clear()
	a.normal_hand.append_array(hand_a)
	b.normal_hand.clear()
	b.normal_hand.append_array(hand_b)
	return {"round": Round.new(a, b, deck_a, deck_b, null, null, boss), "a": a, "b": b}


## 首局底注 10/人；K 下注 20；5 压 K 下注 5；A 不要 → B 收 45
static func _test_base() -> void:
	var ctx := _make_round([U.card("K")], [U.card("5")])
	var r: Round = ctx["round"]
	var a: Player.PlayerData = ctx["a"]
	var b: Player.PlayerData = ctx["b"]

	r.process_play([a.normal_hand[0]])
	U.eq(a.chips, 470, "A 打出 K：底注 10 + 下注 20 → 470")
	U.eq(r.table_chips, 40, "桌面 40（底注 20 + 下注 20）")

	r.process_play([b.normal_hand[0]])
	U.eq(b.chips, 485, "B 打出 5：底注 10 + 下注 5 → 485")
	U.eq(r.table_chips, 45, "桌面 45")

	var res := r.process_play([])
	U.eq(res["state"], "round_win", "A 不要 → 小回合结束")
	U.eq(res["pot"], 45, "pot=45")
	U.eq(b.chips, 530, "B 收走桌面 45")
	U.eq(a.chips, 470, "A 净损失 30（底注 10 + 出牌 20）")
	U.eq(r.table_chips, 0, "桌面清空")


## 小回合开局底注：双方各 10；上一小回合赢家下一回合投 15
static func _test_round_ante() -> void:
	var ctx := _make_round([U.card("5")], [U.card("K")])
	var r: Round = ctx["round"]
	var a: Player.PlayerData = ctx["a"]
	var b: Player.PlayerData = ctx["b"]

	# 第一小回合：A 首手 → 双方底注 10，A 另下注 5
	r.process_play([a.normal_hand[0]])
	U.eq(a.chips, 485, "首回合 A：底注 10 + 下注 5")
	U.eq(b.chips, 490, "首回合 B：底注 10")
	U.eq(r.table_chips, 25, "首回合桌面 25")

	var res := r.process_play([])
	U.eq(res["state"], "round_win", "B 不要，A 赢小回合")
	U.eq(a.chips, 510, "A 收桌面 25")

	# 下一小回合：赢家 A 先手，底注 15；B 底注 10
	a.normal_hand.clear()
	a.normal_hand.append(U.card("5"))
	r.process_play([a.normal_hand[0]])
	U.eq(a.chips, 490, "赢家 A 下一回合底注 15 + 下注 5")
	U.eq(b.chips, 480, "B 下一回合底注 10")
	U.eq(r.table_chips, 30, "桌面 25（底注）+ 下注 5")


## 5 压 5：底注 10 + 基础 5 + 压牌 15 = 下注 30
static func _test_pressure() -> void:
	var ctx := _make_round([U.card("5")], [U.card("5")])
	var r: Round = ctx["round"]
	var a: Player.PlayerData = ctx["a"]
	var b: Player.PlayerData = ctx["b"]

	r.process_play([a.normal_hand[0]])
	U.eq(a.chips, 485, "A 底注 10 + 下注 5")

	r.process_play([b.normal_hand[0]])
	U.eq(b.chips, 470, "B 底注 10 + 压 5：5 + 15 = 20")
	U.eq(r.table_chips, 45, "桌面 45")


## 立刻补区出牌加注：1 张 +1，一次出 2 张 +3（都计入桌面下注）
static func _test_instant_stake() -> void:
	# 单张：A 从立刻补区出 7（基础 0）+ 加注 1
	var ctx := _make_round([U.card("K")], [U.card("8")])
	var r: Round = ctx["round"]
	var a: Player.PlayerData = ctx["a"]
	a.instant_hand = [U.card("7")]
	r.process_play([a.instant_hand[0]])
	U.eq(a.chips, 489, "立刻补区 1 张：底注 10 + 加注 1")
	U.eq(r.table_chips, 21, "桌面 21")

	# 两张：A 一次出立刻补区的 7 + 7（基础 0）+ 加注 3
	var ctx2 := _make_round([U.card("K")], [U.card("8")])
	var r2: Round = ctx2["round"]
	var a2: Player.PlayerData = ctx2["a"]
	a2.instant_hand = [U.card("7"), U.card("7")]
	var pair: Array[Card] = [a2.instant_hand[0], a2.instant_hand[1]]
	r2.process_play(pair)
	U.eq(a2.chips, 487, "立刻补区 2 张：底注 10 + 加注 3")
	U.eq(r2.table_chips, 23, "桌面 23")


## A 只有 3 筹码，底注 10 + 下注 5 → -12；输掉后还不起，停在 -12
static func _test_debt() -> void:
	var ctx := _make_round([U.card("5")], [U.card("7")], 3, 500)
	var r: Round = ctx["round"]
	var a: Player.PlayerData = ctx["a"]
	var b: Player.PlayerData = ctx["b"]

	r.process_play([a.normal_hand[0]])
	U.eq(a.chips, -12, "A 底注 10 + 下注 5 后筹码 -12（负数即负债）")

	r.process_play([b.normal_hand[0]])
	var res := r.process_play([])
	U.eq(res["state"], "round_win", "A 不要")
	U.eq(a.chips, -12, "A 付不起 2×12，余额停在 -12，不继续借")
	U.eq(b.chips, 515, "B 收桌面 25（含双方底注）")
	U.ok(r.failed_players.has(a), "A 被记为还不起")


## A 负债 12 但赢下小回合：收 25 后为 13，仍不够还 24
static func _test_winner_debt() -> void:
	var ctx := _make_round([U.card("5")], [U.card("K")], 3, 500)
	var r: Round = ctx["round"]
	var a: Player.PlayerData = ctx["a"]
	var b: Player.PlayerData = ctx["b"]

	r.process_play([a.normal_hand[0]])
	var res := r.process_play([])
	U.eq(res["state"], "round_win", "B 不要")
	U.eq(a.chips, 13, "A 收 25 后为 13，不够还 24，余额保持 13")
	U.ok(r.failed_players.has(a), "A 被记为还不起")


## 第 9 关：即使 A 赢下小回合，桌面也归 BOSS
static func _test_level9_pot_to_boss() -> void:
	var boss := Player.PlayerData.new()
	boss.name = "BOSS"
	var ctx := _make_round([U.card("5")], [U.card("K")], 500, 500, boss)
	var r: Round = ctx["round"]
	r.pot_to_boss = true
	var a: Player.PlayerData = ctx["a"]
	var b: Player.PlayerData = ctx["b"]

	r.process_play([a.normal_hand[0]])
	var res := r.process_play([])
	U.eq(res["state"], "round_win", "B 不要，A 赢小回合")
	U.eq(a.chips, 485, "A 付底注 10 + 自己的 5")
	U.eq(b.chips, 490, "B 只付底注 10，没有收到桌面")
	U.eq(boss.chips, 525, "桌面 25（双方底注 + A 的 5）归 BOSS")


## 7gui523 只赢小回合；先结算桌面/负债，付不起则本局结束
static func _test_gui523_small_round() -> void:
	var gui: Array[Card] = [U.card("7"), U.card("大鬼"), U.card("5"), U.card("2"), U.card("3")]

	# A 有 30：底注 10 + 下注 5 → 15；收桌面 25 → 40；无负债，正常赢小回合
	var ctx := _make_round(gui, [U.card("K")], 30, 500)
	var r: Round = ctx["round"]
	var a: Player.PlayerData = ctx["a"]
	var res := r.process_play(gui)
	U.eq(res["state"], "round_win", "7gui523 赢得小回合")
	U.eq(a.chips, 40, "A 底注 10 + 下注 5，收桌面 25 → 40")
	U.ok(not r.failed_players.has(a), "无负债，不失败")


## 付得起 2×负债 → 扣到 0；付不起 → 余额不动，不借钱
static func _test_pay_exact() -> void:
	var p := Player.PlayerData.new()

	p.chips = 5
	U.ok(p.pay_debt(2), "5 够付 2×2=4")
	U.eq(p.chips, 1, "5 还 4 剩 1")

	p.chips = 20
	U.ok(p.pay_debt(10), "20 够付 2×10")
	U.eq(p.chips, 0, "付完刚好 0")

	p.chips = 3
	U.ok(not p.pay_debt(2), "3 不够付 2×2=4")
	U.eq(p.chips, 3, "付不起就不借，余额保持 3")


## A 只有 10：底注 10 + 10 的 10 → -10；桌面 20（双方底注）+ 10 = 30
## 赢下后收 30 → 20，双倍还 20 → 0（桌面刚好覆盖双倍负债）
static func _test_pot_covers_debt() -> void:
	var ctx := _make_round([U.card("10")], [U.card("2")], 10, 500)
	var r: Round = ctx["round"]
	var a: Player.PlayerData = ctx["a"]

	r.process_play([a.normal_hand[0]])
	U.eq(a.chips, -10, "A 底注 10 + 下注 10 后 -10")
	U.eq(r.table_chips, 30, "桌面 30（双方底注 20 + A 的 10）")
	var res := r.process_play([])
	U.eq(res["state"], "round_win", "B 不要，A 赢")
	U.eq(a.chips, 0, "A 收 30 → 20，双倍还 20 → 0")
	U.ok(not r.failed_players.has(a), "刚好付得起，不失败")


## BOSS 清空：玩家剩余牌 × 3 在结算前扣筹码
static func _test_boss_finish_penalty() -> void:
	var deck_boss := Deck.DeckManager.new()
	var deck_human := Deck.DeckManager.new()
	deck_boss.init_standard()
	deck_human.init_standard()
	deck_boss.active_count = 0

	var human := Player.PlayerData.new()
	var boss := Player.PlayerData.new()
	human.name = "H"
	boss.name = "B"
	human.normal_hand = [U.card("5"), U.card("5")]
	boss.normal_hand = [U.card("7")]
	human.reset_battle_chips(500)
	boss.reset_battle_chips(500)

	var r := Round.new(boss, human, deck_boss, deck_human, null, null, boss)
	var res := r.process_play([boss.normal_hand[0]])
	# 玩家剩余 = 2 张手牌 + 32 张 active = 34 → 34 × 3 = 102
	U.eq(res["state"], "boss_finish", "BOSS 手牌+牌堆空")
	U.eq(res["penalty"], 102, "3N 惩罚 = 102")
	U.eq(human.chips, 388, "玩家筹码 500 - 底注 10 - 惩罚 102")
	U.eq(boss.chips, 510, "桌面 20（双方底注）归 BOSS")