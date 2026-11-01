extends "res://scripts/ui/play_input.gd"

# ═══════════════════════════════════════════════════════════════
# play — 顶层门面：创建子系统组件，桥接跨域调用
#
# 继承链：Control → play_base → play_anim → play_ui → play_input → play
#
# 委托对象（RefCounted，横向拆分）：
#   _entrance      — LevelEntrance   入口/门票/训练/重开/清理
#   _round_binding — RoundBinding    关卡启动 / Round 引擎 / 信号 / 结束
#   _ai_orch       — AIOrchestrator  AI 调度 / 延迟 / BOSS 结算
#   已有组件：
#   _dialogs       — PlayDialogs     通用弹窗
#   _main_menu     — MainMenu        主菜单
#   _guess_phase   — GuessPhase      猜先阶段
#
# 数据流：
#   _do_play / _on_pass_pressed  →  _take_round_result  →  _end_level / _show_game_over
#   AIOrchestrator.schedule(self) →  延迟 → think → process_play → 回调门面
# ═══════════════════════════════════════════════════════════════

var _entrance: LevelEntrance
var _round_binding: RoundBinding
var _ai_orch: AIOrchestrator
var _dialogs: PlayDialogs
var _main_menu: MainMenu
var _guess_phase: GuessPhase


func _ready() -> void:
	_entrance = LevelEntrance.new()
	_round_binding = RoundBinding.new()
	_ai_orch = AIOrchestrator.new()
	_dialogs = PlayDialogs.new(self)
	_main_menu = MainMenu.new(self, _dialogs)
	_guess_phase = GuessPhase.new(self)

	_setup_damage_delay_bar()
	setup_responsive()
	get_viewport().size_changed.connect(setup_responsive)
	_setup_ui_theme()

	_anim_overlay = Control.new()
	_anim_overlay.name = "AnimOverlay"
	_anim_overlay.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	_anim_overlay.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_anim_overlay.z_index = 50
	add_child(_anim_overlay)

	_drop_hint = Panel.new()
	_drop_hint.name = "DropHint"
	_drop_hint.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_drop_hint.z_index = 40
	_drop_hint.visible = false
	var hint_sb := StyleBoxFlat.new()
	hint_sb.bg_color = Color(COL_GOLD, HINT_BG_ALPHA)
	hint_sb.border_color = Color(COL_GOLD, HINT_BORDER_ALPHA)
	hint_sb.set_border_width_all(2)
	hint_sb.set_corner_radius_all(8)
	_drop_hint.add_theme_stylebox_override("panel", hint_sb)
	add_child(_drop_hint)

	_discard_hint = Panel.new()
	_discard_hint.name = "DiscardHint"
	_discard_hint.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_discard_hint.z_index = 40
	_discard_hint.visible = false
	var discard_sb := StyleBoxFlat.new()
	discard_sb.bg_color = Color(COL_CRIMSON, HINT_BG_ALPHA)
	discard_sb.border_color = Color(COL_CRIMSON, HINT_DISCARD_BORDER_ALPHA)
	discard_sb.set_border_width_all(2)
	discard_sb.set_corner_radius_all(8)
	_discard_hint.add_theme_stylebox_override("panel", discard_sb)
	add_child(_discard_hint)

	_shop_btn.pressed.connect(_on_shop_pressed)

	_state = GameState.new()
	_state.campaign = Campaign.new()
	_state.campaign.cleared_level = 0
	_state.shop = Shop.new()
	_state.tracker = AICore.CardTracker.new()
	_audio = get_node_or_null("/root/AudioSrv") as AudioManager
	_main_menu.show()


# ═══════════════════════════════════════════════════════════════
# 入口：训练 / 重开 / 回主菜单（委托 → LevelEntrance）
# ═══════════════════════════════════════════════════════════════

func _on_training_start(ai_level: int, restrictions: Dictionary, show_ai_hand: bool) -> void:
	_entrance.start_training(self, ai_level, restrictions, show_ai_hand)


func _on_reset_pressed() -> void:
	_entrance.reset_pressed(self)


func _on_back_to_menu() -> void:
	_entrance.back_to_menu(self)


func _set_op_buttons_enabled(on: bool) -> void:
	_shop_btn.disabled = not on
	_new_game_btn.disabled = not on


# ═══════════════════════════════════════════════════════════════
# 出牌 / 不要（桥接 play_input → AIOrchestrator）
# ═══════════════════════════════════════════════════════════════

