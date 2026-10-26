class_name Score
extends RefCounted

# ═══════════════════════════════════════════════════════════════
# Score — 计分模块
#
# 后续统一数值体系：
#   分（对局比分）、筹码（商店货币）、HP（血量）
#   三者最终统一为单一数值类型，各模块从本文件取语义别名。
#   现阶段各自独立，改数值只改这里。
# ═══════════════════════════════════════════════════════════════


## 计算一手牌的原始分数（只累加 card.score）
static func calculate_score(cards: Array[Card]) -> int:
	var total := 0
	for card in cards:
		total += card.score
	return total


# ═══════════════════════════════════════════════════════════════
# 统一换算：分 → 伤害 → HP → 筹码
#
# 分与伤害、伤害与 HP 当前 1:1；筹码锚点为 3 筹码 = 2 HP
# （由小回复药水 30 筹码 / 20 HP 反推）。
# 所有跨维度换算集中在此，平衡调整只改本文件。
# ═══════════════════════════════════════════════════════════════

const CHIPS_PER_HP_NUM := 3
const CHIPS_PER_HP_DEN := 2


## 桌面分 → 小局伤害（附魔加分等修正的插入点）
static func score_to_damage(score: int) -> int:
	return score


## 小局伤害 → 实际 HP 扣减
static func damage_to_hp(damage: int) -> int:
	return damage


## HP 等价筹码
static func hp_to_chips(hp: int) -> int:
	return int(ceil(float(hp) * CHIPS_PER_HP_NUM / CHIPS_PER_HP_DEN))


## 筹码等价 HP
static func chips_to_hp(chips: int) -> int:
	return int(chips * CHIPS_PER_HP_DEN / CHIPS_PER_HP_NUM)