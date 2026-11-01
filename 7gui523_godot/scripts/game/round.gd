class_name Round
extends RefCounted

# ═══════════════════════════════════════════════════════════════
# Round — 回合逻辑（双牌堆 + 筹码桌面）
#
# 数值统一：分 = 伤害 = HP = 筹码（1:1）。
#   每小回合开局 → 双方各缴 10 筹码底注，上一小回合赢家额外 +5
#   出牌 → 按「基础筹码 + 压牌筹码 + 立刻补区加注」往桌面下注，筹码不足记负债
#   小回合结束 → 赢家收走桌面；双方各自双倍偿还本局负债
#   还不起 → 结算后筹码仍为负，由 UI 结束本关/整局（负数即负债）
# ═══════════════════════════════════════════════════════════════


# ═══════════════════════════════════════════════════════════════
# 信号（事件驱动，UI/AI 层监听）
# ═══════════════════════════════════════════════════════════════

signal round_started
signal turn_changed(who: Player.PlayerData)
signal round_won(winner: Player.PlayerData, pot: int)
signal card_played(who: Player.PlayerData, cards: Array[Card])
signal card_passed(who: Player.PlayerData)
signal pressure_chips(who: Player.PlayerData, amount: int)
signal instant_stake_chips(who: Player.PlayerData, amount: int)
signal round_ante(who: Player.PlayerData, amount: int)
signal error_invalid_play(reason: String)
signal error_cannot_beat(reason: String)
signal deck_refill(who: Player.PlayerData, count: int)
signal deck_emptied(which: String)
signal big_card_countered(who: Player.PlayerData, by: Player.PlayerData)
signal special_victory(who: Player.PlayerData)
signal game_over


# ═══════════════════════════════════════════════════════════════
# 状态
# ═══════════════════════════════════════════════════════════════

var current: Player.PlayerData
var opponent: Player.PlayerData
var current_deck: Deck.DeckManager
var opponent_deck: Deck.DeckManager
var current_instant_deck: Deck.DeckManager    ## 立刻补区牌堆（双方共用同一副，换手时交换等于无操作）
var opponent_instant_deck: Deck.DeckManager    ## 同上：指向同一个共享牌堆实例
var last_player: Player.PlayerData = null
var last_play: CardTypeResult = null
## 桌面当前一手（每次出牌被覆盖，供 UI 显示）
var table_cards: Array[Card] = []
## 本小局桌面累计筹码（双方下注之和）
var table_chips := 0

## 桌面筹码别名，与 table_chips 保持 1:1
var table_score: int:
	get: return table_chips

## 第 9 关：无论谁赢，桌面筹码都归 BOSS
var pot_to_boss := false

## 最近一次桌面/负债结算中“还不起”的玩家（负数即负债）
var failed_players: Array[Player.PlayerData] = []

## Boss 特殊规则（来自 Campaign.LevelRule）；为 null 表示无特殊规则
var level_rule = null   ## Campaign.LevelRule
var _boss: Player.PlayerData = null   ## 哪一方是 Boss（用于判断规则触发方）

## 立刻补区出牌加注：出 1 张 +1 筹码，一次出 2 张 +3 筹码（计入桌面下注）
const INSTANT_PLAY_STAKE := [0, 1, 3]

## 每小回合开局底注：双方各投入 10 筹码；上一小回合赢家额外投入 5（共 15）
const ROUND_ANTE_CHIPS := 10
const ROUND_ANTE_WIN_BONUS := 5

## 新小回合首手出牌时是否还需缴纳底注
var _ante_pending := false
## 上一小回合赢家（下一小回合底注 +5）
var _last_round_winner: Player.PlayerData = null


## 小回合出牌结果：统一类型替代散装 Dictionary，编译期检查所有字段拼写。
class PlayResult extends RefCounted:
	var state: String = ""                    ## "continue" | "round_win" | "instant_win" | "boss_finish" | "invalid" | "cannot_beat"
	var winner: Player.PlayerData = null      ## 小回合赢家（round_win / instant_win / boss_finish）
	var pot: int = 0                          ## 赢家收走的桌面筹码
	var penalty: int = 0                      ## BOSS 双空时对手剩余牌惩罚筹码（仅 boss_finish）
	var reason: String = ""                   ## 错误原因文本（invalid / cannot_beat）