func _do_play(indices: Array[int]) -> bool:
	if _state.finished or _invalid_anim_running or not is_instance_valid(_state.round) or _state.round.whose_turn() != _state.human:
		return false
	_invalid_reason = ""
	if indices.is_empty():
		return false
	for k in range(indices.size() - 1):
		if indices.find(indices[k], k + 1) != -1:
			_invalid_reason = "重复选择同一张牌"
			return false
	indices.sort()
	var play: Array[Card] = []
	var n := _state.human.normal_hand.size()
	for i in indices:
		if i < n:
			play.append(_state.human.normal_hand[i])
		else:
			var ji := i - n
			if ji < _state.human.instant_hand.size():
				play.append(_state.human.instant_hand[ji])
	if play.is_empty():
		return false
	play.sort_custom(func(a: Card, b: Card): return Card.get_rank(a.point) < Card.get_rank(b.point))
	var parsed := CardTypeResult.parse_any_card_type(play)
	var reason := ""
	var lr: Campaign.LevelRule = _state.round.level_rule
	if parsed.type == CardTypeResult.CardType.INVALID:
		reason = "非法牌型"
	elif lr != null and lr.ban_gui523 and parsed.type == CardTypeResult.CardType.GUI523:
		reason = "本关禁止打出 7gui523"
	elif _state.round.player_blocked_by_ghost(parsed):
		reason = "赖账鬼禁同点"
	else:
		var lp := _state.round.last_play
		if lp != null and not lp.cards.is_empty() and not CardTypeResult.can_beat(parsed, lp):
			reason = "无法压过"
	if not reason.is_empty():
		_invalid_reason = reason
		return false
	var result := _state.round.process_play(play)
	_take_round_result(result)
	_refresh("你出 %d 张" % play.size())
	if result.state == "boss_finish":
		await _resolve_boss_finish(result.pot, result.penalty)
		return true
	_ai_orch.schedule(self)
	return true


func _on_pass_pressed() -> void:
	if _state.finished:
		return
	if _invalid_anim_running:
		return
	if not is_instance_valid(_state.round):
		return
	if _state.round.whose_turn() != _state.human:
		return
	if _state.player_can_discard and not _state.selected.is_empty():
		var indices := _state.selected.duplicate()
		if _discard_cards(indices, false):
			_state.selected.clear()
		return
	var lp := _state.round.last_play
	if lp == null or lp.cards.is_empty():
		_refresh_status("首出不能不要")
		return
	_state.selected.clear()
	var result := _state.round.process_play([])
	_take_round_result(result)
	_refresh("你选择不要")
	_ai_orch.schedule(self)


# ═══════════════════════════════════════════════════════════════
# 结果分发（桥接 AIOrchestrator → RoundBinding）
# ═══════════════════════════════════════════════════════════════

func _take_round_result(result: Round.PlayResult) -> void:
	var state: String = result.state
	var reason: String = result.reason
	if reason.is_empty():
		reason = state
	match state:
		"invalid", "cannot_beat":
			_refresh_status("操作被拒绝：%s" % reason)
		"instant_win":
			var winner := result.winner
			if winner != null:
				var player_won := winner == _state.human
				var human_failed := _state.round != null and _state.round.failed_players.has(_state.human)
				_state.campaign.sync_from_battle(_state.human, _state.ai)
				if human_failed:
					_end_level(false, true)
				elif player_won and _state.campaign.current_level >= _state.campaign.config.total_levels:
					_end_level(false)
				else:
					_end_level(player_won)


func _resolve_boss_finish(pot: int = 0, penalty: int = 0) -> void:
	await _ai_orch._resolve_boss_finish(self, pot, penalty)


func _end_level(player_won: bool, player_failed: bool = false) -> void:
	_round_binding.end_level(self, player_won, player_failed)


func _show_game_over(title: String, detail: String) -> void:
	_round_binding.show_game_over(self, title, detail)


# ═══════════════════════════════════════════════════════════════
# 按钮回调
# ═══════════════════════════════════════════════════════════════

func _on_shop_pressed() -> void:
	if _state.phase == "game_over":
		return
	if _state.phase == "shop":
		_show_shop_panel()
	elif _state.phase == "battle":
		_refresh_status("战斗中无法打开商店，请在关末进入")
	else:
		_show_shop_panel()


func _on_next_level() -> void:
	if _state.phase != "shop":
		return
	_hide_shop_panel()
	if _state.campaign.current_level >= _state.campaign.config.total_levels:
		_show_game_over("最终挑战结束", "赌场老板未被击败，无法通关。")
		return
	_round_binding.start_level(self, _state.campaign.current_level + 1)