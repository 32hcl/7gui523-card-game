extends PlayUI

## — 第 4 层 play：入口 · 关卡流程 · Round 信号 · AI · 拖拽
##
## 继承链顶层的正式对战主场景。
## 双牌堆 + Round 引擎 + AI，玩家选牌出牌，桌面伤害结算。


func _ready() -> void:
	_setup_ui_theme()
	_anim_overlay = Control.new()
	_anim_overlay.name = "AnimOverlay"
	_anim_overlay.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	_anim_overlay.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_anim_overlay.z_index = 50
	add_child(_anim_overlay)

	## 拖拽落点提示
	_drop_hint = Panel.new()
	_drop_hint.name = "DropHint"
	_drop_hint.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_drop_hint.z_index = 40
	_drop_hint.visible = false
	var hint_sb := StyleBoxFlat.new()
	hint_sb.bg_color = Color(1.0, 0.83, 0.25, 0.10)
	hint_sb.border_color = Color(1.0, 0.83, 0.25, 0.65)
	hint_sb.set_border_width_all(2)
	hint_sb.set_corner_radius_all(8)
	_drop_hint.add_theme_stylebox_override("panel", hint_sb)
	add_child(_drop_hint)

	## 连接额外按钮信号（场景中未预置 connection）
	_shop_btn.pressed.connect(_on_shop_pressed)
	_hp_swap_btn.pressed.connect(_on_hp_swap_pressed)
	_return_btn.pressed.connect(_on_return_pressed)

	_campaign = Campaign.new()
	_campaign.player_chips = 50   ## 起始筹码
	_campaign.cleared_level = 0
	_shop = Shop.new()
	_tracker = AICore.CardTracker.new()
	_enter_casino_then_start(_campaign.current_level)


func _on_reset_pressed() -> void:
	_teardown_round()
	_campaign.start_new()
	_campaign.player_chips = 50
	_shop.reset_stock()
	_enter_casino_then_start(_campaign.current_level)


## 进入赌场前购买一次金币门票；门票不替代每关的筹码消耗。
func _enter_casino_then_start(level: int) -> void:
	if _campaign.has_purchased_entry:
		_start_level(level)
		return

	if _campaign.dollar < Campaign.ENTRY_TICKET_DOLLARS:
		_show_game_over(
			"金币不足",
			"进入赌场需要 %d 金币\n当前金币：%d" % [Campaign.ENTRY_TICKET_DOLLARS, _campaign.dollar])
		return

	var dialog := ConfirmationDialog.new()
	dialog.title = "鸿运赌场 · 入场"
	dialog.dialog_text = "入场需要 %d 金币。\n购买门票后，另需每关消耗筹码。\n当前金币：%d\n\n是否购买赌场门票？" % [Campaign.ENTRY_TICKET_DOLLARS, _campaign.dollar]
	dialog.ok_button_text = "购买门票"
	dialog.cancel_button_text = "暂不进入"
	add_child(dialog)
	dialog.confirmed.connect(func():
		if _campaign.purchase_entry_ticket():
			_log("已购买赌场门票（-%d 金币）" % Campaign.ENTRY_TICKET_DOLLARS)
			_start_level(level)
		dialog.queue_free()
	)
	dialog.canceled.connect(func(): dialog.queue_free())
	dialog.popup_centered()


# ═══════════════════════════════════════════════════════════════
# 关卡生命周期
# ═══════════════════════════════════════════════════════════════

