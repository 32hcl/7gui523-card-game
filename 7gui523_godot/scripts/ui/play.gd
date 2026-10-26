extends "res://scripts/ui/play_input.gd"

const BOSS_AI := preload("res://scripts/ai/boss_ai.gd")

## — 第 4 层 play：入口 · 关卡流程 · Round 信号 · AI · 交互调度
##
## 继承链顶层的正式对战主场景。
## 双牌堆 + Round 引擎 + AI，玩家选牌出牌，桌面伤害结算。

func _ready() -> void:
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

	## 拖拽落点提示
	_drop_hint = Panel.new()
	_drop_hint.name = "DropHint"
	_drop_hint.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_drop_hint.z_index = 40
	_drop_hint.visible = false
	var hint_sb := StyleBoxFlat.new()
	hint_sb.bg_color = Color(COL_GOLD, 0.10)
	hint_sb.border_color = Color(COL_GOLD, 0.65)
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
	discard_sb.bg_color = Color(COL_CRIMSON, 0.10)
	discard_sb.border_color = Color(COL_CRIMSON, 0.75)
	discard_sb.set_border_width_all(2)
	discard_sb.set_corner_radius_all(8)
	_discard_hint.add_theme_stylebox_override("panel", discard_sb)
	add_child(_discard_hint)

	## 连接额外按钮信号（场景中未预置 connection）
	_shop_btn.pressed.connect(_on_shop_pressed)

	_state = GameState.new()
	_campaign = Campaign.new()
	_campaign.player_chips = 50   ## 起始筹码

	_campaign.cleared_level = 0
	_shop = Shop.new()
	_tracker = AICore.CardTracker.new()
	_show_main_menu()


# ═══════════════════════════════════════════════════════════════
# 主界面（play 内覆盖层）：开始游戏 / 阅读规则
# ═══════════════════════════════════════════════════════════════

const RULES_TEXT := """【目标】逐关击败 BOSS：把对方 HP 扣到 0 即过关；你的 HP 归零则挑战失败。

【出牌】拖拽手牌到桌面区域出牌，牌型必须合法且压过上家。双击桌面 = 不要（首出不能不要）。

【战力】7 > 大鬼 > 小鬼 > 5 > 2 > 3 > A > K > Q > J > 10 > 9 > 8 > 6 > 4。

【7鬼523】凑齐 7 + 任意鬼 + 5 + 2 + 3 共 5 张一次打出，直接赢得本关（第 9 关禁用）。

【得分与伤害】桌面牌面总分为本局伤害；以相同牌型相同点数压制全伤害牌时触发压迫加分。

【补牌】每小局结束后由赢家从牌堆补牌。

【筹码与门票】首次进入赌场需购买门票（金币）；每关入场消耗筹码，过关赢得筹码。

【关末商店】每关胜利后自动开启：可购买附魔强化小丑牌；一级店消费满 32 筹码解锁二级店（随机 2 件附魔商品）。

【关卡】共 9 关。第 7 关「黑老大」：BOSS 出完牌强制弃掉你最大的牌；第 9 关「赌场老板」是不可战胜的最终挑战。"""


