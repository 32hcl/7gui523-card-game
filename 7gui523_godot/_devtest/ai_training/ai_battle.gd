class_name AIBattleSim
extends RefCounted

# ═══════════════════════════════════════════════════════════════
# AIBattleSim — 无头对局模拟器
# 跑一局完整游戏（洗牌→发牌→出牌循环→结算），返回胜者和指标
# ═══════════════════════════════════════════════════════════════

class BattleResult:
	var winner: Player.PlayerData
	var loser: Player.PlayerData
	var winner_ai_level: int
	var loser_ai_level: int
	var total_tricks: int         ## 总局数（round_win 次数）
	var winner_damage: int        ## 胜者累计造成的伤害
	var loser_damage: int         ## 负者累计造成的伤害
	var win_by_instant: bool      ## 是否 GUI523 / 空手+空牌堆 直接胜
	var winner_remaining: int     ## 胜者剩余手牌数
	var loser_remaining: int      ## 负者剩余手牌数


## 打一局完整对局
## 返回 BattleResult 或 null（出错时）
static func simulate(player_a: Player.PlayerData, player_b: Player.PlayerData) -> BattleResult:
	var a := _clone_player(player_a)
	var b := _clone_player(player_b)
	a.ai_level = player_a.ai_level
	b.ai_level = player_b.ai_level

	var deck_a := Deck.DeckManager.new()
	var deck_b := Deck.DeckManager.new()
	deck_a.init_standard()
	deck_b.init_standard()

	deck_a.fill_player_normal(a)
	deck_b.fill_player_normal(b)

	var tracker_a := AICore.CardTracker.new()
	var tracker_b := AICore.CardTracker.new()

	var round := Round.new(a, b, deck_a, deck_b)
	var result := BattleResult.new()
	result.winner_ai_level = 0
	result.loser_ai_level = 0

	var max_moves := 500
	var move_count := 0

	while move_count < max_moves:
		move_count += 1
		var cur := round.whose_turn()
		var is_a := cur == a
		var opp := b if is_a else a
		var tracker := tracker_a if is_a else tracker_b
		var opp_tracker := tracker_b if is_a else tracker_a
		var cur_deck := deck_a if is_a else deck_b
		var prev := round.last_play
		var ts := round.table_score

		var play: Array[Card] = AICore.choose(cur, opp, prev, cur_deck, ts, tracker)
		var res: Dictionary = round.process_play(play)

		if not play.is_empty():
			tracker.record_played(play)
			opp_tracker.record_played(play)

		match res.get("state", ""):
			"round_win":
				result.total_tricks += 1
				if res["winner"] == a:
					result.winner_damage += res["damage"]
				else:
					result.loser_damage += res["damage"]
				continue

			"instant_win":
				var winner = res["winner"] as Player.PlayerData
				result.win_by_instant = true
				result.winner = winner
				result.loser = b if winner == a else a
				result.winner_ai_level = result.winner.ai_level
				result.loser_ai_level = result.loser.ai_level
				result.winner_remaining = result.winner.hand_size()
				result.loser_remaining = result.loser.hand_size()
				return result

			"invalid", "cannot_beat":
				var loser = cur
				var winner = opp
				result.winner = winner
				result.loser = loser
				result.winner_ai_level = winner.ai_level
				result.loser_ai_level = loser.ai_level
				result.winner_remaining = winner.hand_size()
				result.loser_remaining = loser.hand_size()
				return result

			_:
				continue

	# 超时 → 比较伤害判定胜负
	var a_dmg := result.winner_damage if result.winner_damage > 0 else result.loser_damage
	var b_dmg := result.loser_damage
	if a_dmg >= b_dmg:
		result.winner = a
		result.loser = b
	else:
		result.winner = b
		result.loser = a
	result.winner_ai_level = result.winner.ai_level
	result.loser_ai_level = result.loser.ai_level
	result.winner_remaining = result.winner.hand_size()
	result.loser_remaining = result.loser.hand_size()
	return result


static func _clone_player(src: Player.PlayerData) -> Player.PlayerData:
	var p := Player.PlayerData.new()
	p.name = src.name
	p.is_human = src.is_human
	p.ai_level = src.ai_level
	return p