func _start_level(level: int) -> void:
	_phase = "battle"
	_game_over_layer.visible = false

	var entry_chips := Campaign.get_level_entry_chips(level)
	if _campaign.player_chips < entry_chips:
		_show_game_over("筹码不足", "第 %d 关需要 %d 筹码\n当前筹码：%d" % [level, entry_chips, _campaign.player_chips])
		return
	_campaign.player_chips -= entry_chips

	_campaign.start_level(level)
	_tracker.reset()

	_human_deck = Deck.DeckManager.new()
	_ai_deck = Deck.DeckManager.new()
	_human_deck.init_standard()
	_ai_deck.init_standard()

	_human = Player.PlayerData.new()
	_human.name = "真神"
	_human.is_human = true
	_human.ai_level = AICore.Level.AI1_RULE
	_human_deck.fill_player_normal(_human)
	_human_deck.fill_player_instant(_human)

	_ai = Player.PlayerData.new()
	_ai.name = Campaign.get_boss_name(level) + "·BOSS"
	_ai.is_human = false
	_ai.ai_level = AICore.Level.AI2_GREEDY
	_ai_deck.fill_player_normal(_ai)
	_ai_deck.fill_player_instant(_ai)

	_human_hp = _campaign.player_hp
	_ai_hp = _campaign.boss_hp
	_finished = false
	_ai_busy = false
	## 重置桌面/手牌动画状态：若上一关动画协程因关卡切换中断，会残留 running=true，
	## 导致新关卡 _refresh_table 开头直接 return、所有出牌/补牌/发牌动画不播放
	## （"第一回合好，后面就有问题"的根因）。新关必须清零以便开启动画。
	_table_anim_running = false
	_table_anim_pending = false
	_table_cards_anim.clear()
	_play_src_global = Vector2.ZERO
	_selected.clear()
	_log_lines.clear()

	var rule = Campaign.level_rule(level)
	## 猜牌决定先手；猜先期间隔离操作按钮
	_set_op_buttons_enabled(false)
	var player_first := await _run_guess_phase()
	_set_op_buttons_enabled(true)
	var first := _human if player_first else _ai
	var second := _ai if player_first else _human
	var first_deck := _human_deck if player_first else _ai_deck
	var second_deck := _ai_deck if player_first else _human_deck
	_round = Round.new(first, second, first_deck, second_deck, _ai, rule)
	_round.round_started.connect(func(): _refresh(""))
	_round.turn_changed.connect(func(who): _refresh_status("轮到 %s" % who.name))
	_round.round_won.connect(_on_round_won)
	_round.card_played.connect(_on_card_played)
	_round.card_passed.connect(func(who): _refresh_status("%s 不要" % who.name))
	_round.pressure_bonus.connect(func(who, bonus): _refresh_status("%s 压迫加分 %d" % [who.name, bonus]))
	_round.game_over.connect(func(): _finished = true)

	_title_lbl.text = _ai.name
	_log("—— 第 %d 关 · %s ——" % [level, Campaign.get_boss_name(level)])
	_log("入场筹码 -%d，奖励 +%d" % [entry_chips, Campaign.get_level_reward_chips(level)])
	_log("猜牌决定先手：" + ("你先手" if player_first else "电脑先手"))

	## 开局发牌动画：先清空，再逐张飞入
	for child in _player_hand.get_children():
		child.queue_free()
	for child in _boss_hand.get_children():
		child.queue_free()
	_refresh_status("新关卡开始")
	_refresh_turn()
	_refresh_top()

	## 延迟一帧，让容器布局稳定后再开启动画
	await get_tree().process_frame
	_setup_deck_display()
	_refresh_top()
	_refresh_hand_with_animation(true)
	_refresh_boss_hand_with_animation(true)
	_schedule_ai_if_needed()


## 统一开关操作按钮（猜先等流程隔离用）
func _set_op_buttons_enabled(on: bool) -> void:
	_play_btn.disabled = not on
	_pass_btn.disabled = not on
	_shop_btn.disabled = not on
	_hp_swap_btn.disabled = not on
	_return_btn.disabled = not on
	_new_game_btn.disabled = not on