func _show_main_menu() -> void:
	var overlay := Control.new()
	overlay.name = "MainMenuOverlay"
	overlay.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	overlay.z_index = 80
	overlay.mouse_filter = Control.MOUSE_FILTER_STOP
	add_child(overlay)

	## 底色压暗：与全局背景同色系，主界面独立成屏
	var dim := ColorRect.new()
	dim.color = Color(COL_BG, 0.97)
	dim.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	dim.mouse_filter = Control.MOUSE_FILTER_STOP
	overlay.add_child(dim)

	var box := VBoxContainer.new()
	box.set_anchors_and_offsets_preset(Control.PRESET_CENTER)
	box.offset_left = -160
	box.offset_top = -160
	box.offset_right = 160
	box.offset_bottom = 160
	box.alignment = BoxContainer.ALIGNMENT_CENTER
	box.add_theme_constant_override("separation", 16)
	overlay.add_child(box)

	var title := Label.new()
	title.text = "七鬼 523"
	title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	title.add_theme_font_size_override("font_size", 44)
	title.add_theme_color_override("font_color", COL_GOLD)
	title.add_theme_color_override("font_outline_color", Color(0.3, 0.2, 0.08, 1.0))
	title.add_theme_constant_override("outline_size", 8)
	box.add_child(title)

	var sub := Label.new()
	sub.text = "鸿运赌场 · 逐关挑战"
	sub.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	sub.add_theme_font_size_override("font_size", 14)
	sub.add_theme_color_override("font_color", COL_TEXT_DIM)
	box.add_child(sub)

	var spacer := Control.new()
	spacer.custom_minimum_size = Vector2(0, 18)
	box.add_child(spacer)

	var start_btn := Button.new()
	start_btn.text = "开始游戏"
	start_btn.custom_minimum_size = Vector2(220, 52)
	style_button_balatro(start_btn, COL_GREEN, Color(0.95, 0.95, 0.93))
	box.add_child(start_btn)

	var settings_btn := Button.new()
	settings_btn.text = "⚙ 设置"
	settings_btn.custom_minimum_size = Vector2(220, 52)
	style_button_balatro(settings_btn, Color(0.18, 0.17, 0.19), Color(0.88, 0.86, 0.83))
	box.add_child(settings_btn)

	var rules_btn := Button.new()
	rules_btn.text = "阅读规则"
	rules_btn.custom_minimum_size = Vector2(220, 52)
	style_button_balatro(rules_btn, COL_ACCENT, Color(0.95, 0.95, 0.93))
	box.add_child(rules_btn)

	start_btn.pressed.connect(func():
		overlay.queue_free()
		_enter_casino_then_start(_campaign.current_level))
	settings_btn.pressed.connect(func(): SettingsDialog.show(overlay, self))
	rules_btn.pressed.connect(func(): _show_rules_dialog(overlay))


## 规则弹窗：挂在主界面 overlay 内，金边面板 + 滚动文本
func _show_rules_dialog(menu: Control) -> void:
	var panel := Panel.new()
	panel.add_theme_stylebox_override("panel", flat_border(
		COL_PANEL, COL_GOLD.darkened(0.3), 2, 10, Color(0, 0, 0, 0.5), 10, 4))
	panel.set_anchors_and_offsets_preset(Control.PRESET_CENTER)
	panel.offset_left = -260
	panel.offset_top = -250
	panel.offset_right = 260
	panel.offset_bottom = 250
	panel.z_index = 5
	menu.add_child(panel)

	var vb := VBoxContainer.new()
	vb.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	vb.offset_left = 16
	vb.offset_top = 14
	vb.offset_right = -16
	vb.offset_bottom = -14
	vb.add_theme_constant_override("separation", 10)
	panel.add_child(vb)

	var title := Label.new()
	title.text = "游戏规则"
	title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	title.add_theme_font_size_override("font_size", 20)
	title.add_theme_color_override("font_color", COL_GOLD)
	vb.add_child(title)

	var scroll := ScrollContainer.new()
	scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
	vb.add_child(scroll)
	var body := Label.new()
	body.text = RULES_TEXT
	body.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	body.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	body.add_theme_font_size_override("font_size", 13)
	body.add_theme_color_override("font_color", COL_TEXT)
	scroll.add_child(body)

	var close_btn := Button.new()
	close_btn.text = "关闭"
	close_btn.custom_minimum_size = Vector2(140, 40)
	close_btn.size_flags_horizontal = Control.SIZE_SHRINK_CENTER
	style_button_balatro(close_btn, Color(0.18, 0.17, 0.19), Color(0.88, 0.86, 0.83))
	vb.add_child(close_btn)
	close_btn.pressed.connect(func(): panel.queue_free())


