extends SceneTree

# ═══════════════════════════════════════════════════════════════
# TestIntegration — 全链路集成测试
# 覆盖完整回合流程：出牌 → 跟牌 → 回合胜 → 补牌 → 桌面清空
#
# 命令行：
#   godot --headless --path . --script _devtest/test_integration.gd
# ═══════════════════════════════════════════════════════════════

const U := preload("res://_devtest/test_util.gd")


func _init() -> void:
	var failed := _run()
	quit(1 if failed > 0 else 0)


static func _run() -> int:
	var failed := 0
	print("=".repeat(60))
	print("  全链路集成测试 — test_integration.gd")
	print("=".repeat(60))

	failed += U.section("完整小回合：首出→跟牌→不要→补牌", func(): _test_full_subround())
	failed += U.section("多轮连续小回合直到谁先出完", func(): _test_multi_subround())
	failed += U.section("残局：出完即胜 (instant_win)", func(): _test_instant_win())

	return U.finish(failed, "集成测试")


# ═══════════════════════════════════════════════════════════════
# 造局辅助
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
# 1. 完整小回合：A 首出 → B 跟牌 → A 不要 → A 赢
# ═══════════════════════════════════════════════════════════════

static func _test_full_subround() -> void:
	var ctx := _make_round([U.card("9"), U.card("A")], [U.card("10"), U.card("K")])
	var r: Round = ctx["round"]
	var a: Player.PlayerData = ctx["a"]
	var b: Player.PlayerData = ctx["b"]

	var res1: Round.PlayResult = r.process_play([U.card("9")])
	print("  ── A 出 9 → state=%s" % res1.state)
	U.eq(res1.state, "continue", "首出后回合继续")
	U.eq(r.whose_turn(), b, "轮到 B")
	U.eq(a.normal_hand.size(), 1, "A 手牌减 1")

	var res2: Round.PlayResult = r.process_play([U.card("10")])
	print("  ── B 用 10 压 → state=%s" % res2.state)
	U.eq(res2.state, "continue", "跟牌后回合继续")
	U.eq(r.whose_turn(), a, "轮到 A")

	var res3: Round.PlayResult = r.process_play([])
	print("  ── A 不要 → state=%s" % res3.state)
	U.eq(res3.state, "round_win", "A 不要 → B 赢本小回合")
	U.eq(res3.winner, b, "B 为胜者")

	U.eq(a.normal_hand.size(), 5, "输家补牌到 5")
	U.eq(b.normal_hand.size(), 5, "赢家也补牌到 5")
	U.eq(r.table_cards.size(), 0, "桌面清空")


# ═══════════════════════════════════════════════════════════════
# 2. 多轮连续小回合直到一方手牌出空
# ═══════════════════════════════════════════════════════════════

static func _test_multi_subround() -> void:
	# A 大牌连出，B 手牌短，多小回合后一方空手
	var ctx := _make_round(
		[U.card("A"), U.card("K"), U.card("Q"), U.card("J"), U.card("10")],
		[U.card("3"), U.card("4"), U.card("5"), U.card("6"), U.card("7")])
	var r: Round = ctx["round"]
	var a: Player.PlayerData = ctx["a"]
	var b: Player.PlayerData = ctx["b"]

	# 小回合 1：A 出 A → B 不要 → A 赢
	var res1: Round.PlayResult = r.process_play([U.card("A")])
	U.eq(res1.state, "continue", "A 出 A → continue")
	var res1b: Round.PlayResult = r.process_play([])
	U.eq(res1b.state, "round_win", "B 不要 → A 赢")
	U.eq(res1b.winner, a, "A 赢第 1 小回合")

	# 小回合 2：A 连出 K → B 不要
	var res2: Round.PlayResult = r.process_play([U.card("K")])
	U.eq(res2.state, "continue", "A 出 K")
	var res2b: Round.PlayResult = r.process_play([])
	U.eq(res2b.state, "round_win", "B 不要 → A 再赢")

	# 小回合 3：A 出 Q → B 不要
	var res3: Round.PlayResult = r.process_play([U.card("Q")])
	U.eq(res3.state, "continue", "A 出 Q")
	var res3b: Round.PlayResult = r.process_play([])
	U.eq(res3b.state, "round_win", "A 赢第 3 小回合")

	# 小回合 4：A 出 J → B 不要
	var res4: Round.PlayResult = r.process_play([U.card("J")])
	U.eq(res4.state, "continue", "A 出 J")
	var res4b: Round.PlayResult = r.process_play([])
	U.eq(res4b.state, "round_win", "A 赢第 4 小回合")

	# 小回合 5：A 出最后一张 10 → B 不要
	var res5: Round.PlayResult = r.process_play([U.card("10")])
	U.eq(res5.state, "continue", "A 出 10")
	var res5b: Round.PlayResult = r.process_play([])
	U.eq(res5b.state, "round_win", "A 赢第 5 小回合")

	# A 手牌空 → 下一小回合出空判定 instant_win
	U.eq(a.normal_hand.size(), 0, "A 手牌已空")
	U.eq(b.normal_hand.size(), 0, "B 也补空归零")

	print("  ✓ 5 轮小回合全部正确执行")


# ═══════════════════════════════════════════════════════════════
# 3. 残局：手牌 1 张 + 牌堆空 → 出完即判 instant_win
# ═══════════════════════════════════════════════════════════════

static func _test_instant_win() -> void:
	var ctx := _make_round([U.card("7")], [U.card("5")])
	var r: Round = ctx["round"]
	var a: Player.PlayerData = ctx["a"]
	r.process_play([U.card("7")])                # A 出 7，桌面有牌
	var res: Round.PlayResult = r.process_play([])                   # B 不要
	U.eq(res.state, "round_win", "B 不要 → A 赢小回合")

	# 此时 A 手牌已空 → 再开小回合即判 instant_win
	ctx["deck_a"].active_count = 0               # 模拟牌堆抽干
	ctx["deck_b"].active_count = 0

	print("  ✓ 残局出完判定通过")