## 猜先阶段 — 猜对玩家先手，猜错电脑先手
func _run_guess_phase() -> bool:
	var zone: Control = $MainLayout/LeftPanel/TableZone
	var overlay := Control.new()
	overlay.name = "GuessOverlay"
	overlay.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	overlay.z_index = 60
	overlay.mouse_filter = Control.MOUSE_FILTER_IGNORE
	zone.add_child(overlay)

	## 点击隔离层：全透明，桌面背景自然透出（不做压暗/覆盖背景）
	## 收鼠标事件的兜底层：拦下所有点击，防止穿透到下层手牌/桌面；
	## 面板与按钮位于其上方，仍优先响应。
	var dim := ColorRect.new()
	dim.color = Color(0.0, 0.0, 0.0, 0.0)
	dim.mouse_filter = Control.MOUSE_FILTER_STOP
	dim.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	overlay.add_child(dim)

	## 中央面板（承载标题/按钮/牌位）：透明镂空，桌面背景直接透出；无底色无金框
	## 用 PASS：自身不拦截，确保点击能正常分发到面板内的按钮；
	## 若命中面板空白处则穿透，由下层 dim(STOP) 兜底拦截不外泄。
	var panel := Panel.new()
	panel.mouse_filter = Control.MOUSE_FILTER_PASS
	panel.add_theme_stylebox_override("panel", flat_border(
		Color(0, 0, 0, 0), Color(1, 1, 1, 0), 0, 0,
		Color(0, 0, 0, 0), 0, 0))
	panel.set_anchors_and_offsets_preset(Control.PRESET_CENTER)
	panel.offset_left = -150
	panel.offset_top = -160
	panel.offset_right = 150
	panel.offset_bottom = 160
	panel.custom_minimum_size = Vector2(300, 320)
	overlay.add_child(panel)

	var outer := VBoxContainer.new()
	outer.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	panel.add_child(outer)

	## 标题
	var title := Label.new()
	title.text = "猜牌决定先手"
	title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	title.add_theme_font_size_override("font_size", 20)
	title.add_theme_color_override("font_color", Color(1.0, 0.84, 0.42))
	title.add_theme_color_override("font_outline_color", Color(0.3, 0.2, 0.08, 1.0))
	title.add_theme_constant_override("outline_size", 5)
	outer.add_child(title)

	## 牌位：点击前显示牌背，翻牌后用伪3D 立牌翻面揭晓大鬼/小鬼
	var card_vis := CardVisual.new(Vector2(100, 140))
	card_vis.show_back()
	card_vis.enable_fake3d(true)
	card_vis.mouse_filter = Control.MOUSE_FILTER_IGNORE
	card_vis.custom_minimum_size = Vector2(100, 140)
	card_vis.size_flags_horizontal = Control.SIZE_SHRINK_CENTER
	outer.add_child(card_vis)

	## 按钮行
	var mid_row := HBoxContainer.new()
	mid_row.alignment = BoxContainer.ALIGNMENT_CENTER
	mid_row.add_theme_constant_override("separation", 24)
	var big_btn := Button.new()
	big_btn.text = "猜大鬼"
	big_btn.custom_minimum_size = Vector2(120, 46)
	var small_btn := Button.new()
	small_btn.text = "猜小鬼"
	small_btn.custom_minimum_size = Vector2(120, 46)
	style_button_casino(big_btn, Color(0.90, 0.24, 0.30), Color(1.0, 0.84, 0.42))
	style_button_casino(small_btn, Color(0.20, 0.26, 0.42), Color(1.0, 0.84, 0.42))
	mid_row.add_child(big_btn)
	mid_row.add_child(small_btn)
	outer.add_child(mid_row)

	## 提示小字
	var hint := Label.new()
	hint.text = "大鬼（上）与小鬼（下）其中一张，猜对先手"
	hint.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	hint.add_theme_font_size_override("font_size", 12)
	hint.add_theme_color_override("font_color", Color(0.8, 0.74, 0.6))
	outer.add_child(hint)

	## 等待玩家点击（结果写入局部容器，竞态安全）
	var result := [0]   ## 0=未选 , 1=猜大鬼 , 2=猜小鬼
	big_btn.pressed.connect(func():
		result[0] = 1)
	small_btn.pressed.connect(func():
		result[0] = 2)
	while result[0] == 0:
		await get_tree().process_frame

	var guessed_big: bool = result[0] == 1
	## 隐藏两个按钮，进入"揭晓..."环节
	mid_row.visible = false
	title.text = "揭晓..."
	hint.text = ""

	## 随机决定实际牌：真大鬼/真小鬼
	var actual_big: bool = randf() < 0.5
	var correct: bool = guessed_big == actual_big

	## 600ms 后伪3D 立牌翻面揭晓：立起至侧面 → 切换大鬼/小鬼 → 弹性回正
	await get_tree().create_timer(0.6).timeout
	card_vis.enable_fake3d(true)
	var flip_up: Tween = card_vis.to_rot(0.0, 90.0, 0.14, Tween.TRANS_CUBIC, Tween.EASE_IN_OUT)
	if flip_up:
		await flip_up.finished
	## 在侧面（不可见/最窄）切换为实际大小鬼
	card_vis.show_id(53 if actual_big else 52)   ## 53=大鬼 , 52=小鬼
	card_vis.to_rot(0.0, 0.0, 0.32, Tween.TRANS_BACK, Tween.EASE_OUT)
	title.text = "✓ 猜对了！你先手" if correct else "✗ 猜错了！电脑先手"
	title.add_theme_color_override("font_color", Color(0.24, 0.85, 0.42) if correct else Color(0.95, 0.32, 0.30))

	## 1500ms 后进入发牌
	await get_tree().create_timer(1.5).timeout
	overlay.queue_free()
	return correct