func _on_reset_pressed() -> void:

	_phase = "battle"
	_finished = true
	_ai_busy = false
	_last_boss_play_count = 0
	if _shop_panel:
		_shop_panel.visible = false
	var old_guess := get_node_or_null("MainVBox/TableArea/GuessOverlay")
	if old_guess:
		old_guess.queue_free()
	_teardown_round()
	_table_anim_running = false
	_table_anim_pending = false
	_table_cards_anim.clear()
	_table_render_sig = "@init"
	_play_src_global = Vector2.ZERO
	_selected.clear()
	_player_discard_count = 0
	_player_discarded_this_round = false
	_player_can_discard = false
	for row: HBoxContainer in [_top_hand, _bottom_hand, _boss_hand, _boss_hand_instant]:
		for slot in row.get_children():
			row.remove_child(slot)
			slot.queue_free()
	for child in _table_cards.get_children():
		_table_cards.remove_child(child)
		child.queue_free()
	for child in _anim_overlay.get_children():
		child.queue_free()
	_campaign.start_new()
	_campaign.player_chips = 50
	_shop.reset_stock()
	_enter_casino_then_start(_campaign.current_level)


func _on_back_to_menu() -> void:
	_phase = "battle"
	_finished = true
	_ai_busy = false
	if _shop_panel:
		_shop_panel.visible = false
	var old_guess := get_node_or_null("MainVBox/TableArea/GuessOverlay")
	if old_guess:
		old_guess.queue_free()
	_teardown_round()
	_table_anim_running = false
	_table_anim_pending = false
	_table_cards_anim.clear()
	_table_render_sig = "@init"
	_play_src_global = Vector2.ZERO
	_selected.clear()
	_player_discard_count = 0
	_player_discarded_this_round = false
	_player_can_discard = false
	for row: HBoxContainer in [_top_hand, _bottom_hand, _boss_hand, _boss_hand_instant]:
		for slot in row.get_children():
			row.remove_child(slot)
			slot.queue_free()
	for child in _table_cards.get_children():
		_table_cards.remove_child(child)
		child.queue_free()
	for child in _anim_overlay.get_children():
		child.queue_free()
	_campaign.start_new()
	_campaign.player_chips = 50
	_shop.reset_stock()
	# 清除旧主菜单覆盖层，重新显示
	var old_menu := get_node_or_null("MainMenuOverlay")
	if old_menu:
		old_menu.queue_free()
	_show_main_menu()


## 进入赌场前购买一次金币门票；门票不替代每关的筹码消耗。
func _enter_casino_then_start(level: int) -> void:
	if _campaign.has_purchased_entry:
		_start_level(level)
		return

	if _campaign.dollar < GameConfig.ENTRY_TICKET_DOLLARS:
		_show_game_over(
			"金币不足",
			"进入赌场需要 %d 金币\n当前金币：%d" % [GameConfig.ENTRY_TICKET_DOLLARS, _campaign.dollar])
		return

	var panel := Panel.new()
	panel.add_theme_stylebox_override("panel", flat_border(
		COL_PANEL, COL_GOLD.darkened(0.3), 2, 10, Color(0, 0, 0, 0.55), 12, 4))
	panel.set_anchors_and_offsets_preset(Control.PRESET_CENTER)
	panel.offset_left = -230
	panel.offset_top = -150
	panel.offset_right = 230
	panel.offset_bottom = 150
	panel.z_index = 5
	add_child(panel)

	var vb := VBoxContainer.new()
	vb.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	vb.offset_left = 22; vb.offset_top = 18; vb.offset_right = -22; vb.offset_bottom = -18
	vb.add_theme_constant_override("separation", 12)
	panel.add_child(vb)

	var title := Label.new()
	title.text = "鸿运赌场 · 入场"
	title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	title.add_theme_font_size_override("font_size", 18)
	title.add_theme_color_override("font_color", COL_GOLD)
	vb.add_child(title)

	var sep := HSeparator.new()
	sep.add_theme_constant_override("separation", 8)
	vb.add_child(sep)

	var body := Label.new()
	body.text = "入场需要 %d 金币。购买门票后，另需每关消耗筹码。\n\n当前金币：%d\n\n是否购买赌场门票？" % [GameConfig.ENTRY_TICKET_DOLLARS, _campaign.dollar]
	body.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	body.add_theme_font_size_override("font_size", 13)
	body.add_theme_color_override("font_color", Color(0.88, 0.86, 0.83))
	body.autowrap_mode = TextServer.AUTOWRAP_WORD
	vb.add_child(body)

	var sep2 := HSeparator.new()
	vb.add_child(sep2)

	var btns := HBoxContainer.new()
	btns.alignment = BoxContainer.ALIGNMENT_CENTER
	btns.add_theme_constant_override("separation", 16)
	vb.add_child(btns)

	var buy_btn := Button.new()
	buy_btn.text = "购买门票"
	buy_btn.custom_minimum_size = Vector2(130, 40)
	style_button_balatro(buy_btn, COL_GOLD, Color(0.10, 0.08, 0.06))
	btns.add_child(buy_btn)

	var cancel_btn := Button.new()
	cancel_btn.text = "暂不进入"
	cancel_btn.custom_minimum_size = Vector2(110, 40)
	style_button_balatro(cancel_btn, Color(0.18, 0.17, 0.19), Color(0.88, 0.86, 0.83))
	btns.add_child(cancel_btn)

	var back_btn := Button.new()
	back_btn.text = "回到主界面"
	back_btn.custom_minimum_size = Vector2(110, 40)
	style_button_balatro(back_btn, Color(0.18, 0.17, 0.19), Color(0.88, 0.86, 0.83))
	btns.add_child(back_btn)

	buy_btn.pressed.connect(func():
		if _campaign.purchase_entry_ticket():
			_start_level(level)
		panel.queue_free()
	)
	cancel_btn.pressed.connect(func(): panel.queue_free())
	back_btn.pressed.connect(func():
		_on_back_to_menu()
		panel.queue_free()
	)


