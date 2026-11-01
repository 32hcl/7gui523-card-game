class_name RoundBinding
extends RefCounted

# ═══════════════════════════════════════════════════════════════
# RoundBinding — Round 引擎生命周期、信号管理、关卡结束判定
#
# 由 play.gd 门面创建，负责：
#   关卡启动  — start_level / setup_level_players / prepare_level_rule
#   Round 引擎 — create_round / teardown_round
#   发牌动画  — begin_deal_animation
#   信号处理  — on_card_played / on_round_won 等 9 个处理器
#   关卡结束  — end_level / show_game_over
# ═══════════════════════════════════════════════════════════════


func start_level(base: Control, level: int) -> void:
	var state: GameState = base._state
	var generation := state.flow_generation
	state.phase = "battle"
	base._game_over_layer.visible = false

	state.campaign.start_level(level)
	state.shop.current_level = level
	state.tracker.reset()

	_setup_level_players(base, level)
	_reset_level_anim_state(base)
	var rule := _prepare_level_rule(base, level)

	base._set_op_buttons_enabled(false)
	var player_first: bool = await base._guess_phase.run()
	if generation != state.flow_generation:
		return
	base._set_op_buttons_enabled(true)

	create_round(base, player_first, rule)
	base._title_lbl.text = state.ai.name
	await _begin_deal_animation(base)
	if generation != state.flow_generation:
		return


func _setup_level_players(base: Control, level: int) -> void:
	var state: GameState = base._state

	if state.human_deck == null:
		state.human_deck = Deck.DeckManager.new()
		state.human_deck.init_standard()
	else:
		state.human_deck.refill_active()

	state.instant_deck = Deck.DeckManager.new()
	state.instant_deck.init_instant()
	state.instant_deck.loop_active = true

	state.ai_deck = Deck.DeckManager.new()
	state.ai_deck.init_standard()
	state.ai_deck.refill_active()

	state.human = Player.PlayerData.new()
	state.human.name = "32%Hcl"
	state.human.is_human = true
	state.human.ai_level = AICore.Level.AI1_RULE
	state.human.card_sort_mode = base._card_sort_mode
	for card in state.campaign.pending_hand_cards:
		if state.human.normal_hand.size() < GameConfig.NORMAL_MAX:
			state.human.normal_hand.append(card)
		else:
			state.human_deck.add_to_hidden(card)
	state.campaign.pending_hand_cards.clear()
	state.human_deck.fill_player_normal(state.human)
	state.instant_deck.fill_player_instant(state.human)

	state.ai = Player.PlayerData.new()
	if base._training.is_active and base._training.ai_override_level >= 0:
		state.ai.name = base._training.ai_display_name()
		state.ai.ai_level = base._training.ai_override_level
	else:
		state.ai.name = Campaign.get_boss_name(level) + "·BOSS"
		state.ai.ai_level = AICore.Level.AI2_GREEDY
	state.ai.is_human = false
	state.ai.card_sort_mode = base._card_sort_mode
	state.ai_deck.fill_player_normal(state.ai)
	state.instant_deck.fill_player_instant(state.ai)

	base._apply_pixelation()

	state.human.reset_battle_chips(state.campaign.player_chips)
	state.ai.reset_battle_chips(state.campaign.boss_chips)
	state.finished = false
	state.ai_busy = false


func _reset_level_anim_state(base: Control) -> void:
	base._table_anim_running = false
	base._table_anim_pending = false
	base._table_cards_anim.clear()
	base._table_render_sig = "@init"
	base._play_src_global = Vector2.ZERO
	base._state.selected.clear()
	base._state.player_can_discard = false
	base._state.player_discard_count = 0
	base._state.player_discarded_this_round = false
	base._clear_children(base._table_cards)
	base._clear_children(base._anim_overlay)


func _prepare_level_rule(base: Control, level: int) -> Campaign.LevelRule:
	var state: GameState = base._state
	var rule: Campaign.LevelRule = Campaign.level_rule(level)
	if base._training.is_active:
		rule = base._training.build_rule()
	state.mask_player_draws = rule.mask_player_draws
	state.initial_deal_finished = false
	state.masked_player_cards.clear()
	var ai_hand_buf: Array[Card] = []
	state.ai.collect_hand_into(ai_hand_buf)
	rule.mutate_boss_hand(ai_hand_buf)
	return rule