# ═══════════════════════════════════════════════════════════════
# 生命周期
# ═══════════════════════════════════════════════════════════════

func _init(first: Player.PlayerData, second: Player.PlayerData,
		first_deck: Deck.DeckManager, second_deck: Deck.DeckManager,
		first_instant_deck: Deck.DeckManager = null, second_instant_deck: Deck.DeckManager = null,
		boss: Player.PlayerData = null, rule = null) -> void:
	current = first
	opponent = second
	current_deck = first_deck
	opponent_deck = second_deck
	current_instant_deck = first_instant_deck
	opponent_instant_deck = second_instant_deck
	_boss = boss
	level_rule = rule

	## 急眼雀：Boss 每轮必定先手
	if level_rule and level_rule.boss_always_first and _boss and current != _boss:
		_swap()

	## 首个小回合：首手出牌时缴底注
	_ante_pending = true


## 当前该谁出牌（方便外边查询）
func whose_turn() -> Player.PlayerData:
	return current


# ═══════════════════════════════════════════════════════════════
# 核心：处理一手出牌
# ═══════════════════════════════════════════════════════════════

## 返回 PlayResult: 字段 state / winner / pot / penalty / reason
##   "continue"    → 已轮换，该下一位出
##   "round_win"   → 有人"不要"了，上家赢，pot = 桌面筹码
##   "instant_win" → Gui523 或手牌 + 游戏牌堆(active) 双空，直接赢
##   "boss_finish" → BOSS 手牌 + 游戏牌堆(active) 双空，桌面已结算
##   "invalid"     → 牌型非法（已轮换，current 变了）
##   "cannot_beat" → 压不过上家（已轮换，current 变了）
func process_play(play: Array[Card]) -> PlayResult:

	# ── ① 出牌为空 ──
	if play.is_empty():
		if last_play == null or last_play.cards.is_empty():
			# 自由局：不能空过（空=非法牌型）
			if current.hand_size() < 1:
				error_invalid_play.emit("没有手牌，只能不要")
			else:
				error_invalid_play.emit("不能空过！")
			_swap()
			var r := PlayResult.new()
			r.state = "invalid"
			r.reason = "不能空过！"
			return r
		else:
			# 有人出过牌 → "不要" → 上家赢，双方补牌到 5
			card_passed.emit(current)
			_refill_normal(current, current_deck)
			_refill_normal(opponent, opponent_deck)
			return _round_win(last_player)

	# ── ② 解析牌型 ──
	var parsed := CardTypeResult.parse_any_card_type(play)
	if parsed.type == CardTypeResult.CardType.INVALID:
		error_invalid_play.emit("不是合法牌型")
		_swap()
		var r := PlayResult.new()
		r.state = "invalid"
		r.reason = "不是合法牌型"
		return r

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
			var r := PlayResult.new()
			r.state = "cannot_beat"
			r.reason = "打不过上家"
			return r

		# ③-b 检测大牌反压
		if last_play.type in [
				CardTypeResult.CardType.BOMB,
				CardTypeResult.CardType.ROCKET,
				CardTypeResult.CardType.FIVE_OF_A_KIND,
		]:
			big_card_countered.emit(last_player, current)

	# ── ④ Gui523 直接赢 ──
	if parsed.type == CardTypeResult.CardType.GUI523:
		return _play_gui523(parsed, play)

	# ── ⑤～⑧ 普通出牌：下注 → 上桌 → 补立刻区 → 判定结束 → 轮换 ──
	return _play_normal(parsed, play)


# ═══════════════════════════════════════════════════════════════
# 内部
# ═══════════════════════════════════════════════════════════════

## 7gui523：直接赢下当前小回合（第 9 关禁止玩家打出）
func _play_gui523(parsed: CardTypeResult, play: Array[Card]) -> PlayResult:
	## 赌场老板：禁止玩家打出 7gui523
	if level_rule and level_rule.ban_gui523 and _boss and current != _boss:
		error_invalid_play.emit("本关禁止打出 7gui523！")
		_swap()
		var r := PlayResult.new()
		r.state = "invalid"
		r.reason = "本关禁止打出 7gui523！"
		return r
	## 7gui523 强制赢下当前小回合
	_charge_ante_if_needed()
	_pay_stake(parsed, play)
	special_victory.emit(current)
	_play_cards(current, play)
	if current_instant_deck:
		current_instant_deck.fill_player_instant(current)
	## 与"不要"结束时一致：双方普通手牌补到 5
	current_deck.fill_player_normal(current)
	opponent_deck.fill_player_normal(opponent)
	table_cards.clear()
	table_cards.append_array(play)
	last_play = parsed
	last_player = current
	return _round_win(current)


