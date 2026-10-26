class_name AICore
extends RefCounted

# ═══════════════════════════════════════════════════════════════
# AICore — AI 核心模块
# 包含: enumerateLegalPlays, AI1(规则), AI2(贪心)
# ═══════════════════════════════════════════════════════════════

const COUNTER_RISK_WEIGHT := 2


# ═══════════════════════════════════════════════════════════════
# 枚举所有合法出牌组合
# ═══════════════════════════════════════════════════════════════

static func enumerate_legal_plays(player: Player.PlayerData) -> Array[Array]:
	var result: Array[Array] = []
	var hand_buf: Array[Card] = []
	player.collect_hand_into(hand_buf)
	var n := hand_buf.size()
	for mask in range(1, 1 << n):
		var subset: Array[Card] = []
		for i in n:
			if mask & (1 << i):
				subset.append(hand_buf[i])
		var parsed := CardTypeResult.parse_any_card_type(subset)
		if parsed.type != CardTypeResult.CardType.INVALID and parsed.type != CardTypeResult.CardType.PASS:
			result.append(subset)
	return result


# ═══════════════════════════════════════════════════════════════
# 检查是否拆对/拆三/拆四
# ═══════════════════════════════════════════════════════════════

static func breaks_combo(play: Array[Card], player: Player.PlayerData) -> bool:
	var hand_count := {}
	var hand_buf: Array[Card] = []
	player.collect_hand_into(hand_buf)
	for c in hand_buf:
		hand_count[c.point] = hand_count.get(c.point, 0) + 1

	var play_count := {}
	for c in play:
		play_count[c.point] = play_count.get(c.point, 0) + 1

	for pt in play_count:
		var in_hand: int = hand_count.get(pt, 0)
		var in_play: int = play_count.get(pt, 0)
		if in_hand >= 2 and in_play == 1: return true
		if in_hand >= 3 and in_play == 2: return true
		if in_hand >= 4 and in_play == 3: return true

	return false


# ═══════════════════════════════════════════════════════════════
# 反压风险估算
# ═══════════════════════════════════════════════════════════════

static func estimate_counter_risk(play: Array[Card], player: Player.PlayerData,
		opponent: Player.PlayerData, tracker: CardTracker, parsed: CardTypeResult,
		table_score: int, deck: Deck.DeckManager) -> int:
	if tracker == null:
		return 0
	if opponent.is_hand_empty():
		return 0
	if parsed.type == CardTypeResult.CardType.ROCKET:
		return 0
	if parsed.type == CardTypeResult.CardType.GUI523:
		return 0

	var my_rank := Card.get_rank(parsed.key_point)

	var opp_beats := 0
	for pt in Card.RANK_MAP:
		var r: int = Card.RANK_MAP[pt]
		if r < my_rank:
			continue
		var might := tracker.expected_opponent_count(pt, opponent.hand_size(), deck.cards.size())
		if might > 0.5:
			opp_beats += 1

	if opp_beats == 0:
		return 0

	var weight := 1 + table_score / 10
	return opp_beats * weight


# ═══════════════════════════════════════════════════════════════
# 单步评分
# ═══════════════════════════════════════════════════════════════

