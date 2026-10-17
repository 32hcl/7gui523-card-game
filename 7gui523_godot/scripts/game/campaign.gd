class_name Campaign
extends RefCounted

# ═══════════════════════════════════════════════════════════════
# Campaign — 关卡模式
# ═══════════════════════════════════════════════════════════════


# ═══════════════════════════════════════════════════════════════
# 配置
# ═══════════════════════════════════════════════════════════════

class CampaignConfig:
	var player_init_hp   := 140
	var player_max_hp    := 210
	var boss_max_hp      := 110
	var total_levels     := 9
	var heal_per_card    := 3


class LevelResult:
	var player_won: bool
	var player_remain_cards: int
	var boss_remain_cards: int
	var player_hp_delta: int
	var boss_hp_delta: int
	var boss_cleared: bool
	var chips_delta: int
	var chips_after: int


# ═══════════════════════════════════════════════════════════════
# 静态数据表
# ═══════════════════════════════════════════════════════════════

## 入场筹码（赌注）
const ENTRY_CHIPS := [0, 1, 5, 10, 20, 50, 100, 200, 400, 800]

## 赌场门票（金币；与每关入场筹码完全独立）
const ENTRY_TICKET_DOLLARS := 5_000_000
const INITIAL_DOLLARS := 50_000_000_000

## 奖励筹码（赢后获得）
const REWARD_CHIPS := [0, 3, 12, 25, 50, 120, 250, 500, 1000, 2000]

## 每关 BOSS 名
const BOSS_NAMES := ["", "新手", "换牌虫", "急眼雀", "疯狗", "赖账鬼", "不识数", "黑老大", "千里眼", "赌场老板"]


## 关卡 与 战役一体化：每关规则（LevelRule）+ 关卡间流程（Campaign）


## ═══════════════════════════════════════════════════════════════
## 每关特殊规则（LevelRule）
## ═══════════════════════════════════════════════════════════════

class LevelRule:
	var boss_always_first := false   ## 急眼雀(3)：每轮 boss 先手
	var boss_bite_damage  := false   ## 疯狗(4)：每张牌 ×3 额外伤害
	var ghost_cheat       := false   ## 赖账鬼(5)：同花可压 + 禁同点
	var blind_mix         := false   ## 不识数(6)：50% 概率蒙混过关
	var boss_steal_max    := false   ## 黑老大(7)：先出完时弃玩家最大牌
	var ban_gui523        := false   ## 赌场老板(9)：禁止打出 7gui523


	## ── 判定方法（规则体在此，Round/UI 仅调度）──

	## 赖账鬼：Boss 用同花色单张，可强压 J 以下的单张（无视点数）
	func ghost_can_force_beat(parsed: CardTypeResult, last_play: CardTypeResult) -> bool:
		if not ghost_cheat:
			return false
		if parsed.type != CardTypeResult.CardType.SINGLE:
			return false
		if last_play == null or last_play.cards.is_empty():
			return false
		if last_play.type != CardTypeResult.CardType.SINGLE:
			return false
		if Card.get_rank(last_play.cards[0].point) >= Card.get_rank("J"):
			return false
		return parsed.cards[0].suit == last_play.cards[0].suit


	## 赖账鬼：禁止玩家用相同点数压牌（任意牌型，key_point 相同即禁）；Boss 自身不受限
	func ghost_block_player(parsed: CardTypeResult, last_play: CardTypeResult, actor_is_boss: bool) -> bool:
		if not ghost_cheat:
			return false
		if actor_is_boss:
			return false
		if last_play == null or last_play.cards.is_empty():
			return false
		return parsed.key_point == last_play.key_point


	## 赖账鬼：Boss Pass 前，若上家是 J 以下单张，用同花色任意单张强压（返回强出的牌，空=不触发）
	func ghost_force_play(boss_hand: Array[Card], last_play: CardTypeResult) -> Array[Card]:
		if not ghost_cheat:
			return []
		if last_play == null or last_play.cards.is_empty():
			return []
		if last_play.type != CardTypeResult.CardType.SINGLE:
			return []
		if Card.get_rank(last_play.cards[0].point) >= Card.get_rank("J"):
			return []
		for c in boss_hand:
			if c.suit == last_play.cards[0].suit:
				return [c]
		return []


	## 不识数：Boss 压不过时，同牌型有 50% 概率蒙混过关
	func blind_mix_may_pass(parsed: CardTypeResult, last_play: CardTypeResult) -> bool:
		if not blind_mix:
			return false
		if last_play == null:
			return false
		return parsed.type == last_play.type and randi() % 2 == 0


	## 黑老大：Boss 先出完时，弃掉输家手中最大的一张牌
	func steal_max_from(loser: Player.PlayerData) -> void:
		if not boss_steal_max:
			return
		if loser == null or loser.hand.is_empty():
			return
		var max_card: Card = null
		for c in loser.hand:
			if max_card == null or Card.get_rank(c.point) > Card.get_rank(max_card.point):
				max_card = c
		if max_card != null:
			loser.remove_cards_from_hand([max_card])


	## 疯狗：Boss 出牌立刻咬噬玩家，每张牌造成 3 点额外伤害
	func bite_damage(num_cards: int) -> int:
		return num_cards * 3 if boss_bite_damage else 0


