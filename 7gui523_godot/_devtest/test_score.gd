extends SceneTree

# ═══════════════════════════════════════════════════════════════
# TestScore — 牌面基础筹码 + 压牌筹码
#
# 数值统一后：分 = 伤害 = HP = 筹码（1:1）。
#   基础筹码：5/10/K = 5/10/20，其他 = 0
#   压牌筹码：牌型 + 主点数都相同时，按每张牌的 PRESSURE_CHIPS 累加
#   特殊牌型（炸弹/王炸/五同点/7gui523）只要压得过就按每张牌的 PRESSURE_CHIPS 累加
# ═══════════════════════════════════════════════════════════════

const U := preload("res://_devtest/test_util.gd")


func _init() -> void:
	var failed := _run()
	quit(1 if failed > 0 else 0)


static func _run() -> int:
	var failed := 0
	print("=".repeat(60))
	print("  筹码与压牌测试 — test_score.gd")
	print("=".repeat(60))

	failed += U.section("calculateScore（基础筹码）", func(): _test_calculate_score())
	failed += U.section("pressureChips（同牌型同点）", func(): _test_pressure_chips())
	failed += U.section("pressureChips 边界", func(): _test_pressure_boundaries())
	failed += U.section("特殊牌型压牌筹码", func(): _test_special_pressure())

	return U.finish(failed, "筹码")


# ═══════════════════════════════════════════════════════════════
# 1. 基础筹码
# ═══════════════════════════════════════════════════════════════

static func _test_calculate_score() -> void:
	U.eq(Score.calculate_score([U.card("5")]), 5, "5=5")
	U.eq(Score.calculate_score([U.card("10")]), 10, "10=10")
	U.eq(Score.calculate_score([U.card("K")]), 20, "K=20")
	U.eq(Score.calculate_score([U.card("A")]), 0, "A=0")
	U.eq(Score.calculate_score([U.card("5"), U.card("10"), U.card("K")]), 35, "5+10+K=35")
	U.eq(Score.calculate_score([U.card("5"), U.card("10"), U.card("K"), U.card("5"), U.card("K")]), 60, "复合=60")
	U.eq(Score.calculate_score([]), 0, "空=0")


# ═══════════════════════════════════════════════════════════════
# 2. 压牌筹码
# ═══════════════════════════════════════════════════════════════

static func _test_pressure_chips() -> void:
	# 单张同点：2/3/5/7=15，4/6/8/9/10/J/Q/K/A 依次 4/6/8/9/10/11/12/13/14，鬼=20
	U.eq(CardTypeResult.calculate_pressure_chips(U.s("2"), U.s("2")), 15, "2压2=15")
	U.eq(CardTypeResult.calculate_pressure_chips(U.s("3"), U.s("3")), 15, "3压3=15")
	U.eq(CardTypeResult.calculate_pressure_chips(U.s("4"), U.s("4")), 4, "4压4=4")
	U.eq(CardTypeResult.calculate_pressure_chips(U.s("5"), U.s("5")), 15, "5压5=15")
	U.eq(CardTypeResult.calculate_pressure_chips(U.s("6"), U.s("6")), 6, "6压6=6")
	U.eq(CardTypeResult.calculate_pressure_chips(U.s("7"), U.s("7")), 15, "7压7=15")
	U.eq(CardTypeResult.calculate_pressure_chips(U.s("8"), U.s("8")), 8, "8压8=8")
	U.eq(CardTypeResult.calculate_pressure_chips(U.s("9"), U.s("9")), 9, "9压9=9")
	U.eq(CardTypeResult.calculate_pressure_chips(U.s("10"), U.s("10")), 10, "10压10=10")
	U.eq(CardTypeResult.calculate_pressure_chips(U.s("J"), U.s("J")), 11, "J压J=11")
	U.eq(CardTypeResult.calculate_pressure_chips(U.s("Q"), U.s("Q")), 12, "Q压Q=12")
	U.eq(CardTypeResult.calculate_pressure_chips(U.s("K"), U.s("K")), 13, "K压K=13")
	U.eq(CardTypeResult.calculate_pressure_chips(U.s("A"), U.s("A")), 14, "A压A=14")
	U.eq(CardTypeResult.calculate_pressure_chips(U.s("大鬼"), U.s("大鬼")), 20, "大鬼压大鬼=20")
	U.eq(CardTypeResult.calculate_pressure_chips(U.s("小鬼"), U.s("小鬼")), 20, "小鬼压小鬼=20")

	# 多张同点：每张累加
	U.eq(CardTypeResult.calculate_pressure_chips(U.pair("5"), U.pair("5")), 30, "对5压对5=30")
	U.eq(CardTypeResult.calculate_pressure_chips(U.pair("10"), U.pair("10")), 20, "对10压对10=20")
	U.eq(CardTypeResult.calculate_pressure_chips(U.pair("K"), U.pair("K")), 26, "对K压对K=26")
	U.eq(CardTypeResult.calculate_pressure_chips(U.ptrips("7"), U.ptrips("7")), 45, "三7压三7=45")

	# 三带二：5 张都按各自压牌值累加（7*3 + 5*2 = 45 + 30）
	var t1 := CardTypeResult.parse_card_type([U.card("7"), U.card("7"), U.card("7"), U.card("5"), U.card("5")])
	var t2 := CardTypeResult.parse_card_type([U.card("7"), U.card("7"), U.card("7"), U.card("5"), U.card("5")])
	U.eq(CardTypeResult.calculate_pressure_chips(t2, t1), 75, "77755压77755=75")


