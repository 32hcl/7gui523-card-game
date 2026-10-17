class_name CardTypeNames
extends RefCounted

# ═══════════════════════════════════════════════════════════════
# CardTypeNames — 牌型中文名单一来源
#
# 规则层（cardtype.gd 调试输出）与 UI 层（play.gd 牌桌标签）统一调用，
# 消除原先两处各自维护、文案不一致（单张/单牌、王炸/火箭…）的映射。
# 参数刻意用 int（CardTypeResult.CardType 枚举值）而非枚举类型，
# 避免 global 对 core 形成类型环依赖；match 分支内只读取枚举常量做比较。
# ═══════════════════════════════════════════════════════════════


static func name_for(t: int) -> String:
	match t:
		CardTypeResult.CardType.PASS:
			return "不出"
		CardTypeResult.CardType.SINGLE:
			return "单张"
		CardTypeResult.CardType.PAIR:
			return "对子"
		CardTypeResult.CardType.TRIPLE:
			return "三张"
		CardTypeResult.CardType.TRIPLE_WITH_ONE:
			return "三带一"
		CardTypeResult.CardType.TRIPLE_WITH_TWO:
			return "三带二"
		CardTypeResult.CardType.BOMB:
			return "炸弹"
		CardTypeResult.CardType.FIVE_OF_A_KIND:
			return "五同点"
		CardTypeResult.CardType.ROCKET:
			return "王炸"
		CardTypeResult.CardType.GUI523:
			return "7gui523"
		CardTypeResult.CardType.INVALID:
			return "非法"
		_:
			return "未知"
