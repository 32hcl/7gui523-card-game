class_name AIOrchestrator
extends RefCounted

# ═══════════════════════════════════════════════════════════════
# AIOrchestrator — AI 思考调度、延迟触发、结果分发
#
# 由 play.gd 门面创建，负责：
#   schedule()    — 延迟触发 AI 思考
#   think()       — 执行 AI 选牌 + process_play
#   resolve_boss_finish() — BOSS 出完结算动画
# ═══════════════════════════════════════════════════════════════

const BOSS_AI := preload("res://scripts/ai/boss_ai.gd")


func schedule(base: Control) -> void:
	var state: GameState = base._state
	if state.finished:
		return
	if state.round == null:
		return
	var cur := state.round.whose_turn()
	if not cur.is_human and not state.ai_busy:
		state.ai_busy = true
		base._round_lbl.text = "AI 思考中…"
		base._round_lbl.add_theme_color_override("font_color", base.COL_TEXT)
		var t := base.get_tree().create_timer(base.AI_THINK_DELAY)
		t.timeout.connect(func(): _on_think_timer(base))


func _on_think_timer(base: Control) -> void:
	var state: GameState = base._state
	state.ai_busy = false
	if state.finished or state.round == null:
		return
	var gen := state.flow_generation
	var cur := state.round.whose_turn()
	if cur == state.ai:
		var buf: Array[Card] = []
		state.ai.collect_hand_into(buf)
		if state.round.level_rule:
			state.round.level_rule.mutate_boss_hand(buf)
	var lp := state.round.last_play
	var table_score := state.round.table_score
	var play: Array[Card] = BOSS_AI.choose(cur, lp, table_score)
	if play.is_empty() and lp != null and not lp.cards.is_empty() and not cur.is_hand_empty():
		var cur_hand_buf: Array[Card] = []
		cur.collect_hand_into(cur_hand_buf)
		var forced := state.round.ghost_force_play(cur_hand_buf)
		if not forced.is_empty():
			play = forced
	var result := state.round.process_play(play)
	base._take_round_result(result)
	base._refresh("AI 出 %d 张" % play.size())
	if result.state == "boss_finish":
		await _resolve_boss_finish(base, result.pot, result.penalty)
		if gen != state.flow_generation:
			return
		return
	schedule(base)


func _resolve_boss_finish(base: Control, pot: int = 0, penalty: int = 0) -> void:
	var state: GameState = base._state
	if state.finished:
		return
	var gen := state.flow_generation
	await base._play_damage_delay()
	if gen != state.flow_generation:
		return
	var human_failed := state.round != null and state.round.failed_players.has(state.human)
	state.campaign.sync_from_battle(state.human, state.ai)
	base._refresh("BOSS 出完牌：桌面 %d 归对方，剩余牌惩罚 %d，你的筹码 %d%s" % [
		pot, penalty, state.human.chips, "（还不起负债）" if human_failed else ""])
	base._end_level(not human_failed, human_failed)