static func evaluate_play(play: Array[Card], player: Player.PlayerData, opponent: Player.PlayerData,
		deck: Deck.DeckManager, previous: CardTypeResult, table_score: int, tracker: CardTracker) -> Dictionary:
	var bd := {
		"base": 0, "score_card": 0, "special": 0, "endgame": 0,
		"combo_break": 0, "table_score": 0, "defensive": 0, "tracker": 0, "total": 0,
	}

	bd["table_score"] = table_score * 2
	bd["base"] = play.size() * 10
	if play.size() == player.hand_size():
		bd["base"] += 1000

	var rank_penalty := 0
	for c in play:
		var r := Card.get_rank(c.point)
		if r >= 13:
			rank_penalty -= r * 3
		elif r >= 10:
			rank_penalty -= r * 2
		else:
			rank_penalty -= r
	bd["endgame"] = rank_penalty

	for c in play:
		if c.score > 0 and table_score > 0:
			bd["score_card"] += c.score * 2

	bd["combo_break"] = -17 if breaks_combo(play, player) else 0

	var parsed := CardTypeResult.parse_card_type(play)
	if parsed.type == CardTypeResult.CardType.BOMB:
		bd["special"] -= 92
	elif parsed.type == CardTypeResult.CardType.ROCKET:
		bd["special"] -= 459

	if opponent.hand_size() <= 2:
		if parsed.type == CardTypeResult.CardType.BOMB:
			bd["special"] += 236
		elif parsed.type == CardTypeResult.CardType.ROCKET:
			bd["special"] += 223

	if tracker:
		for c in play:
			if tracker.is_exhausted(c.point):
				bd["tracker"] += 30
		var risk := estimate_counter_risk(play, player, opponent, tracker, parsed, table_score, deck)
		bd["defensive"] = -risk * COUNTER_RISK_WEIGHT

	bd["total"] = bd["base"] + bd["score_card"] + bd["special"] + bd["endgame"] \
		+ bd["combo_break"] + bd["table_score"] + bd["defensive"] + bd["tracker"]

	return bd


static func play_gain(play: Array[Card], player: Player.PlayerData, opponent: Player.PlayerData,
		deck: Deck.DeckManager, previous: CardTypeResult, table_score: int, tracker: CardTracker) -> int:
	return evaluate_play(play, player, opponent, deck, previous, table_score, tracker)["total"]


# ═══════════════════════════════════════════════════════════════
# AI1 — 规则驱动（不依赖对手/桌面分/tracker）
# ═══════════════════════════════════════════════════════════════

# 零分桌面不出炸弹
static var ai1_zero_score_pass := true


static func choose_ai1(player: Player.PlayerData, prev: CardTypeResult,
		opponent: Player.PlayerData = null, deck: Deck.DeckManager = null,
		table_score: int = 0, tracker: CardTracker = null) -> Array[Card]:
	var all_plays := enumerate_legal_plays(player)
	if all_plays.is_empty():
		return []

	# 首出（prev 为 null 或桌面无牌时）
	if prev == null or prev.cards.is_empty():
		var hand_buf: Array[Card] = []
		player.collect_hand_into(hand_buf)
		if hand_buf.is_empty():
			return []
		var sorted := hand_buf.duplicate()
		sorted.sort_custom(func(a: Card, b: Card): return Card.get_rank(a.point) < Card.get_rank(b.point))
		var first_point: String = sorted[0].point
		var combo: Array[Card] = []
		for c in hand_buf:
			if c.point == first_point:
				combo.append(c)
		return [combo[0]] if combo.size() >= 3 else combo

	# 跟牌
	var same_type: Array[Array] = []
	var bombs: Array[Array] = []
	var fives: Array[Array] = []
	var rockets: Array[Array] = []

	for play in all_plays:
		var parsed := CardTypeResult.parse_any_card_type(play)
		if not CardTypeResult.can_beat(parsed, prev):
			continue
		match parsed.type:
			CardTypeResult.CardType.FIVE_OF_A_KIND:
				fives.append(play)
			CardTypeResult.CardType.BOMB:
				bombs.append(play)
			CardTypeResult.CardType.ROCKET:
				rockets.append(play)
			_:
				same_type.append(play)

	if not same_type.is_empty():
		var best := same_type[0]
		var best_rank := Card.get_rank(CardTypeResult.parse_card_type(best).key_point)
		for play in same_type:
			var r := Card.get_rank(CardTypeResult.parse_card_type(play).key_point)
			if r < best_rank:
				best = play
				best_rank = r
		return best

	if ai1_zero_score_pass and table_score == 0 and not bombs.is_empty():
		return []

	if not bombs.is_empty():
		var best := bombs[0]
		var best_rank := Card.get_rank(CardTypeResult.parse_card_type(best).key_point)
		for play in bombs:
			var r := Card.get_rank(CardTypeResult.parse_card_type(play).key_point)
			if r < best_rank:
				best = play
				best_rank = r
		return best

	if not fives.is_empty():
		var best := fives[0]
		var best_rank := Card.get_rank(CardTypeResult.parse_card_type(best).key_point)
		for play in fives:
			var r := Card.get_rank(CardTypeResult.parse_card_type(play).key_point)
			if r < best_rank:
				best = play
				best_rank = r
		return best

	if not rockets.is_empty():
		return rockets[0]

	return []


