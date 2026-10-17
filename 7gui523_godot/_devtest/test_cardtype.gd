extends SceneTree

# ═══════════════════════════════════════════════════════════════
# TestCardType — 牌型规则层（对应 C++ tests/test_cardtype.cpp）
# 覆盖：牌型解析 / canBeat(同牌型规则) / typeRank全覆盖 / pass&空
# 只依赖 Card + CardTypeResult，纯逻辑，不触碰存储层。
#
# 命令行：
#   godot --headless --path . --script _devtest/test_cardtype.gd
# ═══════════════════════════════════════════════════════════════

const U := preload("res://_devtest/test_util.gd")


func _init() -> void:
	var failed := _run()
	quit(1 if failed > 0 else 0)


static func _run() -> int:
	var failed := 0
	print("=".repeat(60))
	print("  牌型规则测试 — test_cardtype.gd")
	print("=".repeat(60))

	failed += U.section("parseCardType", func(): _test_parse_card_type())
	failed += U.section("canBeat (同牌型规则)", func(): _test_can_beat())
	failed += U.section("typeRank_full", func(): _test_type_rank_full())
	failed += U.section("pass & empty", func(): _test_pass_and_empty())

	return U.finish(failed, "牌型规则")


# ═══════════════════════════════════════════════════════════════
# 1. parseCardType
# ═══════════════════════════════════════════════════════════════

static func _test_parse_card_type() -> void:
	var C := CardTypeResult.CardType

	U.eq(U.type_of([U.card("大鬼")]), C.SINGLE, "大鬼=单张")
	U.eq(U.type_of([U.card("K")]), C.SINGLE, "K=单张")
	U.eq(U.type_of(U.pairs("5")), C.PAIR, "55=对子")
	U.eq(U.type_of(U.pairs("K")), C.PAIR, "KK=对子")
	U.eq(U.type_of([U.card("5"), U.card("6")]), C.INVALID, "56=非法")
	U.eq(U.type_of([U.card("大鬼"), U.card("小鬼")]), C.ROCKET, "大鬼+小鬼=王炸")
	U.eq(U.type_of([U.card("大鬼"), U.card("大鬼")]), C.INVALID, "双大鬼≠对子")
	U.eq(U.type_of(U.trips("7")), C.TRIPLE, "777=三张")

	var tw1 := CardTypeResult.parse_card_type([U.card("7", "黑桃"), U.card("7", "红桃"), U.card("7", "梅花"), U.card("3")])
	U.eq(tw1.type, C.TRIPLE_WITH_ONE, "7773=三带一")
	U.eq(tw1.key_point, "7", "三带一主点数")
	U.eq(tw1.kicker, "3", "三带一带牌")

	var tw2 := CardTypeResult.parse_card_type([U.card("7"), U.card("7"), U.card("7"), U.card("5"), U.card("5")])
	U.eq(tw2.type, C.TRIPLE_WITH_TWO, "77755=三带二")
	U.eq(tw2.key_point, "7", "三带二主点数")
	U.eq(tw2.kicker, "5", "三带二带对子")

	U.eq(U.type_of([U.card("7"), U.card("7"), U.card("7"), U.card("5"), U.card("6")]), C.INVALID, "77756=非法")
	U.eq(U.type_of([U.card("7"), U.card("7"), U.card("5"), U.card("5"), U.card("3")]), C.INVALID, "77553=非法")
	U.eq(U.type_of([U.card("7"), U.card("7"), U.card("7"), U.card("7"), U.card("3")]), C.INVALID, "77773=非法")
	U.eq(U.type_of(U.bomb("7")), C.BOMB, "7777=炸弹")

	var bomb := CardTypeResult.parse_card_type(U.bomb("7"))
	U.eq(bomb.type, C.BOMB, "炸弹类型")
	U.eq(bomb.count, 4, "炸弹count=4")

	var five5 := CardTypeResult.parse_five_of_a_kind(U.five("7", 5))
	U.eq(five5.type, C.FIVE_OF_A_KIND, "5张7=五同点")
	U.eq(five5.count, 5, "五同点count=5")

	var five6 := CardTypeResult.parse_five_of_a_kind(U.five("7", 6))
	U.eq(five6.type, C.FIVE_OF_A_KIND, "6张7=五同点")
	U.eq(five6.count, 6, "六张count=6")

	var triple := CardTypeResult.parse_card_type(U.trips("7"))
	U.eq(triple.type, C.TRIPLE, "777=三张")
	U.eq(triple.count, 0, "三张count=0")


# ═══════════════════════════════════════════════════════════════
# 2. canBeat (同牌型规则)
# ═══════════════════════════════════════════════════════════════