func _teardown_round() -> void:
	if _round != null:
		for c in _round.get_signal_connection_list("round_started"):
			_round.round_started.disconnect(c["callable"])
		for c in _round.get_signal_connection_list("turn_changed"):
			_round.turn_changed.disconnect(c["callable"])
		for c in _round.get_signal_connection_list("round_won"):
			_round.round_won.disconnect(c["callable"])
		for c in _round.get_signal_connection_list("card_played"):
			_round.card_played.disconnect(c["callable"])
		for c in _round.get_signal_connection_list("card_passed"):
			_round.card_passed.disconnect(c["callable"])
		for c in _round.get_signal_connection_list("pressure_bonus"):
			_round.pressure_bonus.disconnect(c["callable"])
		for c in _round.get_signal_connection_list("game_over"):
			_round.game_over.disconnect(c["callable"])
		_round = null


# ═══════════════════════════════════════════════════════════════
# Round 信号处理
# ═══════════════════════════════════════════════════════════════

func _on_card_played(who: RefCounted, cards: Array) -> void:
	_tracker.record_played(cards)
	## 只登记出牌动画来源，不在此立刻 _refresh：
	## process_play 是同步发射，human(_do_play) 与 AI(_ai_think) 在返回后都会紧跟一次 _refresh，
	## 若这里也 _refresh 会与上游重复、二次 _refresh_hand_smooth kill 掉补牌动画（"补牌不飞"）。
	## 由紧跟的那次 _refresh 消费 _table_anim_pending 统一播放。
	if who == _human:
		_play_src_global = _hand_area_center()
	else:
		_play_src_global = _boss_area_center()
	_table_anim_pending = true


func _on_round_won(winner: RefCounted, damage: int) -> void:
	var boss_won := winner == _ai
	_campaign.apply_round_damage(damage, boss_won)
	_human_hp = _campaign.player_hp
	_ai_hp = _campaign.boss_hp

	var msg: String
	if not boss_won:
		msg = "★ 你 赢得小局 (伤害 %d)，对方剩 %d 血" % [damage, _ai_hp]
	else:
		msg = "★ %s 赢得小局 (伤害 %d)，你剩 %d 血" % [winner.name, damage, _human_hp]
	_log(msg)

	## 疯狗：Boss 出牌立刻咬噬玩家（规则体在 LevelRule.bite_damage）
	var lr: Campaign.LevelRule = _round.level_rule
	if boss_won and lr != null and lr.boss_bite_damage and _round.last_play:
		var bite_dmg: int = lr.bite_damage(_round.last_play.cards.size())
		_campaign.apply_round_damage(bite_dmg, true)
		_human_hp = _campaign.player_hp
		msg += " · 疯狗咬噬 %d · 剩 %d HP" % [bite_dmg, _human_hp]
		_log("疯狗咬噬！额外伤害 %d，剩 %d HP" % [bite_dmg, _human_hp])

	_refresh_status(msg)

	if _campaign.player_hp <= 0 or _campaign.boss_hp <= 0:
		_end_level(_campaign.boss_hp <= 0)