# ═══════════════════════════════════════════════════════════════
# 关卡生命周期
# ═══════════════════════════════════════════════════════════════

func _start_level(level: int) -> void:
	var generation := _flow_generation
	_phase = "battle"
	_game_over_layer.visible = false

	var entry_chips := Campaign.get_level_entry_chips(level)
	if _campaign.player_chips < entry_chips:
		_show_game_over("筹码不足", "第 %d 关需要 %d 筹码\n当前筹码：%d" % [level, entry_chips, _campaign.player_chips])
		return
	_campaign.player_chips -= entry_chips

	_campaign.start_level(level)
	_shop.current_level = level
	_tracker.reset()

	_human_deck = Deck.DeckManager.new()
	_ai_deck = Deck.DeckManager.new()
	_human_deck.init_standard()
	_ai_deck.init_standard()

	_human = Player.PlayerData.new()
	_human.name = "32%Hcl"
	_human.is_human = true
	_human.ai_level = AICore.Level.AI1_RULE
	_human.card_sort_mode = _card_sort_mode
	_human_deck.fill_player_normal(_human)
	_human_deck.fill_player_instant(_human)

	_ai = Player.PlayerData.new()
	_ai.name = Campaign.get_boss_name(level) + "·BOSS"
	_ai.is_human = false
	_ai.ai_level = AICore.Level.AI1_RULE
	_ai.card_sort_mode = _card_sort_mode
	_ai_deck.fill_player_normal(_ai)
	_ai_deck.fill_player_instant(_ai)

	_apply_pixelation()

	_human_hp = _campaign.player_hp
	_ai_hp = _campaign.boss_hp
	_last_player_hp = -1
	_last_boss_hp = -1
	_finished = false
	_ai_busy = false
	## 重置桌面/手牌动画状态：若上一关动画协程因关卡切换中断，会残留 running=true，
	## 导致新关卡 _refresh_table 开头直接 return、所有出牌/补牌/发牌动画不播放
	## （"第一回合好，后面就有问题"的根因）。新关必须清零以便开启动画。
	_table_anim_running = false
	_table_anim_pending = false
	_table_cards_anim.clear()
	_table_render_sig = "@init"   ## 强制新关卡首次 _refresh_table 全量重建（含旧桌面溶解）
	_play_src_global = Vector2.ZERO
	_selected.clear()
	_player_can_discard = false
	## 新关卡必须清空上一关桌面牌；Round 尚未创建时不会触发信号刷新旧节点。
	for child in _table_cards.get_children():
		_table_cards.remove_child(child)
		child.queue_free()
	for child in _anim_overlay.get_children():
		child.queue_free()

	var rule = Campaign.level_rule(level)
	_mask_player_draws = rule.mask_player_draws
	_initial_deal_finished = false
	_set_player_hp_visible(false)
	_masked_player_cards.clear()
	var ai_hand_buf: Array[Card] = []
	_ai.collect_hand_into(ai_hand_buf)
	rule.mutate_boss_hand(ai_hand_buf)
	## 猜牌决定先手；猜先期间隔离操作按钮
	_set_op_buttons_enabled(false)
	var player_first := await _run_guess_phase()
	if generation != _flow_generation:
		return
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
	_round.pressure_bonus.connect(_on_pressure_bonus)
	_round.game_over.connect(func(): _finished = true)

	_title_lbl.text = _ai.name

	## 开局发牌动画：先清空，再逐张飞入
	for row: HBoxContainer in [_top_hand, _bottom_hand, _boss_hand, _boss_hand_instant]:
		for slot in row.get_children():
			row.remove_child(slot)
			slot.queue_free()
	_boss_hand.set_meta("_boss_known_normal", 0)
	_boss_hand_instant.set_meta("_boss_known_instant", 0)
	_refresh_status("新关卡开始")
	_refresh_turn()
	_refresh_top()

	## 延迟一帧，让容器布局稳定后再开启动画
	await get_tree().process_frame
	_setup_deck_display()
	_play_deck_shuffle()   ## 新关洗牌：牌背抬起再落回
	_refresh_top()
	_refresh_hand_with_animation(true)
	_refresh_boss_hand_with_animation(true)
	await get_tree().create_timer(INITIAL_DEAL_DURATION + INITIAL_DEAL_DELAY * 6.0).timeout
	_initial_deal_finished = true
	_set_player_hp_visible(true)
	_play_initial_hp_reveal()
	_schedule_ai_if_needed()


