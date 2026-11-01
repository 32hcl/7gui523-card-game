class_name Campaign
extends RefCounted

# ═══════════════════════════════════════════════════════════════
# Campaign — 关卡模式（筹码版）
#
# 数值统一：HP / 得分 / 筹码 全部 1:1 合并为筹码。
#   玩家筹码跨关保留；BOSS 每关重置为 BOSS_INITIAL_CHIPS。
#   小回合的输赢由 Round 直接结算到 PlayerData.chips / debt；
#   本类只负责关卡推进、奖励与“破产即结束”的判定。
# ═══════════════════════════════════════════════════════════════


# ═══════════════════════════════════════════════════════════════
# 配置
# ═══════════════════════════════════════════════════════════════

class CampaignConfig:
	var initial_chips      := GameConfig.INITIAL_CHIPS
	var boss_initial_chips := GameConfig.BOSS_INITIAL_CHIPS
	var total_levels       := GameConfig.TOTAL_LEVELS


class LevelResult:
	var player_won: bool
	var boss_cleared: bool
	var chips_delta: int
	var chips_after: int


# ═══════════════════════════════════════════════════════════════
# 静态数据（引用 GameConfig 全局常量，避免数据重复）
# ═══════════════════════════════════════════════════════════════

## 入场筹码（赌注）
const ENTRY_CHIPS := GameConfig.ENTRY_CHIPS

## 赌场门票（金币；与每关入场筹码完全独立）
const ENTRY_TICKET_DOLLARS := GameConfig.ENTRY_TICKET_DOLLARS
const INITIAL_DOLLARS := GameConfig.INITIAL_DOLLARS

## 奖励筹码（赢后获得）
const REWARD_CHIPS := GameConfig.REWARD_CHIPS

## 每关 BOSS 名
const BOSS_NAMES := GameConfig.BOSS_NAMES


## 关卡 与 战役一体化：每关规则（LevelRule）+ 关卡间流程（Campaign）


## ═══════════════════════════════════════════════════════════════
## 每关特殊规则（LevelRule）
## ═══════════════════════════════════════════════════════════════

class LevelRule:
	var mask_player_draws := false ## 抹牌虫(2)：初始发牌后，新摸牌有概率显示牌背
	var boss_swap_low_cards := false ## 换牌鬼(8)：BOSS 可把4/6换成指定牌
	var _boss_swapped_cards: Dictionary = {}
	var boss_always_first := false   ## 急眼雀(3)：每轮 boss 先手
	var boss_bite_damage  := false   ## 疯狗(4)：每张牌 ×3 额外负债
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
		if loser == null or loser.is_hand_empty():
			return
		var max_card: Card = null
		var loser_hand: Array[Card] = []
		loser.collect_hand_into(loser_hand)
		for c in loser_hand:
			if max_card == null or Card.get_rank(c.point) > Card.get_rank(max_card.point):
				max_card = c
		if max_card != null:
			loser.remove_cards_from_hand([max_card])


	## 疯狗：Boss 赢下小局时，每张出牌给输家追加多少负债
	func bite_damage(num_cards: int) -> int:
		return num_cards * GameConfig.BITE_CHIPS_PER_CARD if boss_bite_damage else 0

	func mutate_boss_hand(hand: Array[Card]) -> void:
		if not boss_swap_low_cards:
			return
		var replacements := ["6", "8", "9", "10", "J", "Q", "K"]
		for card in hand:
			if not _boss_swapped_cards.has(card) and (card.point == "4" or card.point == "6"):
				_boss_swapped_cards[card] = true
				card.point = replacements[randi() % replacements.size()]


	func reset_boss_swaps() -> void:
		for card in _boss_swapped_cards:
			card.reset_to_def()
		_boss_swapped_cards.clear()


## 获取关卡规则（工厂：九关特殊规则表）
static func level_rule(level: int) -> LevelRule:
	var r := LevelRule.new()
	match level:
		2: r.mask_player_draws = true
		3: r.boss_always_first = true
		4: r.boss_bite_damage  = true
		5: r.ghost_cheat       = true
		6: r.blind_mix         = true
		7: r.boss_steal_max    = true
		8: r.boss_swap_low_cards = true
		9: r.ban_gui523        = true
	return r


# ═══════════════════════════════════════════════════════════════
# 静态工具
# ═══════════════════════════════════════════════════════════════

static func get_level_entry_chips(level: int) -> int:
	if level < 1 or level > GameConfig.TOTAL_LEVELS:
		return 0
	return ENTRY_CHIPS[level]

static func get_level_reward_chips(level: int) -> int:
	if level < 1 or level > GameConfig.TOTAL_LEVELS:
		return 0
	return REWARD_CHIPS[level]

static func get_boss_name(level: int) -> String:
	if level < 1 or level > GameConfig.TOTAL_LEVELS:
		return "???"
	return BOSS_NAMES[level]


# ═══════════════════════════════════════════════════════════════
# 实例
# ═══════════════════════════════════════════════════════════════

var config: CampaignConfig
var player_chips: int = GameConfig.INITIAL_CHIPS
var boss_chips: int = GameConfig.BOSS_INITIAL_CHIPS
var current_level: int
var dollar: int = INITIAL_DOLLARS
var cleared_level: int
var has_purchased_entry: bool
## 商店 TO_HAND 购买、留到下关开牌时进入手牌的牌
var pending_hand_cards: Array[Card] = []


func _init(cfg: CampaignConfig = CampaignConfig.new()) -> void:
	config = cfg
	start_new()


func start_new() -> void:
	player_chips = config.initial_chips
	boss_chips = config.boss_initial_chips
	current_level = 1
	dollar = INITIAL_DOLLARS
	cleared_level = 0
	has_purchased_entry = false
	pending_hand_cards.clear()


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
	boss_chips = config.boss_initial_chips


## 把 Round 结算后的双方筹码同步回战役（负数即负债）
func sync_from_battle(human: Player.PlayerData, ai: Player.PlayerData) -> void:
	player_chips = human.chips
	boss_chips = ai.chips


# ═══════════════════════════════════════════════════════════════
# 关卡结算 / 判定
# ═══════════════════════════════════════════════════════════════

## 关卡结束：胜利发奖励筹码；玩家筹码为负（还不起）或第 9 关结束本轮
func finish_level(player_won: bool, player_failed: bool = false) -> LevelResult:
	var lr := LevelResult.new()
	## 第 9 关是不可通关的最终挑战；player_failed = 本小局还不起负债。
	var accepted := player_won and not player_failed and player_chips >= 0 and current_level < config.total_levels
	lr.player_won = accepted
	lr.boss_cleared = accepted
	lr.chips_delta = 0

	if accepted:
		var reward := get_level_reward_chips(current_level)
		player_chips += reward
		lr.chips_delta = reward
		if current_level > cleared_level:
			cleared_level = current_level

	lr.chips_after = player_chips

	if player_failed or player_chips < 0 or current_level >= config.total_levels:
		has_purchased_entry = false

	return lr


func is_game_over() -> bool:
	return player_chips < 0