# ═══════════════════════════════════════════════════════════════
# AI3 — 可训练权重（继承 AI2 贪心逻辑，用 AIWeights 驱动评分）
# ═══════════════════════════════════════════════════════════════

static func choose_ai3(player: Player.PlayerData, opponent: Player.PlayerData, prev: CardTypeResult,
		deck: Deck.DeckManager, table_score: int, tracker: CardTracker, w: AIWeights) -> Array[Card]:
	var all_plays := enumerate_legal_plays(player)
	if all_plays.is_empty():
		return []

	var finish := try_finish_play(player, all_plays, prev)
	if not finish.is_empty():
		return finish

	var intercept := try_endgame_intercept(player, all_plays, prev, opponent.hand_size())
	if not intercept.is_empty():
		return intercept

	var best: Array[Card] = []
	var best_gain := -INF
	for play in all_plays:
		var parsed := CardTypeResult.parse_any_card_type(play)
		if prev != null and not prev.cards.is_empty() and not CardTypeResult.can_beat(parsed, prev):
			continue
		var gain := _weighted_gain(play, player, opponent, parsed, prev, deck, table_score, tracker, w)
		if gain > best_gain:
			best_gain = gain
			best = play

	return best


static func _weighted_gain(play: Array[Card], player: Player.PlayerData,
		opponent: Player.PlayerData, parsed: CardTypeResult, previous: CardTypeResult,
		deck: Deck.DeckManager, table_score: int, tracker: CardTracker, w: AIWeights) -> float:
	var gain := 0.0
	var has_previous := previous != null and not previous.cards.is_empty()
	var is_bomb := parsed.type == CardTypeResult.CardType.BOMB \
		or parsed.type == CardTypeResult.CardType.ROCKET \
		or parsed.type == CardTypeResult.CardType.FIVE_OF_A_KIND

	var play_score := 0.0
	for c in play:
		play_score += float(c.score)

	## — 出完即胜 —
	if play.size() == player.hand_size():
		gain += w.finish_hand_reward

	## — 跟牌 / 首出 —
	if has_previous:
		gain += w.beat_same_type_reward if not is_bomb else w.beat_bomb_reward
		if opponent.hand_size() <= 2 and is_bomb:
			gain += w.opponent_low_hand_reward
		elif opponent.hand_size() <= 2:
			gain -= w.opponent_empty_hand_penalty   ## 对手濒临出完而本手未拦截
		gain -= float(Card.get_rank(parsed.key_point)) * w.rank_cost
	else:
		gain -= float(Card.get_rank(parsed.key_point)) * w.high_card_cost
		gain += play_score * w.damage_per_point_reward   ## 首出建立桌面分，赢下回合即伤害

	## — 拆组合成本（按被拆的最大组尺寸计一次） —
	match _broken_combo_size(play, player):
		2:
			gain -= w.break_pair_cost
		3:
			gain -= w.break_triple_cost
		4:
			gain -= w.break_bomb_cost
		5:
			gain -= w.break_five_kind_cost
	if _breaks_rocket(play, player):
		gain -= w.break_rocket_cost

	## — 计分牌 —
	for c in play:
		if table_score > 0:
			gain += float(c.score) * w.score_card_reward
		else:
			gain += float(c.score) * w.low_card_release_reward
	if table_score > 0 and play_score >= float(table_score):
		gain += w.clear_table_reward   ## 本手把桌面分完整兑现

	if parsed.type == CardTypeResult.CardType.FIVE_OF_A_KIND:
		gain += w.five_card_score_reward

	## — 保留潜力（出完后剩余手牌的组合价值） —
	var rest := _rest_counts(play, player)
	var pairs_kept := 0
	var bomb_kept := false
	for pt in rest:
		var n: int = rest[pt]
		if n >= 2:
			pairs_kept += 1
		if n >= 4:
			bomb_kept = true
	if pairs_kept > 0:
		gain += w.preserve_same_point_reward * float(min(pairs_kept, 3))
	if bomb_kept:
		gain += w.preserve_bomb_reward
	if rest.get("大鬼", 0) > 0 and rest.get("小鬼", 0) > 0:
		gain += w.preserve_rocket_reward
	if rest.get("7", 0) > 0 and rest.get("5", 0) > 0 \
			and rest.get("2", 0) > 0 and rest.get("3", 0) > 0:
		gain += w.preserve_gui523_reward

	## — 记牌器信息 —
	var risk := 0
	if tracker != null:
		for c in play:
			if tracker.is_exhausted(c.point):
				gain += w.exhausted_point_reward
			elif tracker.remaining_count(c.point) <= 2:
				gain += w.rare_point_reward   ## 剩 1-2 张的稀有点，趁还能压时兑现

		risk = estimate_counter_risk(play, player, opponent, tracker, parsed, table_score, deck)
		gain -= float(risk) * w.counter_risk_cost

		## 未知边际风险：可能补强成反压的点数（期望持有 0.2~0.5 张）
		var my_rank := Card.get_rank(parsed.key_point)
		var marginal := 0
		for pt in Card.RANK_MAP:
			if Card.RANK_MAP[pt] < my_rank:
				continue
			var might := tracker.expected_opponent_count(pt, opponent.hand_size(), deck.cards.size())
			if might >= 0.2 and might < 0.5:
				marginal += 1
		gain -= float(marginal) * w.unknown_card_risk_cost

		## 扫点：对手大概率持有该点，而我方一次打光全部余量
		var play_pt := {}
		for c in play:
			play_pt[c.point] = play_pt.get(c.point, 0) + 1
		for pt in play_pt:
			var exp_opp := tracker.expected_opponent_count(pt, opponent.hand_size(), deck.cards.size())
			if exp_opp >= 1.0 and tracker.remaining_count(pt) - play_pt[pt] <= 0:
				gain += w.known_opponent_card_reward

	## — 回合终结与期望伤害（risk==0 为启发式：对手当前算不出反压） —
	var ends_round := has_previous and risk == 0
	if ends_round:
		gain += w.finish_round_reward
		gain += float(table_score) * w.table_score_reward
	if play.size() == player.hand_size() or ends_round:
		var expected_damage := float(table_score) + play_score
		gain += expected_damage * w.round_damage_reward
		if expected_damage >= 30.0:
			gain += w.large_damage_reward

	## — 牌堆剩余风险：未知牌越多，炸弹类大件越难再生 —
	if is_bomb:
		gain -= float(deck.cards.size()) * w.remaining_deck_risk_cost

	## — 手牌规模（减牌收益与张数成本对偶，交由训练器定夺） —
	gain += float(play.size()) * w.reduce_hand_size_reward
	gain -= float(play.size()) * w.play_card_count_cost
	return gain