## 获取关卡规则（工厂：九关特殊规则表）
static func level_rule(level: int) -> LevelRule:
	var r := LevelRule.new()
	match level:
		3: r.boss_always_first = true
		4: r.boss_bite_damage  = true
		5: r.ghost_cheat       = true
		6: r.blind_mix         = true
		7: r.boss_steal_max    = true
		9: r.ban_gui523        = true
	return r


# ═══════════════════════════════════════════════════════════════
# 静态工具
# ═══════════════════════════════════════════════════════════════

static func get_level_entry_chips(level: int) -> int:
	if level < 1 or level > 9:
		return 0
	return ENTRY_CHIPS[level]

static func get_level_reward_chips(level: int) -> int:
	if level < 1 or level > 9:
		return 0
	return REWARD_CHIPS[level]

static func get_boss_name(level: int) -> String:
	if level < 1 or level > 9:
		return "???"
	return BOSS_NAMES[level]


# ═══════════════════════════════════════════════════════════════
# 实例
# ═══════════════════════════════════════════════════════════════

var config: CampaignConfig
var player_hp: int
var boss_hp: int
var current_level: int
var player_chips: int = 50
var dollar: int = INITIAL_DOLLARS
var cleared_level: int
var has_purchased_entry: bool


func _init(cfg: CampaignConfig = CampaignConfig.new()) -> void:
	config = cfg
	start_new()


func start_new() -> void:
	player_hp = config.player_init_hp
	boss_hp = config.boss_max_hp
	current_level = 1
	player_chips = 50
	dollar = INITIAL_DOLLARS
	cleared_level = 0
	has_purchased_entry = false


func load_persistent_state(chips: int, dollars: int, cleared: int, has_entry: bool) -> void:
	player_chips = maxi(0, chips)
	dollar = maxi(0, dollars)
	cleared_level = cleared
	has_purchased_entry = has_entry


## 购买一次赌场门票。门票金币和关卡筹码是两套独立资源。
func purchase_entry_ticket() -> bool:
	if has_purchased_entry:
		return true
	if dollar < ENTRY_TICKET_DOLLARS:
		return false
	dollar -= ENTRY_TICKET_DOLLARS
	has_purchased_entry = true
	return true


func start_level(level: int) -> void:
	current_level = level
	boss_hp = config.boss_max_hp


# ═══════════════════════════════════════════════════════════════
# 关卡 9 — 赌场老板 / 通用结算
# ═══════════════════════════════════════════════════════════════

## 每轮扣血：伤害直接扣给输家
## 第 9 关赌场老板：boss不掉血，所有伤害转给玩家
func apply_round_damage(damage: int, boss_won: bool) -> void:
	if damage <= 0:
		return
	if current_level == 9:
		player_hp -= damage
	elif boss_won:
		player_hp -= damage
	else:
		boss_hp -= damage


# ═══════════════════════════════════════════════════════════════
# 通用结算 / 判定
# ═══════════════════════════════════════════════════════════════

## 关卡结束：玩家先出完→回血，没出完→扣血（余牌 × 3）
func finish_level(player_won: bool, player_remain: int, boss_remain: int) -> LevelResult:
	var lr := LevelResult.new()
	## 第 9 关是不可通关的最终挑战：不能推进为玩家胜利。
	var accepted_player_win := player_won and current_level < config.total_levels
	lr.player_won = accepted_player_win
	lr.boss_cleared = accepted_player_win
	lr.player_remain_cards = player_remain
	lr.boss_remain_cards = boss_remain
	lr.chips_delta = 0
	lr.chips_after = player_chips

	var prev_player_hp := player_hp
	var prev_boss_hp := boss_hp

	var total_remain := player_remain + boss_remain
	var heal_amount := total_remain * config.heal_per_card

	if accepted_player_win:
		player_hp += heal_amount
		var reward := get_level_reward_chips(current_level)
		player_chips += reward
		lr.chips_delta += reward
		if current_level > cleared_level:
			cleared_level = current_level
	else:
		player_hp -= heal_amount

	lr.chips_after = player_chips

	if is_game_over() or current_level >= config.total_levels:
		has_purchased_entry = false

	player_hp = mini(player_hp, config.player_max_hp)
	lr.player_hp_delta = player_hp - prev_player_hp
	lr.boss_hp_delta = boss_hp - prev_boss_hp

	return lr


func is_game_over() -> bool:
	return player_hp <= 0


func is_victory() -> bool:
	## 最后一关是不可通关挑战，不产生“全部通关”状态。
	return false


## 关卡挑战结束（通关或无法继续，如第 9 关无法击杀）
func is_campaign_finished() -> bool:
	return player_hp <= 0 or current_level >= config.total_levels