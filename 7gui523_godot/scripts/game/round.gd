class_name Round
extends RefCounted

# ═══════════════════════════════════════════════════════════════
# Round — 回合逻辑（双牌堆）
#
# 用法：
#   var r = Round.new(player_A, player_B, deck_A, deck_B)
#   var result = r.process_play(cards)  # 喂入选好的牌
#   根据 result["state"] 决定下一步
# ═══════════════════════════════════════════════════════════════


# ═══════════════════════════════════════════════════════════════
# 信号（事件驱动，UI/AI 层监听）
# ═══════════════════════════════════════════════════════════════

signal round_started
signal turn_changed(who: RefCounted)
signal round_won(winner: RefCounted, damage: int)
signal card_played(who: RefCounted, cards: Array)
signal card_passed(who: RefCounted)
signal pressure_bonus(who: RefCounted, bonus: int)
signal error_invalid_play(reason: String)
signal error_cannot_beat(reason: String)
signal error_hand_full
signal error_not_your_turn
signal deck_refill(who: RefCounted, count: int)
signal deck_emptied(which: String)
signal big_card_countered(who: RefCounted, by: RefCounted)
signal special_victory(who: RefCounted)
signal game_over


# ═══════════════════════════════════════════════════════════════
# 状态
# ═══════════════════════════════════════════════════════════════

var current: Player.PlayerData
var opponent: Player.PlayerData
var current_deck: Deck.DeckManager
var opponent_deck: Deck.DeckManager
var last_player: RefCounted = null
var last_play: CardTypeResult = null
## 桌面当前一手（每次出牌被覆盖，供 UI 显示，不再跨手累积）
var table_cards: Array[Card] = []
var table_bonus := 0            ## 压迫加分累积
var table_score := 0            ## 桌面全部落牌的原始分累积（结算用）

## Boss 特殊规则（来自 Campaign.LevelRule）；为 null 表示无特殊规则
var level_rule = null   ## Campaign.LevelRule
var _boss: Player.PlayerData = null   ## 哪一方是 Boss（用于判断规则触发方）


# ═══════════════════════════════════════════════════════════════
# 生命周期
# ═══════════════════════════════════════════════════════════════

func _init(first: Player.PlayerData, second: Player.PlayerData,
		first_deck: Deck.DeckManager, second_deck: Deck.DeckManager,
		boss: Player.PlayerData = null, rule = null) -> void:
	current = first
	opponent = second
	current_deck = first_deck
	opponent_deck = second_deck
	_boss = boss
	level_rule = rule

	## 急眼雀：Boss 每轮必定先手
	if level_rule and level_rule.boss_always_first and _boss and current != _boss:
		_swap()


## 当前该谁出牌（方便外边查询）
func whose_turn() -> Player.PlayerData:
	return current


# ═══════════════════════════════════════════════════════════════
# 核心：处理一手出牌
# ═══════════════════════════════════════════════════════════════