func create_round(base: Control, player_first: bool, rule: Campaign.LevelRule) -> void:
	var state: GameState = base._state
	var first := state.human if player_first else state.ai
	var second := state.ai if player_first else state.human
	var first_deck := state.human_deck if player_first else state.ai_deck
	var second_deck := state.ai_deck if player_first else state.human_deck
	var shared_instant_deck := state.instant_deck
	state.round = Round.new(first, second, first_deck, second_deck,
		shared_instant_deck, shared_instant_deck, state.ai, rule)
	state.round.pot_to_boss = state.campaign.current_level >= state.campaign.config.total_levels
	state.round.round_started.connect(func(): base._refresh(""), CONNECT_ONE_SHOT)
	state.round.turn_changed.connect(func(who): base._refresh_status("轮到 %s" % who.name))
	state.round.round_won.connect(func(winner, pot): _on_round_won(base, winner, pot))
	state.round.card_played.connect(func(who, cards): _on_card_played(base, who, cards))
	state.round.card_passed.connect(func(who): base._refresh_status("%s 不要" % who.name))
	state.round.pressure_chips.connect(func(who, amt): _on_pressure_chips(base, who, amt))
	state.round.instant_stake_chips.connect(func(who, amt): _on_instant_stake_chips(base, who, amt))
	state.round.round_ante.connect(func(who, amt): _on_round_ante(base, who, amt), CONNECT_ONE_SHOT)
	state.round.game_over.connect(func(): state.finished = true, CONNECT_ONE_SHOT)

	state.round.card_played.connect(func(_who, _cards): base._audio.play_sfx("card_play"))
	state.round.card_passed.connect(func(_who): base._audio.play_sfx("card_pass"))
	state.round.round_won.connect(func(_winner, _dmg): base._audio.play_sfx("round_win"), CONNECT_ONE_SHOT)
	state.round.game_over.connect(func(): base._audio.play_sfx("game_over"), CONNECT_ONE_SHOT)


func _begin_deal_animation(base: Control) -> void:
	var state: GameState = base._state
	for row: HBoxContainer in [base._top_hand, base._bottom_hand, base._boss_hand, base._boss_hand_instant]:
		base._clear_children(row)
	base._boss_hand.set_meta("_boss_known_normal", 0)
	base._boss_hand_instant.set_meta("_boss_known_instant", 0)
	base._refresh_status("新关卡开始")
	base._refresh_turn()
	base._refresh_top()

	await base.get_tree().process_frame
	base._setup_deck_display()
	base._play_deck_shuffle()
	base._refresh_top()
	base._refresh_hand_with_animation(true)
	base._refresh_boss_hand_with_animation(true)
	await base.get_tree().create_timer(base.INITIAL_DEAL_DURATION + base.INITIAL_DEAL_DELAY * 6.0).timeout
	if state.mask_player_draws:
		for c in state.human.normal_hand:
			state.masked_player_cards[c] = false
		for c in state.human.instant_hand:
			state.masked_player_cards[c] = false
	state.initial_deal_finished = true
	base._ai_orch.schedule(base)


func teardown_round(base: Control) -> void:
	if base._state.round == null:
		return
	for sig in base._state.round.get_signal_list():
		var sig_name: String = sig["name"]
		for c in base._state.round.get_signal_connection_list(sig_name):
			base._state.round.disconnect(sig_name, c["callable"])
	base._state.round = null


# ═══════════════════════════════════════════════════════════════
# Round 信号处理器
# ═══════════════════════════════════════════════════════════════

func _on_card_played(base: Control, who: Player.PlayerData, cards: Array[Card]) -> void:
	base._state.tracker.record_played(cards)
	if who == base._state.human:
		base._play_src_global = base._hand_area_center()
	else:
		base._play_src_global = base._boss_src_center(cards)
	base._table_anim_pending = true


