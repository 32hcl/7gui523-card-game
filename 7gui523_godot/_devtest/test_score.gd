extends SceneTree

# ═══════════════════════════════════════════════════════════════
# TestScore — 计分与压迫加分层（对应 C++ tests/test_score.cpp）
# 覆盖：AllOrNothing压迫加分 / calculateScore /
#       pressureBonus原始版（对照 C++ test_pressureBonus）
# 注：原 tableScore 静态函数已并入 Round（table_score 累积 + table_bonus），
#     其行为由 test_ai_round.gd 的回合链路断言覆盖。
# 依赖 Card + CardTypeResult + Score，纯逻辑。
#
# 命令行：
#   godot --headless --path . --script _devtest/test_score.gd
# ═══════════════════════════════════════════════════════════════

const U := preload("res://_devtest/test_util.gd")


func _init() -> void:
	var failed := _run()
	quit(1 if failed > 0 else 0)


static func _run() -> int:
	var failed := 0
	print("=".repeat(60))
	print("  计分与压迫加分测试 — test_score.gd")
	print("=".repeat(60))

	failed += U.section("pressureBonus AllOrNothing", func(): _test_pressure_bonus())
	failed += U.section("calculateScore", func(): _test_calculate_score())
	failed += U.section("pressureBonus original", func(): _test_pressure_bonus_original())

	return U.finish(failed, "计分")


# ═══════════════════════════════════════════════════════════════
# 1. pressureBonus AllOrNothing
# ═══════════════════════════════════════════════════════════════

static func _test_pressure_bonus() -> void:
	# 全部是分牌 → 触发
	U.eq(CardTypeResult.calculate_pressure_bonus(U.s("5"), U.s("5")), 5, "单5压单5=5")
	U.eq(CardTypeResult.calculate_pressure_bonus(U.s("K"), U.s("K")), 20, "单K压单K=20")
	U.eq(CardTypeResult.calculate_pressure_bonus(U.pair("5"), U.pair("5")), 10, "对5压对5=10")
	U.eq(CardTypeResult.calculate_pressure_bonus(U.pair("10"), U.pair("10")), 20, "对10压对10=20")
	U.eq(CardTypeResult.calculate_pressure_bonus(U.pair("K"), U.pair("K")), 40, "对K压对K=40")

	# 不同点数不奖励
	U.eq(CardTypeResult.calculate_pressure_bonus(U.s("10"), U.s("5")), 0, "10压5不同点=0")
	# 无分牌不触发
	U.eq(CardTypeResult.calculate_pressure_bonus(U.s("7"), U.s("7")), 0, "7压7=0")

	# 三带二 77755：含非分牌 7 → 不触发
	var t1 := CardTypeResult.parse_card_type([U.card("7"), U.card("7"), U.card("7"), U.card("5"), U.card("5")])
	var t2 := CardTypeResult.parse_card_type([U.card("7"), U.card("7"), U.card("7"), U.card("5"), U.card("5")])
	U.eq(CardTypeResult.calculate_pressure_bonus(t2, t1), 0, "77755不全分=0")

	# 不同牌型不奖励（单张→对子）
	U.eq(CardTypeResult.calculate_pressure_bonus(U.pair("5"), U.s("5")), 0, "对子不压单张不加分")


# ═══════════════════════════════════════════════════════════════
# 2. calculateScore
# ═══════════════════════════════════════════════════════════════

static func _test_calculate_score() -> void:
	U.eq(Score.calculate_score([U.card("5")]), 5, "5=5分")
	U.eq(Score.calculate_score([U.card("10")]), 10, "10=10分")
	U.eq(Score.calculate_score([U.card("K")]), 20, "K=20分")
	U.eq(Score.calculate_score([U.card("A")]), 0, "A=0分")
	U.eq(Score.calculate_score([U.card("5"), U.card("10"), U.card("K")]), 35, "5+10+K=35")
	U.eq(Score.calculate_score([U.card("5"), U.card("10"), U.card("K"), U.card("5"), U.card("K")]), 60, "复合=60")
	U.eq(Score.calculate_score([]), 0, "空=0")


# ═══════════════════════════════════════════════════════════════
# 3. pressureBonus（原始版，对照 C++ test_pressureBonus）
# ═══════════════════════════════════════════════════════════════

static func _test_pressure_bonus_original() -> void:
	# 空上家 → 0
	var empty := CardTypeResult.new()
	U.eq(CardTypeResult.calculate_pressure_bonus(U.s("5"), empty), 0, "空上家=0")
	# 同点同型递进
	U.eq(CardTypeResult.calculate_pressure_bonus(U.s("5"), U.s("5")), 5, "5-5=5")
	U.eq(CardTypeResult.calculate_pressure_bonus(U.s("10"), U.s("10")), 10, "10-10=10")
	U.eq(CardTypeResult.calculate_pressure_bonus(U.s("K"), U.s("K")), 20, "K-K=20")
	U.eq(CardTypeResult.calculate_pressure_bonus(U.pair("5"), U.pair("5")), 10, "55-55=10")
	U.eq(CardTypeResult.calculate_pressure_bonus(U.pair("10"), U.pair("10")), 20, "1010-1010=20")
	U.eq(CardTypeResult.calculate_pressure_bonus(U.pair("K"), U.pair("K")), 40, "KK-KK=40")
	# 不同点不奖励
	U.eq(CardTypeResult.calculate_pressure_bonus(U.s("10"), U.s("5")), 0, "10-5=0")
	U.eq(CardTypeResult.calculate_pressure_bonus(U.s("7"), U.s("7")), 0, "7-7=0")
	# 混分不触发
	var t1 := CardTypeResult.parse_card_type([U.card("7"), U.card("7"), U.card("7"), U.card("5"), U.card("5")])
	var t2 := CardTypeResult.parse_card_type([U.card("7"), U.card("7"), U.card("7"), U.card("5"), U.card("5")])
	U.eq(CardTypeResult.calculate_pressure_bonus(t2, t1), 0, "77755=0")
	# 不同牌型不奖励
	U.eq(CardTypeResult.calculate_pressure_bonus(U.pair("5"), U.s("5")), 0, "对子-单张=0")