## 传入 current 玩家选的牌，Round 做全部判定
## 返回 Dictionary: {state, winner, table_cards, table_bonus, reason}
##   "continue"    → 已轮换，该下一位出
##   "round_win"   → 有人"不要"了，上家赢
##   "instant_win" → Gui523 或手牌+牌堆双空，直接赢
##   "invalid"     → 牌型非法（已轮换，current 变了）
##   "cannot_beat" → 压不过上家（已轮换，current 变了）
func process_play(play: Array[Card]) -> Dictionary:

	# ── ① 出牌为空 ──
	if play.is_empty():
		if last_play == null or last_play.cards.is_empty():
			# 自由局：不能空过（空=非法牌型）
			if current.hand.is_empty():
				error_invalid_play.emit("没有手牌，只能不要")
			else:
				error_invalid_play.emit("不能空过！")
			_swap()
			return {"state": "invalid", "reason": "不能空过！"}
		else:
			# 有人出过牌 → "不要" → 上家赢，双方补牌到 5
			card_passed.emit(current)
			var old_size := current.hand.size()
			current_deck.fill_to_five(current)
			var drawn := current.hand.size() - old_size
			if drawn > 0:
				deck_refill.emit(current, drawn)

			old_size = opponent.hand.size()
			opponent_deck.fill_to_five(opponent)
			drawn = opponent.hand.size() - old_size
			if drawn > 0:
				deck_refill.emit(opponent, drawn)

			return _round_win(last_player)

	# ── ② 解析牌型 ──
	var parsed := CardTypeResult.parse_any_card_type(play)
	if parsed.type == CardTypeResult.CardType.INVALID:
		error_invalid_play.emit("不是合法牌型")
		_swap()
		return {"state": "invalid", "reason": "不是合法牌型"}

	# ── ③ 能不能压过 ──
	if last_play != null and not last_play.cards.is_empty():
		var beat := CardTypeResult.can_beat(parsed, last_play)

		## 赖账鬼：Boss 用同花色单张可压 J 以下的单张（无视点数）
		if not beat and _boss and current == _boss and level_rule:
			if level_rule.ghost_can_force_beat(parsed, last_play):
				beat = true

		## 不识数（Blind Mix）：Boss 出牌压不过时，50% 概率同牌型蒙混过关
		if not beat and current == _boss and level_rule:
			if level_rule.blind_mix_may_pass(parsed, last_play):
				beat = true

		if not beat:
			error_cannot_beat.emit("打不过上家")
			_swap()
			return {"state": "cannot_beat", "reason": "打不过上家"}

		# ③-b 检测大牌反压
		if last_play.type in [CardTypeResult.CardType.BOMB, CardTypeResult.CardType.ROCKET, CardTypeResult.CardType.FIVE_OF_A_KIND]:
			big_card_countered.emit(last_player, current)

	# ── ④ Gui523 直接赢 ──
	if parsed.type == CardTypeResult.CardType.GUI523:
		## 赌场老板：禁止玩家打出 7gui523
		if level_rule and level_rule.ban_gui523 and _boss and current != _boss:
			error_invalid_play.emit("本关禁止打出 7gui523！")
			_swap()
			return {"state": "invalid", "reason": "本关禁止打出 7gui523！"}
		special_victory.emit(current)
		return _instant_win(current)

	# ── ⑤ 压迫加分 ──
	var bonus := CardTypeResult.calculate_pressure_bonus(parsed, last_play)
	parsed.bonus_score = bonus
	table_bonus += bonus
	if bonus > 0:
		pressure_bonus.emit(current, bonus)

	# ── ⑥ 从手牌删除 → 放桌上（桌面只保留当前一手；落牌分另累加）──
	current.remove_cards_from_hand(play)
	table_cards.clear()
	table_cards.append_array(play)
	table_score += Score.calculate_score(play)

	last_play = parsed
	last_player = current
	card_played.emit(current, play)

	# ── ⑥-b 立刻补区：出了立刻补满 2 张（与普通区互不干扰；普通区仍只在"不要"时补 5）──
	if current_deck:
		current_deck.fill_player_instant(current)

	# ── ⑦ 手牌打空了？（不补牌，补牌只在"不要"时触发） ──
	if current.hand.is_empty() and current_deck.cards.is_empty():
		deck_emptied.emit(current.name)
		return _instant_win(current)

	# ── ⑧ 轮换到下一位 ──
	_swap()
	turn_changed.emit(current)
	return {"state": "continue"}


# ═══════════════════════════════════════════════════════════════
# 内部
# ═══════════════════════════════════════════════════════════════

## 轮换 current ↔ opponent
func _swap() -> void:
	var tmp_p := current
	current = opponent
	opponent = tmp_p

	var tmp_d := current_deck
	current_deck = opponent_deck
	opponent_deck = tmp_d


## 上家"不要"——结算桌面伤害（5/10/K + 压迫分）
func _round_win(winner: RefCounted) -> Dictionary:
	# 赢家成为下一小局先手：若赢家不是 current（上家），轮换使其先出
	if winner != current:
		_swap()
	var damage := table_score + table_bonus
	table_cards.clear()
	table_score = 0
	table_bonus = 0
	last_play = null
	last_player = null
	round_won.emit(winner, damage)
	return {
		"state": "round_win",
		"winner": winner,
		"damage": damage,
	}


# ═══════════════════════════════════════════════════════════════
# 赖账鬼（ghost_cheat）辅助判定
# ═══════════════════════════════════════════════════════════════

## 赖账鬼：禁玩家用相同点数压牌（供 UI 出牌前拦截，不轮换）；AI 不受此限制
## 判定体在 LevelRule，此处仅调度
func player_blocked_by_ghost(parsed: CardTypeResult) -> bool:
	if level_rule == null:
		return false
	var is_boss := _boss != null and current == _boss
	return level_rule.ghost_block_player(parsed, last_play, is_boss)


## 赖账鬼：Boss Pass 前强压玩家 J 以下的同花色单张（判定体在 LevelRule）
func ghost_force_play(boss_hand: Array[Card]) -> Array[Card]:
	if level_rule == null:
		return []
	return level_rule.ghost_force_play(boss_hand, last_play)


## 直接赢：Gui523 或手牌+牌堆双空
func _instant_win(winner: RefCounted) -> Dictionary:
	## 黑老大：Boss 先出完时，强制弃掉玩家（输家）手中最大的一张牌
	## _instant_win 固定 winner==current，所以 winner==_boss → current==_boss → loser=opponent
	if _boss and winner == _boss and level_rule:
		level_rule.steal_max_from(opponent)
	table_cards.clear()
	table_score = 0
	table_bonus = 0
	last_play = null
	last_player = null
	return {
		"state": "instant_win",
		"winner": winner,
	}