func _on_pressure_chips(base: Control, who: Player.PlayerData, amount: int) -> void:
	if amount > 0:
		base._spawn_floating_chips(amount, "压牌")
	base._refresh_status("%s 压牌 +%d 筹码" % [who.name, amount])


func _on_instant_stake_chips(base: Control, who: Player.PlayerData, amount: int) -> void:
	if amount > 0:
		base._spawn_floating_chips(amount, "立刻补区")


func _on_round_ante(base: Control, who: Player.PlayerData, amount: int) -> void:
	if who == base._state.human and amount > 0:
		base._spawn_floating_chips(amount, "底注")


func _on_round_won(base: Control, winner: Player.PlayerData, pot: int) -> void:
	var state: GameState = base._state
	state.player_discarded_this_round = false
	state.player_can_discard = (winner == state.ai)
	state.campaign.sync_from_battle(state.human, state.ai)

	var boss_won := winner == state.ai
	var msg: String
	if state.round != null and state.round.pot_to_boss:
		msg = "★ 第 9 关：桌面 %d 筹码归 BOSS（你的筹码 %d）" % [pot, state.human.chips]
	elif not boss_won:
		msg = "★ 你赢得小局：桌面 %d 筹码归你（你的筹码 %d）" % [pot, state.human.chips]
	else:
		msg = "★ %s 赢得小局：桌面 %d 筹码归对方（你的筹码 %d）" % [winner.name, pot, state.human.chips]
	var human_failed := state.round != null and state.round.failed_players.has(state.human)
	var ai_failed := state.round != null and state.round.failed_players.has(state.ai)
	if human_failed or ai_failed:
		msg += "（还不起负债）"
	base._refresh_status(msg)

	if human_failed or ai_failed:
		end_level(base, not human_failed, human_failed)


func end_level(base: Control, player_won: bool, player_failed: bool = false) -> void:
	var state: GameState = base._state
	state.phase = "level_end"
	state.finished = true

	if state.round != null and state.round.level_rule != null:
		state.round.level_rule.reset_boss_swaps()

	if base._training.is_active:
		state.human_deck.flush_recycle_buffer()
		for card in state.human.normal_hand:
			state.human_deck.add_to_hidden(card)
		var title: String
		if player_won:
			var net := state.human.chips - GameConfig.INITIAL_CHIPS
			title = "训练通过！净筹码 %+d" % net
		else:
			title = "训练失败"
		show_game_over(base, title, "按「再来一局」用同样配置重新训练")
		return

	state.human_deck.flush_recycle_buffer()
	for card in state.human.normal_hand:
		state.human_deck.add_to_hidden(card)

	state.campaign.sync_from_battle(state.human, state.ai)
	var result := state.campaign.finish_level(player_won, player_failed)
	var accepted_player_won := result.player_won
	base._update_deck_empty(base._player_deck if not accepted_player_won else base._boss_deck, true)

	var msg: String
	if accepted_player_won:
		msg = "👑 第 %d 关通过！+%d 筹码 · 剩余 %d" % \
			[state.campaign.current_level, result.chips_delta, result.chips_after]
	else:
		msg = "☠ 第 %d 关失败… 剩余筹码 %d" % [state.campaign.current_level, result.chips_after]

	base._round_lbl.text = msg
	base._round_lbl.add_theme_color_override("font_color", base.COL_GOLD)

	if state.campaign.current_level >= state.campaign.config.total_levels:
		show_game_over(base, "最终挑战结束", "赌场老板未被击败，无法通关。最终筹码 %d" % state.campaign.player_chips)
	elif not accepted_player_won:
		show_game_over(base, "挑战失败", "你被击败了… 筹码 %d" % state.campaign.player_chips)
	else:
		state.shop.on_level_end()
		state.phase = "shop"
		base._refresh("关末商店")
		base._show_shop_panel()


func show_game_over(base: Control, title: String, detail: String) -> void:
	base._state.phase = "game_over"
	base._shop_btn.disabled = true
	base._game_over_layer.visible = true
	base._game_over_msg.text = "%s\n%s" % [title, detail]
	base._round_lbl.text = "游戏结束"