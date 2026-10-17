extends SceneTree

# ═══════════════════════════════════════════════════════════════
# TestAIRound — AI 决策与回合链路单测
# 覆盖 AI：AICore.choose（枚举/首出/空手）
#       Round：process_play（自由局不能空过、合法首出轮换、非法牌型、
#                            压不过上家、PASS 上家赢并补牌、GUI523 直接胜）
#
# 命令行：
#   godot --headless --path . --script _devtest/test_ai_round.gd
# ═══════════════════════════════════════════════════════════════

const U := preload("res://_devtest/test_util.gd")


func _init() -> void:
	var failed := _run()
	quit(1 if failed > 0 else 0)


static func _run() -> int:
	var failed := 0
	print("=".repeat(60))
	print("  AI 决策与回合链路测试 — test_ai_round.gd")
	print("=".repeat(60))

	failed += U.section("AI.choose 枚举/首出/空手", func(): _test_ai_choose())
	failed += U.section("自由局不能空过", func(): _test_free_no_pass())
	failed += U.section("合法首出 → 轮换", func(): _test_lead_continue())
	failed += U.section("非法牌型 → invalid", func(): _test_invalid_type())
	failed += U.section("压不过上家 → cannot_beat", func(): _test_cannot_beat())
	failed += U.section("PASS → 上家赢并补牌", func(): _test_pass_round_win())
	failed += U.section("GUI523 → 直接胜", func(): _test_gui523_instant())
	failed += U.section("第9关禁止玩家 GUI523", func(): _test_level9_gui523_ban())
	failed += U.section("赖账鬼 ghost_cheat 规则", func(): _test_ghost_cheat())

	return U.finish(failed, "AI/回合")


# ═══════════════════════════════════════════════════════════════
# 造局辅助：给定双方手牌 + 满标准牌堆
# ═══════════════════════════════════════════════════════════════

static func _make_round(hand_a: Array, hand_b: Array) -> Dictionary:
	var deck_a := Deck.DeckManager.new()
	var deck_b := Deck.DeckManager.new()
	deck_a.init_standard()
	deck_b.init_standard()
	var a := Player.PlayerData.new()
	var b := Player.PlayerData.new()
	a.name = "A"
	b.name = "B"
	a.normal_hand.clear()
	a.normal_hand.append_array(hand_a)
	b.normal_hand.clear()
	b.normal_hand.append_array(hand_b)
	var round := Round.new(a, b, deck_a, deck_b)
	return {"round": round, "a": a, "b": b, "deck_a": deck_a, "deck_b": deck_b}


# ═══════════════════════════════════════════════════════════════
# 1. AI.choose — 枚举 / 首出 / 空手
# ═══════════════════════════════════════════════════════════════

static func _test_ai_choose() -> void:
	# 枚举：有手牌时合法动作非空
	var p := Player.PlayerData.new()
	p.normal_hand = [U.card("3"), U.card("3"), U.card("K")]
	p.ai_level = AICore.Level.AI1_RULE
	var legal := AICore.enumerate_legal_plays(p)
	U.ok(legal.size() > 0, "有手牌时枚举出合法动作")

	# 首出（prev 为空）：返回合法组合，通常为单张或对子
	var lead := AICore.choose(p, Player.PlayerData.new(), null, null, 0, null)
	U.ok(not lead.is_empty(), "桌面无牌时首出必有动作")
	U.ok(CardTypeResult.parse_any_card_type(lead).type != CardTypeResult.CardType.INVALID,
		"首出是合法牌型")

	# 空手：返回空，不掉链子
	var empty := Player.PlayerData.new()
	empty.ai_level = AICore.Level.AI1_RULE
	U.eq(AICore.choose(empty, Player.PlayerData.new(), null, null, 0, null).size(), 0,
		"空手 choose 返回空")


# ═══════════════════════════════════════════════════════════════
# 2. 自由局（桌面无牌）不能空过 → invalid
# ═══════════════════════════════════════════════════════════════

static func _test_free_no_pass() -> void:
	var ctx := _make_round([U.card("7")], [U.card("5")])
	var r: Round = ctx["round"]
	var pass_res: Array[Card] = []
	var res: Dictionary = r.process_play(pass_res)
	U.eq(res["state"], "invalid", "自由局空过判为 invalid")
	U.eq(res["reason"], "不能空过！", "原因提示不能空过")