static func _test_can_beat() -> void:
	var single7 := U.s("7")
	var single5 := U.s("5")
	U.ok(CardTypeResult.can_beat(single7, single5), "7压5")
	U.ok(not CardTypeResult.can_beat(single5, single7), "5不压7")
	U.ok(CardTypeResult.can_beat(single7, single7), "同牌可自压")

	var big := U.s("大鬼")
	var small := U.s("小鬼")
	U.ok(CardTypeResult.can_beat(big, small), "大鬼压小鬼")
	U.ok(not CardTypeResult.can_beat(small, big), "小鬼不压大鬼")

	# 同牌型规则：对子不能压单张（须同牌型）
	var pair := U.pair("7")
	U.ok(not CardTypeResult.can_beat(pair, single5), "对子不压单张(同牌型)")
	U.ok(CardTypeResult.can_beat(U.pair("5"), U.pair("8")), "对5压对8")

	var bomb4 := U.pbomb("4")
	U.ok(CardTypeResult.can_beat(bomb4, U.s("7")), "炸弹压单张")
	U.ok(CardTypeResult.can_beat(bomb4, U.pair("7")), "炸弹压对子")
	U.ok(CardTypeResult.can_beat(bomb4, U.ptrips("7")), "炸弹压三张")

	var rocket := U.rocket()
	var bomb7 := U.pbomb("7")
	U.ok(CardTypeResult.can_beat(rocket, bomb7), "王炸压炸弹")
	U.ok(not CardTypeResult.can_beat(bomb7, rocket), "炸弹不压王炸")
	U.ok(CardTypeResult.can_beat(rocket, rocket), "王炸可自压")

	var ttw := CardTypeResult.parse_card_type([U.card("8"), U.card("8"), U.card("8"), U.card("4"), U.card("4")])
	var bomb3 := U.pbomb("3")
	U.ok(CardTypeResult.can_beat(bomb3, ttw), "炸弹压三带二")

	var bomb5 := CardTypeResult.parse_five_of_a_kind(U.five("3", 5))
	U.ok(CardTypeResult.can_beat(bomb5, rocket), "五同点压王炸")
	U.ok(not CardTypeResult.can_beat(rocket, bomb5), "王炸不压五同点")

	var bomb4x5 := U.pbomb("5")
	U.ok(CardTypeResult.can_beat(rocket, bomb4x5), "王炸压4炸弹5")

	var bomb5x3 := CardTypeResult.parse_five_of_a_kind(U.five("3", 5))
	U.ok(CardTypeResult.can_beat(bomb5x3, bomb4x5), "五同点3压4炸弹5")
	U.ok(not CardTypeResult.can_beat(bomb4x5, bomb5x3), "4炸弹5不压五同点3")

	var bomb5x5 := CardTypeResult.parse_five_of_a_kind(U.five("5", 5))
	U.ok(CardTypeResult.can_beat(bomb5x5, bomb5x3), "五同点5压五同点3")
	U.ok(not CardTypeResult.can_beat(bomb5x3, bomb5x5), "五同点3不压五同点5")

	var gui := U.gui523()
	U.ok(CardTypeResult.can_beat(gui, bomb5), "7gui523压五同点")
	U.ok(not CardTypeResult.can_beat(bomb5, gui), "五同点不压7gui523")

	# 三带一 kicker：555A 压 555K
	var tw_K := CardTypeResult.parse_card_type([U.card("5"), U.card("5"), U.card("5"), U.card("K")])
	var tw_A := CardTypeResult.parse_card_type([U.card("5"), U.card("5"), U.card("5"), U.card("A")])
	U.eq(tw_K.kicker, "K", "三带一kicker=K")
	U.eq(tw_A.kicker, "A", "三带一kicker=A")
	U.ok(CardTypeResult.can_beat(tw_A, tw_K), "555A压555K")
	U.ok(not CardTypeResult.can_beat(tw_K, tw_A), "555K不压555A")

	# 三带一：三条不同直接比三条，222>333
	var t222A := CardTypeResult.parse_card_type([U.card("2"), U.card("2"), U.card("2"), U.card("A")])
	var t333K := CardTypeResult.parse_card_type([U.card("3"), U.card("3"), U.card("3"), U.card("K")])
	U.ok(CardTypeResult.can_beat(t222A, t333K), "222A压333K")

	# 三带一完全相同可压
	var t1 := CardTypeResult.parse_card_type([U.card("8"), U.card("8"), U.card("8"), U.card("4")])
	var t2 := CardTypeResult.parse_card_type([U.card("8"), U.card("8"), U.card("8"), U.card("4")])
	U.ok(CardTypeResult.can_beat(t1, t2), "完全相同可压")

	# 三带二 kicker：888A压 888K
	var ttw888K := CardTypeResult.parse_card_type([U.card("8"), U.card("8"), U.card("8"), U.card("K"), U.card("K")])
	var ttw888A := CardTypeResult.parse_card_type([U.card("8"), U.card("8"), U.card("8"), U.card("A"), U.card("A")])
	U.eq(ttw888K.kicker, "K", "三带二kicker=K")
	U.eq(ttw888A.kicker, "A", "三带二kicker=A")
	U.ok(CardTypeResult.can_beat(ttw888A, ttw888K), "888AA压888KK")
	U.ok(not CardTypeResult.can_beat(ttw888K, ttw888A), "888KK不压888AA")


# ═══════════════════════════════════════════════════════════════
# 3. typeRank_full — 各牌型间压制关系全覆盖
# ═══════════════════════════════════════════════════════════════