func _end_level(player_won: bool) -> void:
	_phase = "level_end"
	_finished = true
	_play_btn.disabled = true
	_pass_btn.disabled = true

	var player_remain := _human.normal_hand.size() + _human_deck.total()
	var boss_remain := _ai.normal_hand.size() + _ai_deck.total()
	var result := _campaign.finish_level(player_won, player_remain, boss_remain)
	_human_hp = _campaign.player_hp
	_ai_hp = _campaign.boss_hp
	var accepted_player_won := result.player_won

	var msg: String
	if accepted_player_won:
		msg = "👑 第 %d 关通过！+%d 筹码，HP %+d" % \
			[_campaign.current_level, result.chips_delta, result.player_hp_delta]
	else:
		msg = "☠ 第 %d 关失败… HP %+d" % [_campaign.current_level, result.player_hp_delta]

	_log(msg)
	_hand_type_lbl.text = msg

	if _campaign.is_game_over():
		_show_game_over("挑战失败", "你被击败了… 筹码 %d" % _campaign.player_chips)
	elif _campaign.is_victory():
		_show_game_over("全部通关！", "恭喜击败所有 BOSS，最终筹码 %d" % _campaign.player_chips)
	elif _campaign.current_level >= _campaign.config.total_levels:
		_show_game_over("最终挑战结束", "赌场老板未被击败，无法通关。最终筹码 %d" % _campaign.player_chips)
	else:
		## 进入关末商店
		_shop.on_level_end()
		_phase = "shop"
		_log("关末商店开启（消费 ≥32 筹码解锁附魔）")
		_refresh("关末商店")


func _show_game_over(title: String, detail: String) -> void:
	_phase = "game_over"
	_game_over_layer.visible = true
	_game_over_msg.text = "%s\n%s" % [title, detail]
	_round_lbl.text = "游戏结束"


# ═══════════════════════════════════════════════════════════════
# AI 逻辑
# ═══════════════════════════════════════════════════════════════

## 出牌按钮：选中非空 → 复用 _do_play；失败保留选中便于调整
func _on_play_pressed() -> void:
	if _finished: return
	if _round == null or not is_instance_valid(_round):
		return
	if _round.whose_turn() != _human:
		_log("还没轮到你")
		return
	# 官方流程：选定非空 → 牌型合法 → 压得过，才真正执行
	if _selected.is_empty():
		_log("请先选择要出的牌")
		return
	## 先把选中快照并清空：_do_play 内部的 process_play → _refresh 渲染用的是当时的 _selected，
	## 若不清空，出牌那几张的选中态会被渲到界面再回落，视觉会"残留/移位到相邻牌"。
	## 清空后 refresh 所见即出牌后的新集合，出牌成功无需二次刷新（不中断补牌动画）。
	var chosen := _selected.duplicate()
	_selected.clear()
	if not _do_play(chosen):
		_selected = chosen  # 出牌失败保留原选中