## 统一开关操作按钮（猜先等流程隔离用）
func _set_op_buttons_enabled(on: bool) -> void:
	_shop_btn.disabled = not on
	_new_game_btn.disabled = not on


## 猜先阶段 — 猜对玩家先手，猜错电脑先手
func _run_guess_phase() -> bool:
	var zone: Control = $MainVBox/TableArea
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
	title.add_theme_color_override("font_color", COL_GOLD)
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
	style_button_balatro(big_btn, COL_CRIMSON, COL_GOLD)
	style_button_balatro(small_btn, COL_ACCENT, COL_GOLD)
	mid_row.add_child(big_btn)
	mid_row.add_child(small_btn)
	outer.add_child(mid_row)

	## 提示小字
	var hint := Label.new()
	hint.text = "大鬼（上）与小鬼（下）其中一张，猜对先手"
	hint.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	hint.add_theme_font_size_override("font_size", 12)
	hint.add_theme_color_override("font_color", COL_TEXT_DIM)
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
	title.add_theme_color_override("font_color", COL_GREEN if correct else COL_CRIMSON)

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
	if who == _ai:
		_last_boss_play_count = cards.size()
	## 只登记出牌动画来源，不在此立刻 _refresh：
	## process_play 是同步发射，human(_do_play) 与 AI(_ai_think) 在返回后都会紧跟一次 _refresh，
	## 若这里也 _refresh 会与上游重复、二次 _refresh_hand_smooth kill 掉补牌动画（"补牌不飞"）。
	## 由紧跟的那次 _refresh 消费 _table_anim_pending 统一播放。
	## 严格使用容器中心（不查 CardVisual、不存临时投递）
	if who == _human:
		_play_src_global = _hand_area_center()
	else:
		_play_src_global = _boss_src_center(cards)
	_table_anim_pending = true


func _on_pressure_bonus(who: RefCounted, bonus: int) -> void:
	if bonus > 0:
		_spawn_floating_damage(bonus, "压迫")
	_refresh_status("%s 压迫加分 %d" % [who.name, bonus])


