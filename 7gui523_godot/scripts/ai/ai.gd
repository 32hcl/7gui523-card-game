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
	var n := player.hand.size()
	for mask in range(1, 1 << n):
		var subset: Array[Card] = []
		for i in n:
			if mask & (1 << i):
				subset.append(player.hand[i])
		var parsed := CardTypeResult.parse_any_card_type(subset)
		if parsed.type != CardTypeResult.CardType.INVALID and parsed.type != CardTypeResult.CardType.PASS:
			result.append(subset)
	return result


# ═══════════════════════════════════════════════════════════════
# 检查是否拆对/拆三/拆四
# ═══════════════════════════════════════════════════════════════

static func breaks_combo(play: Array[Card], player: Player.PlayerData) -> bool:
	var hand_count := {}
	for c in player.hand:
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
	if opponent.hand.is_empty():
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
		var might := tracker.expected_opponent_count(pt, opponent.hand.size(), deck.cards.size())
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
	if play.size() == player.hand.size():
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

	if opponent.hand.size() <= 2:
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
		if player.hand.is_empty():
			return []
		var sorted := player.hand.duplicate()
		sorted.sort_custom(func(a: Card, b: Card): return Card.get_rank(a.point) < Card.get_rank(b.point))
		var first_point: String = sorted[0].point
		var combo: Array[Card] = []
		for c in player.hand:
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
# AI2 — 贪心评分
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
		deck: Deck.DeckManager, table_score: int) -> Array[Card]:
	var all_plays := enumerate_legal_plays(player)
	if all_plays.is_empty():
		return []

	var finish := try_finish_play(player, all_plays, prev)
	if not finish.is_empty():
		return finish

	var intercept := try_endgame_intercept(player, all_plays, prev, opponent.hand.size())
	if not intercept.is_empty():
		return intercept

	var best: Array[Card] = []
	var best_gain := -0x7FFFFFFF
	for play in all_plays:
		var parsed := CardTypeResult.parse_any_card_type(play)
		if prev != null and not prev.cards.is_empty() and not CardTypeResult.can_beat(parsed, prev):
			continue
		var gain := play_gain(play, player, opponent, deck, prev, table_score, null)
		if gain > best_gain:
			best_gain = gain
			best = play

	return best


# ═══════════════════════════════════════════════════════════════
# AI 调度入口
# ═══════════════════════════════════════════════════════════════

enum Level {
	AI1_RULE,
	AI2_GREEDY,
}

static func choose(player: Player.PlayerData, opponent: Player.PlayerData, prev: CardTypeResult,
		deck: Deck.DeckManager, table_score: int, tracker: CardTracker) -> Array[Card]:
	var play: Array[Card] = []
	match player.ai_level:
		Level.AI1_RULE:
			play = choose_ai1(player, prev)
		Level.AI2_GREEDY:
			play = choose_ai2(player, opponent, prev, deck, table_score)
		_:
			play = choose_ai1(player, prev)

	if not play.is_empty() and prev != null and not prev.cards.is_empty():
		var parsed := CardTypeResult.parse_any_card_type(play)
		if not CardTypeResult.can_beat(parsed, prev):
			return []

	## 自由局空过非法：AI 决策返回空时，兜底出一张最小单张
	if play.is_empty() and (prev == null or prev.cards.is_empty()) and not player.hand.is_empty():
		var min_card: Card = player.hand[0]
		for c in player.hand:
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