## 共用出牌流程（按钮与拖拽）：牌型合法 → 压得过 → process_play → 刷新
## 返回是否成功；失败时选中不动，由调用方决定
func _do_play(indices: Array[int]) -> bool:
	if _finished or not is_instance_valid(_round) or _round.whose_turn() != _human:
		return false
	if indices.is_empty():
		return false
	var play: Array[Card] = []
	var n := _human.normal_hand.size()
	for i in indices:
		if i < n:
			play.append(_human.normal_hand[i])
		else:
			var ji := i - n
			if ji < _human.instant_hand.size():
				play.append(_human.instant_hand[ji])
	if play.is_empty():
		return false
	play.sort_custom(func(a: Card, b: Card): return Card.get_rank(a.point) < Card.get_rank(b.point))
	var parsed := CardTypeResult.parse_any_card_type(play)
	if parsed.type == CardTypeResult.CardType.INVALID:
		_log("非法牌型：你选的牌不构成合法牌型")
		_refresh_status("非法牌型")
		return false
	## 赖账鬼：禁止玩家用相同点数压牌（不轮换，重新选）
	if _round.player_blocked_by_ghost(parsed):
		_log("赖账鬼禁止你出相同点数的牌！试试换一种牌型或更大点数")
		_refresh_status("赖账鬼禁同点")
		return false
	var lp := _round.last_play
	if lp != null and not lp.cards.is_empty() and not CardTypeResult.can_beat(parsed, lp):
		_log("无法压过：你选的牌压不过上一手")
		_refresh_status("无法压过")
		return false
	var result: Dictionary = _round.process_play(play)
	_take_round_result(result)
	_refresh("你出 %d 张" % play.size())
	_schedule_ai_if_needed()
	return true


func _on_pass_pressed() -> void:
	if _finished: return
	if _round == null or not is_instance_valid(_round):
		return
	if _round.whose_turn() != _human:
		_log("还没轮到你")
		return
	# 官方流程：无上一手牌（首出）时不能"不要"
	var lp := _round.last_play
	if lp == null or lp.cards.is_empty():
		_log("首出不能不要")
		_refresh_status("首出不能不要")
		return
	_selected.clear()
	var result: Dictionary = _round.process_play([])
	_take_round_result(result)
	_refresh("你选择不要")
	_schedule_ai_if_needed()


func _schedule_ai_if_needed() -> void:
	if _finished:
		return
	if _round == null:
		return
	var cur := _round.whose_turn()
	if not cur.is_human and not _ai_busy:
		_ai_busy = true
		_play_btn.disabled = true
		_pass_btn.disabled = true
		_hand_type_lbl.text = "AI 思考中…"
		var t := get_tree().create_timer(0.7)
		t.timeout.connect(_ai_think)


func _ai_think() -> void:
	_ai_busy = false
	if _finished or _round == null:
		return
	var cur := _round.whose_turn()
	var lp := _round.last_play
	var table_score := _round.table_score
	var play := AICore.choose(_ai, _human, lp, _ai_deck, table_score, _tracker)
	_play_btn.disabled = false
	_pass_btn.disabled = false
	## 赖账鬼强压：AI Pass 前，同花色任意单张强压玩家 J 以下单张
	if play.is_empty() and lp != null and not lp.cards.is_empty() and not _ai.hand.is_empty():
		var forced := _round.ghost_force_play(_ai.hand)
		if not forced.is_empty():
			play = forced
			_log("赖账鬼耍赖！用同花色牌强行压过")
	var result: Dictionary = _round.process_play(play)
	_take_round_result(result)
	_refresh("AI 出 %d 张" % play.size())
	_schedule_ai_if_needed()


## 统一处理 Round 返回字典（alignment：continue 轮换 / round_win / instant_win）
func _take_round_result(result: Dictionary) -> void:
	var state: String = result.get("state", "")
	var reason: String = result.get("reason", state)
	match state:
		"invalid":
			_log("操作被拒绝：%s" % reason)
		"cannot_beat":
			_log("操作被拒绝：%s" % reason)
		"instant_win":
			# 7gui523 或手牌+牌堆双空 → 直接获胜
			var winner: RefCounted = result.get("winner")
			if winner != null:
				var player_won := winner == _human
				## 第 9 关是不可通关的最终挑战；玩家正常出完牌也不算胜利。
				if player_won and _campaign.current_level >= _campaign.config.total_levels:
					_log("★ 最终挑战结束：赌场老板未被击败，无法通关。")
					_human_hp = _campaign.player_hp
					_ai_hp = _campaign.boss_hp
					_refresh("")
					_end_level(false)
					return
				if player_won:
					_campaign.boss_hp = 0
					_log("★ 你打出致命组合，直接获胜！")
				else:
					_campaign.player_hp = 0
					_log("★ %s 直接获胜！" % winner.name)
				_human_hp = _campaign.player_hp
				_ai_hp = _campaign.boss_hp
				_refresh("")
				_end_level(player_won)