## 本手拆散的最大同点组尺寸（0=未拆；出整组不罚，部分拆按原组尺寸罚）
static func _broken_combo_size(play: Array[Card], player: Player.PlayerData) -> int:
	var hand_count := {}
	var hand_buf: Array[Card] = []
	player.collect_hand_into(hand_buf)
	for c in hand_buf:
		hand_count[c.point] = hand_count.get(c.point, 0) + 1
	var play_count := {}
	for c in play:
		play_count[c.point] = play_count.get(c.point, 0) + 1
	var worst := 0
	for pt in play_count:
		var in_hand: int = hand_count.get(pt, 0)
		var left := in_hand - int(play_count[pt])
		if in_hand >= 2 and left >= 1:
			worst = max(worst, in_hand)
	return worst


## 王炸拆散：手里大小鬼齐全，本手只出了其中一张
static func _breaks_rocket(play: Array[Card], player: Player.PlayerData) -> bool:
	var hand_pts := {}
	var hand_buf: Array[Card] = []
	player.collect_hand_into(hand_buf)
	for c in hand_buf:
		hand_pts[c.point] = true
	var play_pts := {}
	for c in play:
		play_pts[c.point] = true
	if not (hand_pts.has("大鬼") and hand_pts.has("小鬼")):
		return false
	return play_pts.has("大鬼") != play_pts.has("小鬼")