## 普通出牌流程：桌面下注 → 上手牌 → 补立刻区 → 空手判定 → 轮换
func _play_normal(parsed: CardTypeResult, play: Array[Card]) -> PlayResult:
	# ── ⑤ 桌面下注：底注（开局）+ 基础筹码（5/10/K）+ 压牌筹码 ──
	_charge_ante_if_needed()
	_pay_stake(parsed, play)

	# ── ⑥ 从手牌删除 → 放桌上（桌面只保留当前一手）──
	_play_cards(current, play)
	table_cards.clear()
	table_cards.append_array(play)

	last_play = parsed
	last_player = current
	card_played.emit(current, play)

	# ── ⑥-b 立刻补区：出了立刻补满 2 张（使用独立立刻区牌堆，优先从立刻区牌堆抽）──
	if current_instant_deck:
		current_instant_deck.fill_player_instant(current)

	# ── ⑦ 一方手牌与「游戏牌堆」（active 区）都空：结算桌面后结束 ──
	# 隐藏区只在新关卡开始时并入游戏牌堆，关内不参与抽牌，因此不计入牌堆为空的判定。
	if current.is_hand_empty() and current_deck.active_count == 0:
		return _finish_when_deck_empty()

	# ── ⑧ 轮换到下一位 ──
	_swap()
	turn_changed.emit(current)
	var r := PlayResult.new()
	r.state = "continue"
	return r


## 手牌与游戏牌堆双空：BOSS 收走桌面并按剩余牌惩罚，玩家则直接赢下
func _finish_when_deck_empty() -> PlayResult:
	if _boss and current == _boss:
		## BOSS 先出完：对手剩余牌 × 3 筹码惩罚
		var remaining := opponent.hand_size() + opponent_deck.active_count
		var penalty := remaining * GameConfig.BOSS_FINISH_CHIPS_PER_CARD
		opponent.chips -= penalty
		var pot := _settle_pot(current)
		var r := PlayResult.new()
		r.state = "boss_finish"
		r.winner = current
		r.pot = pot
		r.penalty = penalty
		return r
	deck_emptied.emit(current.name)
	return _instant_win(current)


## 轮换 current ↔ opponent
func _swap() -> void:
	var tmp_p := current
	current = opponent
	opponent = tmp_p

	var tmp_d := current_deck
	current_deck = opponent_deck
	opponent_deck = tmp_d

	var tmp_id := current_instant_deck
	current_instant_deck = opponent_instant_deck
	opponent_instant_deck = tmp_id


func _refill_normal(player: Player.PlayerData, deck: Deck.DeckManager) -> void:
	var old_size := player.hand_size()
	deck.fill_player_normal(player)
	var drawn := player.hand_size() - old_size
	if drawn > 0:
		deck_refill.emit(player, drawn)


## 新小回合的首手出牌前缴纳底注：双方各 10 筹码，上一小回合赢家额外 5
func _charge_ante_if_needed() -> void:
	if not _ante_pending:
		return
	_ante_pending = false
	_charge_ante(current)
	_charge_ante(opponent)


func _charge_ante(player: Player.PlayerData) -> void:
	var amount: int = ROUND_ANTE_CHIPS
	if player == _last_round_winner:
		amount += ROUND_ANTE_WIN_BONUS
	player.pay_stake(amount)
	table_chips += amount
	round_ante.emit(player, amount)


## 拆分一手出牌为普通区牌和立刻区牌（与 remove_cards_from_hand 匹配逻辑一致）
func _split_played_cards(player: Player.PlayerData, play: Array[Card]) -> Array:
	var normal: Array[Card] = []
	var instant: Array[Card] = []
	var normal_keys: Array[String] = []
	for c in player.normal_hand:
		normal_keys.append(c.point + "|" + c.suit)
	for card in play:
		var key: String = card.point + "|" + card.suit
		var idx: int = normal_keys.find(key)
		if idx >= 0:
			normal_keys.remove_at(idx)
			normal.append(card)
		else:
			instant.append(card)
	return [normal, instant]