# ═══════════════════════════════════════════════════════════════
# 3. 边界
# ═══════════════════════════════════════════════════════════════

static func _test_pressure_boundaries() -> void:
	var empty := CardTypeResult.new()
	U.eq(CardTypeResult.calculate_pressure_chips(U.s("5"), empty), 0, "空上家=0")

	var pass_res := CardTypeResult.new()
	pass_res.type = CardTypeResult.CardType.PASS
	U.eq(CardTypeResult.calculate_pressure_chips(U.s("5"), pass_res), 0, "上家PASS=0")

	U.eq(CardTypeResult.calculate_pressure_chips(U.s("10"), U.s("5")), 0, "同牌型不同点=0")
	U.eq(CardTypeResult.calculate_pressure_chips(U.s("大鬼"), U.s("小鬼")), 0, "大鬼压小鬼不同点=0")
	U.eq(CardTypeResult.calculate_pressure_chips(U.pair("5"), U.s("5")), 0, "对子压单张不同牌型=0")
	U.eq(CardTypeResult.calculate_pressure_chips(U.s("5"), U.pair("5")), 0, "单张压对子不同牌型=0")


# ═══════════════════════════════════════════════════════════════
# 4. 特殊牌型（炸弹/王炸/五同点）压牌筹码
# ═══════════════════════════════════════════════════════════════

static func _test_special_pressure() -> void:
	# 炸弹 5：4 张 × 15 = 60，可跨牌型压单张
	U.eq(CardTypeResult.calculate_pressure_chips(U.pbomb("5"), U.s("9")), 60, "炸弹5压单张=60")
	# 炸弹 K：4 张 × 13 = 52，可跨牌型压对子
	U.eq(CardTypeResult.calculate_pressure_chips(U.pbomb("K"), U.pair("A")), 52, "炸弹K压对子=52")
	# 王炸：大鬼 20 + 小鬼 20 = 40
	U.eq(CardTypeResult.calculate_pressure_chips(U.rocket(), U.ptrips("7")), 40, "王炸压三张=40")
	# 五同点 5：5 张 × 15 = 75，可跨牌型压炸弹
	var five5 := CardTypeResult.parse_five_of_a_kind(U.five("5", 5))
	U.eq(CardTypeResult.calculate_pressure_chips(five5, U.pbomb("A")), 75, "五同点5压炸弹=75")
	# 特殊牌互相压制：按自身每张牌的压牌值
	U.eq(CardTypeResult.calculate_pressure_chips(U.pbomb("6"), U.pbomb("4")), 24, "炸弹6压炸弹4=24")
	# 压不过时不计（炸弹 3 压不过王炸）
	U.eq(CardTypeResult.calculate_pressure_chips(U.pbomb("3"), U.rocket()), 0, "压不过不计压牌筹码")
	# 7gui523（7/大鬼/5/2/3）= 15+20+15+15+15 = 80
	U.eq(CardTypeResult.calculate_pressure_chips(U.gui523(), U.s("A")), 80, "7gui523压单张=80")