## 出完后剩余手牌的点数计数
static func _rest_counts(play: Array[Card], player: Player.PlayerData) -> Dictionary:
	var counts := {}
	var hand_buf: Array[Card] = []
	player.collect_hand_into(hand_buf)
	for c in hand_buf:
		counts[c.point] = counts.get(c.point, 0) + 1
	for c in play:
		counts[c.point] = counts.get(c.point, 1) - 1
	return counts


# ═══════════════════════════════════════════════════════════════
# AI2 — 贪心评分（硬编码权重，保留兼容）
# ═══════════════════════════════════════════════════════════════

static func try_finish_play(player: Player.PlayerData, all_plays: Array[Array], prev: CardTypeResult) -> Array[Card]:
	if prev == null or prev.cards.is_empty():
		if not all_plays.is_empty():
			var parsed := CardTypeResult.parse_any_card_type(all_plays.back())
			if parsed.type == CardTypeResult.CardType.GUI523:
				return all_plays.back()
	return []


static func try_endgame_intercept(player: Player.PlayerData, all_plays: Array[Array],
		prev: CardTypeResult, opp_hand_size: int) -> Array[Card]:
	if opp_hand_size > 2:
		return []
	for play in all_plays:
		var parsed := CardTypeResult.parse_any_card_type(play)
		if prev != null and not prev.cards.is_empty() and not CardTypeResult.can_beat(parsed, prev):
			continue
		if parsed.type == CardTypeResult.CardType.ROCKET or parsed.type == CardTypeResult.CardType.BOMB:
			return play
	return []


static func choose_ai2(player: Player.PlayerData, opponent: Player.PlayerData, prev: CardTypeResult,
		deck: Deck.DeckManager, table_score: int, tracker: CardTracker = null) -> Array[Card]:
	var all_plays := enumerate_legal_plays(player)
	if all_plays.is_empty():
		return []

	var finish := try_finish_play(player, all_plays, prev)
	if not finish.is_empty():
		return finish

	var intercept := try_endgame_intercept(player, all_plays, prev, opponent.hand_size())
	if not intercept.is_empty():
		return intercept

	var best: Array[Card] = []
	var best_gain := -INF
	for play in all_plays:
		var parsed := CardTypeResult.parse_any_card_type(play)
		if prev != null and not prev.cards.is_empty() and not CardTypeResult.can_beat(parsed, prev):
			continue
		## 跟牌时只用“刚好压过”的最小组合；炸弹/火箭只在没有普通解时使用。
		var gain := _greedy_gain(play, player, opponent, parsed, prev, deck, table_score, tracker)
		if gain > best_gain:
			best_gain = gain
			best = play

	return best


## 可解释的运行时贪心评分：先保证能结束/能救场，再减少拆组和高牌浪费。
static func _greedy_gain(play: Array[Card], player: Player.PlayerData,
		opponent: Player.PlayerData, parsed: CardTypeResult, previous: CardTypeResult,
		deck: Deck.DeckManager, table_score: int, tracker: CardTracker) -> float:
	var gain := 0.0
	var has_previous := previous != null and not previous.cards.is_empty()
	var is_bomb := parsed.type == CardTypeResult.CardType.BOMB \
		or parsed.type == CardTypeResult.CardType.ROCKET \
		or parsed.type == CardTypeResult.CardType.FIVE_OF_A_KIND

	## 直接清空手牌是最高优先级。
	if play.size() == player.hand_size():
		gain += 100000.0

	if has_previous:
		## 普通牌型优先于炸弹；炸弹只在对手快空牌或无普通解时使用。
		gain += 5000.0 if not is_bomb else -2500.0
		if opponent.hand_size() <= 2 and is_bomb:
			gain += 9000.0
		gain -= float(Card.get_rank(parsed.key_point)) * 20.0
	else:
		## 首出尽量出小牌并保留对子/三条/炸弹。
		gain -= float(Card.get_rank(parsed.key_point)) * 25.0
		gain -= 1800.0 if breaks_combo(play, player) else 0.0

	## 桌面有分时优先带走计分牌，但不要因此拆掉强组合。
	for c in play:
		gain += float(c.score) * (8.0 if table_score > 0 else 1.0)

	if tracker != null:
		for c in play:
			if tracker.is_exhausted(c.point):
				gain += 25.0

	## 少出一张通常优于多拆一张；清空手牌的奖励已单独覆盖。
	gain -= float(play.size()) * 2.0
	return gain