func _on_round_won(winner: RefCounted, damage: int) -> void:
	## 先锁定规则状态，再播放结算动画；动画不能阻塞下一回合的输入资格。
	_player_discarded_this_round = false
	_player_can_discard = (winner == _ai)
	await _play_damage_delay()
	## 弃牌窗口：若本回合是玩家"不要"结束的（赢家=BOSS），下回合玩家可弃牌
	var boss_won := winner == _ai
	_campaign.apply_round_damage(damage, boss_won)
	_human_hp = _campaign.player_hp
	_ai_hp = _campaign.boss_hp

	var msg: String
	if not boss_won:
		msg = "★ 你 赢得小局 (伤害 %d)，对方剩 %d 血" % [damage, _ai_hp]
	else:
		msg = "★ %s 赢得小局 (伤害 %d)，你剩 %d 血" % [winner.name, damage, _human_hp]

	## 疯狗：Boss 出牌立刻咬噬玩家（规则体在 LevelRule.bite_damage）
	var lr: Campaign.LevelRule = _round.level_rule
	if boss_won and lr != null and lr.boss_bite_damage and _last_boss_play_count > 0:
		var bite_dmg: int = lr.bite_damage(_last_boss_play_count)
		_campaign.apply_round_damage(bite_dmg, true)
		_human_hp = _campaign.player_hp
		msg += " · 疯狗咬噬 %d · 剩 %d HP" % [bite_dmg, _human_hp]

	_refresh_status(msg)

	if _campaign.player_hp <= 0 or _campaign.boss_hp <= 0:
		_end_level(_campaign.boss_hp <= 0)


func _end_level(player_won: bool) -> void:
	_phase = "level_end"
	_finished = true
	## 关末清空双方不可见回收缓冲区，残余批次打乱后进入各自隐藏牌堆。
	_human_deck.flush_recycle_buffer()
	_ai_deck.flush_recycle_buffer()

	var player_remain := _human.normal_hand.size() + _human_deck.total()
	var boss_remain := _ai.normal_hand.size() + _ai_deck.total()
	var result := _campaign.finish_level(player_won, player_remain, boss_remain)
	_human_hp = _campaign.player_hp
	_ai_hp = _campaign.boss_hp
	var accepted_player_won := result.player_won
	## 本关已分出胜负（一方先出完）：输家的整套牌背燃烧溶解
	_update_deck_empty(_player_deck if not accepted_player_won else _boss_deck, true)

	var msg: String
	if accepted_player_won:
		msg = "👑 第 %d 关通过！+%d 筹码，HP %+d" % \
			[_campaign.current_level, result.chips_delta, result.player_hp_delta]
	else:
		msg = "☠ 第 %d 关失败… HP %+d" % [_campaign.current_level, result.player_hp_delta]

	_round_lbl.text = msg
	_round_lbl.add_theme_color_override("font_color", COL_GOLD)

	if _campaign.is_game_over():
		_show_game_over("挑战失败", "你被击败了… 筹码 %d" % _campaign.player_chips)
	elif _campaign.current_level >= _campaign.config.total_levels:
		_show_game_over("最终挑战结束", "赌场老板未被击败，无法通关。最终筹码 %d" % _campaign.player_chips)
	else:
		## 进入关末商店：自动弹出面板（此前只设 phase 不弹窗，表现为"胜利后不进商店"）
		_shop.on_level_end()
		_phase = "shop"
		_refresh("关末商店")
		_show_shop_panel()


func _show_game_over(title: String, detail: String) -> void:
	_phase = "game_over"
	_game_over_layer.visible = true
	_game_over_msg.text = "%s\n%s" % [title, detail]
	_round_lbl.text = "游戏结束"


# ═══════════════════════════════════════════════════════════════
# AI 逻辑
# ═══════════════════════════════════════════════════════════════