# ═══════════════════════════════════════════════════════════════
# 3. 合法首出 → 轮换到下家，桌上留牌
# ═══════════════════════════════════════════════════════════════

static func _test_lead_continue() -> void:
	var ctx := _make_round([U.card("7"), U.card("9")], [U.card("5")])
	var r: Round = ctx["round"]
	var res: Dictionary = r.process_play([U.card("7")])
	U.eq(res["state"], "continue", "合法首出进入 continue")
	U.ok(r.last_play != null, "last_play 已记录")
	U.eq(r.table_cards.size(), 1, "桌上留 1 张牌")
	U.eq(r.whose_turn(), ctx["b"], "轮换到对方")
	U.eq(ctx["a"].normal_hand.size(), 1, "首出者手牌减 1（剩 9）")


# ═══════════════════════════════════════════════════════════════
# 4. 非法牌型 → invalid（交换回合）
# ═══════════════════════════════════════════════════════════════

static func _test_invalid_type() -> void:
	var ctx := _make_round([U.card("3"), U.card("7")], [U.card("5")])
	var r: Round = ctx["round"]
	var play: Array[Card] = [U.card("3"), U.card("7")]
	var res: Dictionary = r.process_play(play)
	U.eq(res["state"], "invalid", "散牌判为 invalid 牌型")
	U.eq(r.whose_turn(), ctx["b"], "非法出牌后交回对方回合")


# ═══════════════════════════════════════════════════════════════
# 5. 打不过上家 → cannot_beat（交换回合）
# ═══════════════════════════════════════════════════════════════

static func _test_cannot_beat() -> void:
	var ctx := _make_round([U.card("9"), U.card("A")], [U.card("8")])
	var r: Round = ctx["round"]
	r.process_play([U.card("9")])                    # A 出单张 9，轮到 B
	var res := r.process_play([U.card("8")])         # B 想用 8 压，压不过
	U.eq(res["state"], "cannot_beat", "较小牌压不过返回 cannot_beat")
	U.eq(r.whose_turn(), ctx["a"], "压不过后回到上家回合")


# ═══════════════════════════════════════════════════════════════
# 6. 有人出过牌后，下家 PASS → 上家赢，双方补牌到 5
# ═══════════════════════════════════════════════════════════════

static func _test_pass_round_win() -> void:
	var ctx := _make_round([U.card("9")], [U.card("8")])
	var r: Round = ctx["round"]
	r.process_play([U.card("9")])                    # A 出 9，last_player=A，轮到 B
	var res: Dictionary = r.process_play([])                    # B 不要
	U.eq(res["state"], "round_win", "不要则上家赢")
	U.eq(res["winner"], ctx["a"], "赢家为出牌的上家 A")
	U.eq(ctx["a"].normal_hand.size(), 5, "赢家 A 补牌到 5")
	U.eq(ctx["b"].normal_hand.size(), 5, "输家 B 补牌到 5")
	U.eq(r.table_cards.size(), 0, "桌面清空进入下一小局")


# ═══════════════════════════════════════════════════════════════
# 7. GUI523（7+大鬼+5+2+3）→ 直接胜
# ═══════════════════════════════════════════════════════════════

static func _test_gui523_instant() -> void:
	var gui: Array[Card] = [U.card("7"), U.card("大鬼"), U.card("5"), U.card("2"), U.card("3")]
	var ctx2 := _make_round(gui, [U.card("K")])
	var res: Dictionary = ctx2["round"].process_play(gui)
	U.eq(res["state"], "instant_win", "GUI523 直接获胜")
	U.eq(res["winner"], ctx2["a"], "胜利者为首出者 A")


static func _test_level9_gui523_ban() -> void:
	var gui: Array[Card] = [U.card("7"), U.card("大鬼"), U.card("5"), U.card("2"), U.card("3")]
	var ctx := _make_round(gui, [U.card("K")])
	var r: Round = ctx["round"]
	r._boss = ctx["b"]
	r.level_rule = Campaign.level_rule(9)
	var res := r.process_play(gui)
	U.eq(res["state"], "invalid", "第9关玩家打出 GUI523 被拒绝")
	U.eq(res["reason"], "本关禁止打出 7gui523！", "第9关提示禁止 GUI523")


# ═══════════════════════════════════════════════════════════════
# 8. 赖账鬼（第 5 关 ghost_cheat）— blockPlayer / canBeatOverride / tryForcePlay
#    对应 C++ tests/test_level_rules.cpp 的赖账鬼三段
# ═══════════════════════════════════════════════════════════════