# ═══════════════════════════════════════════════════════════════
# AI 调度入口
# ═══════════════════════════════════════════════════════════════

enum Level {
	AI1_RULE,
	AI2_GREEDY,
	AI3_TRAINED,
	AI4_TRAINED,
}

## 可训练权重实例；为 null 时自动使用 default_weights()
static var weights: AIWeights = null

## 第 4 代权重槽（与 AI3 的 weights 隔离，训练期并行对比）
static var weights4: AIWeights = null

static func choose(player: Player.PlayerData, opponent: Player.PlayerData, prev: CardTypeResult,
		deck: Deck.DeckManager, table_score: int, tracker: CardTracker) -> Array[Card]:
	var play: Array[Card] = []
	match player.ai_level:
		Level.AI1_RULE:
			play = choose_ai1(player, prev)
		Level.AI2_GREEDY:
			play = choose_ai2(player, opponent, prev, deck, table_score, tracker)
		Level.AI3_TRAINED:
			if weights == null:
				weights = AIWeights.default_weights()
			play = choose_ai3(player, opponent, prev, deck, table_score, tracker, weights)
		Level.AI4_TRAINED:
			if weights4 == null:
				weights4 = AIWeights.default_weights()
			play = choose_ai3(player, opponent, prev, deck, table_score, tracker, weights4)
		_:
			play = choose_ai1(player, prev)

	if not play.is_empty() and prev != null and not prev.cards.is_empty():
		var parsed := CardTypeResult.parse_any_card_type(play)
		if not CardTypeResult.can_beat(parsed, prev):
			return []

	## 自由局空过非法：AI 决策返回空时，兜底出一张最小单张
	if play.is_empty() and (prev == null or prev.cards.is_empty()) and not player.is_hand_empty():
		var hand_buf: Array[Card] = []
		player.collect_hand_into(hand_buf)
		var min_card: Card = hand_buf[0]
		for c in hand_buf:
			if Card.get_rank(c.point) < Card.get_rank(min_card.point):
				min_card = c
		return [min_card]

	return play


# ═══════════════════════════════════════════════════════════════
# CardTracker — 记牌器（AI 插件）
# ═══════════════════════════════════════════════════════════════

class CardTracker extends RefCounted:

	const TOTAL_PER_POINT := {
		"A": 4, "K": 4, "Q": 4, "J": 4, "10": 4,
		"9": 4, "8": 4, "7": 4, "6": 4, "5": 4,
		"4": 4, "3": 4, "2": 4,
		"大鬼": 1, "小鬼": 1,
	}

	var _played: Dictionary = {}


	func reset() -> void:
		_played.clear()


	func record_played(cards: Array[Card]) -> void:
		for c in cards:
			_played[c.point] = _played.get(c.point, 0) + 1


	func remaining_count(point: String) -> int:
		return TOTAL_PER_POINT.get(point, 4) - _played.get(point, 0)


	func is_exhausted(point: String) -> bool:
		return remaining_count(point) <= 0


	func total_count(point: String) -> int:
		return TOTAL_PER_POINT.get(point, 4)


	func played_count(point: String) -> int:
		return _played.get(point, 0)


	func expected_opponent_count(point: String, opp_hand_size: int, opp_deck_size: int) -> float:
		var remaining := remaining_count(point)
		var total_unknown := float(opp_hand_size + opp_deck_size)
		if total_unknown <= 0.0:
			return 0.0
		var prob := float(opp_hand_size) / total_unknown
		return remaining * prob