## 共用出牌流程（拖拽松手调用）：牌型合法 → 压得过 → process_play → 刷新
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
		_refresh_status("非法牌型")
		return false
	## 赌场老板（第 9 关）禁止 7gui523。这里先拦一手，避免提交后才被 Round 判非法并白丢回合
	var lr: Campaign.LevelRule = _round.level_rule
	if lr != null and lr.ban_gui523 and parsed.type == CardTypeResult.CardType.GUI523:
		_refresh_status("本关禁止打出 7gui523")
		return false
	## 赖账鬼：禁止玩家用相同点数压牌（不轮换，重新选）
	if _round.player_blocked_by_ghost(parsed):
		_refresh_status("赖账鬼禁同点")
		return false
	var lp := _round.last_play
	if lp != null and not lp.cards.is_empty() and not CardTypeResult.can_beat(parsed, lp):
		_refresh_status("无法压过")
		return false
	var result: Dictionary = _round.process_play(play)
	_take_round_result(result)
	_refresh("你出 %d 张" % play.size())
	if result.get("state", "") == "boss_finish":
		_resolve_boss_finish()
		return true
	_schedule_ai_if_needed()
	return true


## 弃牌（两种代价方案）
## immediate_draw=true  → 方式一：拖到牌堆，每张扣 1 筹码，立即从牌堆补等量
## immediate_draw=false → 方式二：选中+双击桌面，免费，下回合正常补牌
func _discard_cards(indices: Array[int], immediate_draw: bool) -> bool:
	if _finished or not is_instance_valid(_round) or _round.whose_turn() != _human:
		return false
	if not _player_can_discard:
		_refresh_status("当前不可弃牌")
		return false
	if _player_discarded_this_round:
		_refresh_status("本小回合已经弃过牌")
		return false
	if _player_discard_count >= PLAYER_DISCARD_LIMIT:
		_refresh_status("本关弃牌额度已用完（%d 张）" % PLAYER_DISCARD_LIMIT)
		return false
	if indices.is_empty() or indices.size() > PLAYER_DISCARD_LIMIT - _player_discard_count:
		_refresh_status("弃牌数量超出本关剩余额度")
		return false
	var discard_cards: Array[Card] = []
	for index in indices:
		if index < 0 or index >= _human.normal_hand.size():
			_refresh_status("只能弃普通手牌")
			return false
		discard_cards.append(_human.normal_hand[index])
	if discard_cards.is_empty():
		return false

	## 方式一：先校验筹码够不够，再执行
	if immediate_draw:
		var cost: int = discard_cards.size()
		if _campaign.player_chips < cost:
			_refresh_status("筹码不足（弃 %d 张需要 %d 筹码）" % [discard_cards.size(), cost])
			return false
		_campaign.player_chips -= cost

	_human.remove_cards_from_hand(discard_cards)
	_human_deck.recycle_played(discard_cards)
	_player_discard_count += discard_cards.size()
	_player_discarded_this_round = true

	if immediate_draw:
		_human_deck.fill_player_normal(_human)
		_refresh("你弃掉 %d 张，花费 %d 筹码，已补牌" % [discard_cards.size(), discard_cards.size()])
	else:
		_refresh("你弃掉 %d 张（免费，下回合补牌）" % discard_cards.size())
	return true


func _has_legal_human_play() -> bool:
	var previous := _round.last_play
	for candidate in AICore.enumerate_legal_plays(_human):
		var parsed := CardTypeResult.parse_any_card_type(candidate)
		if parsed.type == CardTypeResult.CardType.INVALID:
			continue
		if _round.player_blocked_by_ghost(parsed):
			continue
		if previous == null or previous.cards.is_empty() or CardTypeResult.can_beat(parsed, previous):
			return true
	return false


func _on_pass_pressed() -> void:
	if _finished: return
	if _round == null or not is_instance_valid(_round):
		return
	if _round.whose_turn() != _human:
		return
	## 方式二：弃牌窗口 + 有选中牌 → 免费弃牌，不立即补牌
	if _player_can_discard and not _selected.is_empty():
		var indices := _selected.duplicate()
		if _discard_cards(indices, false):
			_selected.clear()
		return
	# 官方流程：无上一手牌（首出）时不能"不要"
	var lp := _round.last_play
	if lp == null or lp.cards.is_empty():
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
		_round_lbl.text = "AI 思考中…"
		_round_lbl.add_theme_color_override("font_color", COL_TEXT)
		var t := get_tree().create_timer(0.7)
		t.timeout.connect(_ai_think)