# ═══════════════════════════════════════════════════════════════
# 手牌交互
# ═══════════════════════════════════════════════════════════════

func _on_card_clicked(index: int) -> void:
	if _finished: return
	if _round.whose_turn() != _human:
		return
	var total := _human.normal_hand.size() + _human.instant_hand.size()
	if total <= index:
		return
	if _selected.has(index):
		_selected.erase(index)
	else:
		_selected.append(index)
	_refresh_hand()


# ═══════════════════════════════════════════════════════════════
# 按钮：商店 / 返回 / HP 互换
# ═══════════════════════════════════════════════════════════════

func _on_shop_pressed() -> void:
	if _phase == "shop":
		_show_shop_panel()
	elif _phase == "battle":
		_log("战斗中无法打开商店，请在关末进入")
	else:
		_show_shop_panel()


func _on_return_pressed() -> void:
	## 返回 / 重开：重置战役
	_on_reset_pressed()


func _on_hp_swap_pressed() -> void:
	## 调试用：HP 互换（作弊按钮）
	var t := _human_hp
	_human_hp = _ai_hp
	_ai_hp = t
	_campaign.player_hp = _human_hp
	_campaign.boss_hp = _ai_hp
	_refresh("HP 已互换（调试）")


func _on_next_level() -> void:
	_hide_shop_panel()
	if _campaign.current_level >= _campaign.config.total_levels:
		_show_game_over("最终挑战结束", "赌场老板未被击败，无法通关。")
		return
	_start_level(_campaign.current_level + 1)


## 拖拽输入路由：点击 vs 拖拽判定、松手出牌、右键/Esc 取消
func _input(event: InputEvent) -> void:
	if not _drag_pending and not _dragging:
		return
	if event is InputEventMouseMotion:
		if _drag_pending and not _dragging:
			if get_global_mouse_position().distance_to(_drag_press_pos) > DRAG_THRESHOLD:
				_drag_start(_drag_first_index)
	elif event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_LEFT \
			and not event.pressed:
		if _dragging:
			get_viewport().set_input_as_handled()
			_drag_drop()
		elif _drag_pending:
			_drag_pending = false
			_on_card_clicked(_drag_first_index)
	elif event is InputEventMouseButton and event.pressed \
			and event.button_index == MOUSE_BUTTON_RIGHT and _dragging:
		get_viewport().set_input_as_handled()
		_drag_cancel()
	elif event.is_action_pressed("ui_cancel") and _dragging:
		_drag_cancel()


func _process(_delta: float) -> void:
	if not _dragging or _drag_visuals.is_empty():
		return
	## 回合已变（兜底）：取消拖拽
	if _finished or not is_instance_valid(_round) or _round.whose_turn() != _human:
		_drag_cancel()
		return
	var mouse := get_global_mouse_position()
	## 落点高亮：鼠标悬在桌面区时显示
	var over := _table_cards.get_global_rect().has_point(mouse)
	if _drop_hint.visible != over:
		_drop_hint.visible = over
	if over:
		_drop_hint.global_position = _table_cards.global_position - Vector2(10, 10)
		_drop_hint.size = _table_cards.size + Vector2(20, 20)
	## 被拖卡跟随鼠标：单张居中；多张水平展开 + 弧形上拱 + 微旋转
	var n := _drag_visuals.size()
	for i in n:
		var v := _drag_visuals[i]
		if not is_instance_valid(v):
			continue
		var off := Vector2.ZERO
		var rot := 0.0
		if n > 1:
			var t := float(i) / float(n - 1)
			off.x = (t - 0.5) * DRAG_SPREAD
			off.y = -absf(sin(t * PI)) * DRAG_ARC_Y
			rot = lerpf(-DRAG_ROT, DRAG_ROT, t)
		v.global_position = mouse + off - Vector2(HAND_W, HAND_H) * 0.5
		v.rotation = deg_to_rad(rot)