## 拆分 → 从手牌删除 → 分别回收至主牌堆和立刻区牌堆
func _play_cards(player: Player.PlayerData, play: Array[Card]) -> void:
	if not current_deck or not current_instant_deck:
		player.remove_cards_from_hand(play)
		return
	var split := _split_played_cards(player, play)
	player.remove_cards_from_hand(play)
	current_deck.recycle_played(split[0])
	current_instant_deck.recycle_played(split[1])


## 出牌下注：基础筹码（5/10/K）+ 压牌筹码（同牌型同点）+ 立刻补区加注（1 张 1 / 2 张 3）
## 筹码不足会直接扣成负数（负数即负债）。
func _pay_stake(parsed: CardTypeResult, play: Array[Card]) -> void:
	var base_chips: int = Score.calculate_score(play)
	var pressure: int = CardTypeResult.calculate_pressure_chips(parsed, last_play)
	var instant_used: int = current.count_instant_in_play(play)
	var instant_stake: int = INSTANT_PLAY_STAKE[mini(instant_used, INSTANT_PLAY_STAKE.size() - 1)]
	var stake: int = base_chips + pressure + instant_stake
	parsed.pressure_chips = pressure
	if stake > 0:
		current.pay_stake(stake)
		table_chips += stake
		if pressure > 0:
			pressure_chips.emit(current, pressure)
		if instant_stake > 0:
			instant_stake_chips.emit(current, instant_stake)


## 小回合结束：桌面筹码给赢家（第 9 关固定给 BOSS）；
## 双方在收桌面之前先记下各自负债，再按 2× 扣还。
## 返回本次桌面筹码（pot）
func _settle_pot(winner: Player.PlayerData) -> int:
	var pot := table_chips
	table_chips = 0
	failed_players.clear()

	# 负债必须在收桌面之前取快照，避免用赢来的筹码把负债“洗掉”
	var current_debt := current.debt_amount()
	var opponent_debt := opponent.debt_amount()

	var receiver := _boss if pot_to_boss and _boss != null else winner
	receiver.receive_chips(pot)

	# 不可以借钱还贷：筹码不够支付 2×负债时直接记失败，不继续扣成更多负数
	if not current.pay_debt(current_debt):
		failed_players.append(current)
	if not opponent.pay_debt(opponent_debt):
		failed_players.append(opponent)
	return pot


## 上家"不要"——结算桌面筹码与双方负债
func _round_win(winner: Player.PlayerData) -> PlayResult:
	# 赢家成为下一小局先手：若赢家不是 current（上家），轮换使其先出
	if winner != current:
		_swap()

	## 疯狗：BOSS 赢下小局时，按最后出牌张数给输家追加负债（随后双倍偿还）
	if _boss and winner == _boss and level_rule and level_rule.boss_bite_damage:
		var bite_cards := 0
		if last_play != null:
			bite_cards = last_play.cards.size()
		if bite_cards > 0:
			opponent.chips -= level_rule.bite_damage(bite_cards)

	var pot := _settle_pot(winner)
	table_cards.clear()
	last_play = null
	last_player = null
	## 下一小回合底注：上一小回合赢家额外 +5
	_last_round_winner = winner
	if failed_players.is_empty():
		_ante_pending = true
	round_won.emit(winner, pot)
	var r := PlayResult.new()
	r.state = "round_win"
	r.winner = winner
	r.pot = pot
	return r


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


## 直接赢：Gui523 或手牌 + 游戏牌堆(active) 双空。
## 先结算桌面与负债，再返回胜利；负债后筹码 < 0 由 UI 判为还不起。
func _instant_win(winner: Player.PlayerData) -> PlayResult:
	## 黑老大：Boss 先出完时，强制弃掉玩家（输家）手中最大的一张牌
	## _instant_win 固定 winner==current，所以 winner==_boss → current==_boss → loser=opponent
	if _boss and winner == _boss and level_rule:
		level_rule.steal_max_from(opponent)
	var pot := _settle_pot(winner)
	table_cards.clear()
	last_play = null
	last_player = null
	var r := PlayResult.new()
	r.state = "instant_win"
	r.winner = winner
	r.pot = pot
	return r