func _ai_think() -> void:
	_ai_busy = false
	if _finished or _round == null:
		return
	var cur := _round.whose_turn()
	if cur == _ai:
		var buf: Array[Card] = []
		_ai.collect_hand_into(buf)
		_round.level_rule.mutate_boss_hand(buf)
	var opp := _human if cur == _ai else _ai
	var deck := _human_deck if cur == _human else _ai_deck
	var lp := _round.last_play
	var table_score := _round.table_score
	var play: Array[Card] = BOSS_AI.choose(cur, lp, table_score)
	## 赖账鬼强压：AI Pass 前，同花色任意单张强压玩家 J 以下单张
	if play.is_empty() and lp != null and not lp.cards.is_empty() and not cur.is_hand_empty():
		var cur_hand_buf: Array[Card] = []
		cur.collect_hand_into(cur_hand_buf)
		var forced := _round.ghost_force_play(cur_hand_buf)
		if not forced.is_empty():
			play = forced
	var result: Dictionary = _round.process_play(play)
	_take_round_result(result)
	_refresh("AI 出 %d 张" % play.size())
	if result.get("state", "") == "boss_finish":
		await _resolve_boss_finish()
		return
	_schedule_ai_if_needed()


func _resolve_boss_finish() -> void:
	if _finished:
		return
	var remaining_cards := _human.normal_hand.size() + _human.instant_hand.size() + _human_deck.total()
	var damage := remaining_cards * 3
	await _play_damage_delay()
	_campaign.apply_round_damage(damage, true)
	_human_hp = _campaign.player_hp
	_ai_hp = _campaign.boss_hp
	_refresh("BOSS 出完牌，造成 %d 伤害" % damage)
	_end_level(_human_hp > 0)


## 统一处理 Round 返回字典（continue / round_win / instant_win）；
## boss_finish 由 _resolve_boss_finish 等待动画后单独结算。
func _take_round_result(result: Dictionary) -> void:
	var state: String = result.get("state", "")
	var reason: String = result.get("reason", state)
	match state:
		"invalid":
			_refresh_status("操作被拒绝：%s" % reason)
		"cannot_beat":
			_refresh_status("操作被拒绝：%s" % reason)
		"instant_win":
			# 7gui523 或玩家手牌+牌堆双空 → 直接获胜；BOSS 双空走 boss_finish。
			var winner: RefCounted = result.get("winner")
			if winner != null:
				var player_won := winner == _human
				## 第 9 关是不可通关的最终挑战；玩家正常出完牌也不算胜利。
				if player_won and _campaign.current_level >= _campaign.config.total_levels:
					_human_hp = _campaign.player_hp
					_ai_hp = _campaign.boss_hp
					_refresh("")
					_end_level(false)
					return
				if player_won:
					_campaign.boss_hp = 0
				else:
					_campaign.player_hp = 0
				_human_hp = _campaign.player_hp
				_ai_hp = _campaign.boss_hp
				_refresh("")
				_end_level(player_won)


# ═══════════════════════════════════════════════════════════════
# 手牌交互
# ═══════════════════════════════════════════════════════════════

# ═══════════════════════════════════════════════════════════════
# 按钮：商店 / 返回 / HP 互换
# ═══════════════════════════════════════════════════════════════

func _on_shop_pressed() -> void:
	if _phase == "shop":
		_show_shop_panel()
	elif _phase == "battle":
		_refresh_status("战斗中无法打开商店，请在关末进入")
	else:
		_show_shop_panel()


func _on_next_level() -> void:
	_hide_shop_panel()
	if _campaign.current_level >= _campaign.config.total_levels:
		_show_game_over("最终挑战结束", "赌场老板未被击败，无法通关。")
		return
	_start_level(_campaign.current_level + 1)


## 点击点是否落在桌面覆盖控件上（牌堆/商店/重新开始）。
## 这些控件位于出牌区上方，_input 先于 gui 消费，命中时应跳过双击"不要"判定。