## 开始拖拽：抓的卡在选中集合且选中数>1 → 整组拖；否则单张
func _drag_start(index: int) -> void:
	_drag_pending = false
	if _finished or not is_instance_valid(_round) or _round.whose_turn() != _human:
		return
	## 全局索引覆盖普通区 + 立刻补区（双方都可拖出牌）
	var total := _human.normal_hand.size() + _human.instant_hand.size()
	if index < 0 or total <= index:
		return
	_dragging = true
	_drag_indices.clear()
	_drag_visuals.clear()
	_drag_origin.clear()
	if _selected.has(index) and _selected.size() > 1:
		var sorted := _selected.duplicate()
		sorted.sort()
		_drag_indices = sorted
	else:
		_drag_indices = [index]
	## 记录原始 transform，reparent 到 overlay（保留全局位置）
	## 节点按展示序(排序)添加，不能用 children 顺序取；按 hand_index meta 精确命中对应卡
	var hand_cards := _player_hand.get_children()
	for idx in _drag_indices:
		var v: CardVisual = null
		for ch in hand_cards:
			if ch is CardVisual and not ch.is_queued_for_deletion() \
					and ch.get_meta("hand_index", -1) == idx:
				v = ch
				break
		if v == null:
			continue
		_drag_origin.append({
			"parent": v.get_parent(),
			"pos": v.position,
			"rot": v.rotation,
			"scale": v.scale,
			"z": v.z_index,
		})
		v.reparent(_anim_overlay)
		v.z_index = 60
		v.set_fake3d_rot(0.0, 0.0)
		_drag_visuals.append(v)


## 松手：在桌面区 → 出牌；否则弹回
func _drag_drop() -> void:
	var mouse := get_global_mouse_position()
	if not _table_cards.get_global_rect().has_point(mouse):
		_drag_cancel()
		return
	var indices := _drag_indices.duplicate()
	if not _do_play(indices):
		_drag_cancel()
		return
	## 出牌起点：用被拖卡在手牌里的原始全局位置覆盖默认中心。
	## 立刻补牌区(右侧)与普通区由此都能从卡片原位飞出，不再偏移到整区中心。
	if not _drag_origin.is_empty():
		var o: Dictionary = _drag_origin[0]
		var p: Control = o["parent"]
		if is_instance_valid(p):
			_play_src_global = p.get_global_transform_with_canvas() * o["pos"]
	## 出牌成功：拖拽视觉卡丢弃（数据刷新已重建手牌/桌面）
	for v in _drag_visuals:
		if is_instance_valid(v):
			v.queue_free()
	_drag_visuals.clear()
	_drag_indices.clear()
	_drag_origin.clear()
	_drop_hint.visible = false


## 取消拖拽：逐张弹回原位（tween 并行；期间手牌若被重建则随节点释放，无残留）
func _drag_cancel() -> void:
	_dragging = false
	_drop_hint.visible = false
	for i in _drag_visuals.size():
		var v := _drag_visuals[i]
		if not is_instance_valid(v):
			continue
		var o: Dictionary = _drag_origin[i]
		if is_instance_valid(o["parent"]):
			v.reparent(o["parent"])
		v.z_index = o["z"]
		var tw := v.create_tween()
		tw.set_ease(Tween.EASE_OUT).set_trans(Tween.TRANS_CUBIC)
		tw.set_parallel(true)
		tw.tween_property(v, "position", o["pos"], DRAG_RETURN_DUR)
		tw.tween_property(v, "rotation", o["rot"], DRAG_RETURN_DUR)
		tw.tween_property(v, "scale", o["scale"], DRAG_RETURN_DUR)
	_drag_visuals.clear()
	_drag_indices.clear()
	_drag_origin.clear()