static func _test_type_rank_full() -> void:
	var single   := U.s("7")
	var pair     := U.pair("7")
	var triple   := U.ptrips("7")
	var tOne     := CardTypeResult.parse_card_type([U.card("7"), U.card("7"), U.card("7"), U.card("4")])
	var tTwo     := CardTypeResult.parse_card_type([U.card("7"), U.card("7"), U.card("7"), U.card("4"), U.card("4")])
	var bomb     := U.pbomb("7")
	var rocket   := U.rocket()
	var fiveKind := CardTypeResult.parse_five_of_a_kind(U.five("7", 5))
	var gui      := U.gui523()

	# PASS 不参与压制
	var pass_res := CardTypeResult.new()
	pass_res.type = CardTypeResult.CardType.PASS
	U.ok(not CardTypeResult.can_beat(pass_res, single), "PASS不压任何牌")
	U.ok(CardTypeResult.can_beat(single, pass_res), "任何牌能压PASS")

	# 普通牌型：必须同牌型才能压过
	U.ok(CardTypeResult.can_beat(U.pair("5"), U.pair("8")), "对5压对8(点数大)")
	U.ok(not CardTypeResult.can_beat(U.pair("8"), U.pair("5")), "对8不压对5")
	# 跨牌型（普通对普通）不能压过
	U.ok(not CardTypeResult.can_beat(U.ptrips("7"), pair), "三张不压对子")
	U.ok(not CardTypeResult.can_beat(pair, single), "对子不压单张")
	U.ok(not CardTypeResult.can_beat(tOne, pair), "三带一不压对子")
	U.ok(not CardTypeResult.can_beat(tOne, triple), "三带一不压三张")
	U.ok(not CardTypeResult.can_beat(tTwo, tOne), "三带二不压三带一")

	# 特殊牌型：可跨型压普通，且互相按等级分
	U.ok(CardTypeResult.can_beat(bomb, single), "炸弹压单张")
	U.ok(CardTypeResult.can_beat(bomb, pair), "炸弹压对子")
	U.ok(CardTypeResult.can_beat(rocket, single), "王炸压单张")
	U.ok(CardTypeResult.can_beat(rocket, tTwo), "王炸压三带二")
	U.ok(CardTypeResult.can_beat(rocket, bomb), "王炸压炸弹")
	U.ok(not CardTypeResult.can_beat(bomb, rocket), "炸弹不压王炸")
	U.ok(CardTypeResult.can_beat(rocket, rocket), "王炸可自压")
	U.ok(CardTypeResult.can_beat(fiveKind, rocket), "五同点压王炸")
	U.ok(CardTypeResult.can_beat(fiveKind, bomb), "五同点压炸弹")
	U.ok(CardTypeResult.can_beat(fiveKind, single), "五同点压单张")
	U.ok(CardTypeResult.can_beat(gui, fiveKind), "7gui523压五同点")
	U.ok(CardTypeResult.can_beat(gui, rocket), "7gui523压王炸")
	U.ok(CardTypeResult.can_beat(gui, bomb), "7gui523压炸弹")
	U.ok(CardTypeResult.can_beat(gui, single), "7gui523压单张")

	# 普通三带二不压王炸（普通压不过特殊）
	U.ok(not CardTypeResult.can_beat(tTwo, rocket), "三带二不压王炸")
	# 炸弹同型比点数（7gui523战力序：5=12 > 8=3）
	U.ok(CardTypeResult.can_beat(U.pbomb("5"), U.pbomb("8")), "炸弹5比点大于炸弹8")
	U.ok(not CardTypeResult.can_beat(U.pbomb("8"), U.pbomb("5")), "炸弹8不大于炸弹5")


# ═══════════════════════════════════════════════════════════════
# 4. pass & empty
# ═══════════════════════════════════════════════════════════════

static func _test_pass_and_empty() -> void:
	var single7 := U.s("7")

	# 空牌（默认构造）= 上家空 → 能压
	var empty := CardTypeResult.new()
	U.ok(CardTypeResult.can_beat(single7, empty), "上家空→能压")
	U.eq(CardTypeResult.calculate_pressure_bonus(single7, empty), 0, "上空无压迫加分")

	# 显式 PASS
	var pass_res := CardTypeResult.new()
	pass_res.type = CardTypeResult.CardType.PASS
	U.ok(CardTypeResult.can_beat(single7, pass_res), "上家PASS→能压")
	U.eq(CardTypeResult.calculate_pressure_bonus(single7, pass_res), 0, "上家PASS无加分")

	# 空牌不能压任何牌
	U.ok(not CardTypeResult.can_beat(empty, single7), "空牌不压单张")
	U.ok(not CardTypeResult.can_beat(pass_res, single7), "PASS不压单张")

	# INVALID 不能出
	var invalid := CardTypeResult.parse_card_type([U.card("7"), U.card("6")])
	U.eq(invalid.type, CardTypeResult.CardType.INVALID, "76=非法")
	U.ok(not CardTypeResult.can_beat(invalid, single7), "INVALID不压任何牌")