## 造带 Boss 的赖账鬼局（b 为 Boss，规则=第 5 关）
static func _make_ghost_round(hand_a: Array, hand_b: Array) -> Dictionary:
	var ctx := _make_round(hand_a, hand_b)
	var r: Round = ctx["round"]
	r._boss = ctx["b"]
	r.level_rule = Campaign.level_rule(5)
	return ctx


static func _test_ghost_cheat() -> void:
	# 战力序：7>大鬼>小鬼>5>2>3>A>K>Q>J>10>9>8>6>4，"J 以下" = rank<6（10/9/8/6/4）

	# ① blockPlayer：玩家不能用相同点数压（换牌型也禁）
	#    手工布置：玩家 A 回合，last_play = Boss 出的单张红桃 9
	var ctx := _make_ghost_round([U.card("9", "黑桃"), U.card("K")], [U.card("8", "红桃")])
	var r: Round = ctx["round"]
	r.last_play = CardTypeResult.parse_any_card_type([U.card("9", "红桃")])
	r.last_player = ctx["b"]
	var parsed_same := CardTypeResult.parse_any_card_type([U.card("9", "黑桃")])
	U.ok(r.player_blocked_by_ghost(parsed_same), "同点 9 被禁")
	var parsed_diff := CardTypeResult.parse_any_card_type([U.card("K", "黑桃")])
	U.ok(not r.player_blocked_by_ghost(parsed_diff), "不同点 K 不被禁")
	r._swap()   # 模拟轮到 Boss
	var parsed_boss := CardTypeResult.parse_any_card_type([U.card("9", "梅花")])
	U.ok(not r.player_blocked_by_ghost(parsed_boss), "Boss 出牌不受 blockPlayer 限制")

	# ② canBeatOverride：Boss 同花单张压 J 以下单张（无视点数）
	var ctx2 := _make_ghost_round([U.card("10", "黑桃")], [U.card("8", "黑桃"), U.card("A", "红桃")])
	var r2: Round = ctx2["round"]
	r2.process_play([U.card("10", "黑桃")])                     # 玩家出黑桃 10（rank5 < J）
	var res2: Dictionary = r2.process_play([U.card("8", "黑桃")])  # Boss 黑桃 8：点数不够但同花
	U.eq(res2["state"], "continue", "赖账鬼同花单张压过 J 以下单张")

	# ③ 不同花 → 正常 cannot_beat
	var ctx3 := _make_ghost_round([U.card("10", "黑桃")], [U.card("8", "红桃")])
	var r3: Round = ctx3["round"]
	r3.process_play([U.card("10", "黑桃")])
	var res3: Dictionary = r3.process_play([U.card("8", "红桃")])
	U.eq(res3["state"], "cannot_beat", "不同花正常判压不过")

	# ④ 上家是 J（rank6）及以上 → 特权失效
	var ctx4 := _make_ghost_round([U.card("J", "黑桃")], [U.card("8", "黑桃")])
	var r4: Round = ctx4["round"]
	r4.process_play([U.card("J", "黑桃")])
	var res4: Dictionary = r4.process_play([U.card("8", "黑桃")])
	U.eq(res4["state"], "cannot_beat", "J 及以上单张同花也压不过")

	# ⑤ tryForcePlay：Boss Pass 前找同花单张强压（手工布置 last_play）
	var ctx5 := _make_ghost_round([], [U.card("8", "黑桃"), U.card("A", "红桃")])
	var r5: Round = ctx5["round"]
	r5.last_play = CardTypeResult.parse_any_card_type([U.card("10", "黑桃")])
	r5.last_player = ctx5["a"]
	var forced: Array[Card] = r5.ghost_force_play(ctx5["b"].normal_hand)
	U.eq(forced.size(), 1, "tryForcePlay 找到同花单张")
	if forced.size() == 1:
		U.eq(forced[0].suit, "黑桃", "强压牌是同花色")
	var ctx6 := _make_ghost_round([], [U.card("8", "黑桃")])
	var r6: Round = ctx6["round"]
	r6.last_play = CardTypeResult.parse_any_card_type([U.card("J", "黑桃")])
	r6.last_player = ctx6["a"]
	U.eq(r6.ghost_force_play(ctx6["b"].normal_hand).size(), 0, "上家 ≥J 时强压不触发")
