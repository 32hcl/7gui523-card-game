extends Object

# ═══════════════════════════════════════════════════════════════
# TestUtil — 各测试脚本共享的断言工具 + 造牌辅助
#
# 不 extends SceneTree，仅供其他测试脚本用 preload 引用其静态函数。
# 用法：
#   const U := preload("res://_devtest/test_util.gd")
#
# 对应 C++ tests/test_util.h（断言宏统一收口）
# ═══════════════════════════════════════════════════════════════


static var _err_count := 0


## 断言真
static func ok(cond: bool, msg: String) -> void:
	if cond:
		return
	_err_count += 1
	printerr("  ✗ FAIL: %s" % msg)


## 断言相等
static func eq(a: Variant, b: Variant, desc: String) -> void:
	ok(a == b, "%s (期望 %s, 实际 %s)" % [desc, b, a])


## 节：执行一组用例，返回该组失败数
static func section(title: String, fn: Callable) -> int:
	print("\n==== %s ====" % title)
	var prev := _err_count
	fn.call()
	var n := _err_count - prev
	if n == 0:
		print("  ALL OK")
	else:
		print("  %d FAILED" % n)
	return n


## 汇总：跑完一组节，返回总失败数
static func finish(failed: int, label: String) -> int:
	print("")
	print("Result: %s failed" % failed)
	print("=".repeat(60))
	return failed


# ═══════════════════════════════════════════════════════════════
# 造牌辅助 — 与 cardtype_test.gd 原 _card/_s/_pair/... 一致
# ═══════════════════════════════════════════════════════════════

static func card(point: String, suit: String = "") -> Card:
	var c := Card.new()
	c.point = point
	c.suit = suit
	c.score = Card.score_from_point(point)
	return c


## 解析单张 → CardTypeResult
static func s(point: String) -> CardTypeResult:
	return CardTypeResult.parse_card_type([card(point)])


## 造对子原组
static func pairs(point: String) -> Array[Card]:
	return [card(point), card(point)]


## 对子 → CardTypeResult
static func pair(point: String) -> CardTypeResult:
	return CardTypeResult.parse_card_type(pairs(point))


## 三张原组
static func trips(point: String) -> Array[Card]:
	return [card(point), card(point), card(point)]


## 三张 → CardTypeResult
static func ptrips(point: String) -> CardTypeResult:
	return CardTypeResult.parse_card_type(trips(point))


## 炸弹原组
static func bomb(point: String) -> Array[Card]:
	return [card(point), card(point), card(point), card(point)]


## 炸弹 → CardTypeResult
static func pbomb(point: String) -> CardTypeResult:
	return CardTypeResult.parse_card_type(bomb(point))


## 王炸 → CardTypeResult
static func rocket() -> CardTypeResult:
	return CardTypeResult.parse_card_type([card("大鬼"), card("小鬼")])


## 同点 n 张原组（用于五同点）
static func five(point: String, n: int) -> Array[Card]:
	var arr: Array[Card] = []
	for i in n:
		arr.append(card(point))
	return arr


## 7gui523 组合（7 + 大鬼 + 5 + 2 + 3）
static func gui523() -> CardTypeResult:
	return CardTypeResult.parse_card_type([card("7"), card("大鬼"), card("5"), card("2"), card("3")])


## 仅取牌型 enum
static func type_of(cards: Array[Card]) -> int:
	return CardTypeResult.parse_card_type(cards).type