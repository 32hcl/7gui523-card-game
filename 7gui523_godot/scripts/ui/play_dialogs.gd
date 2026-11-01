class_name PlayDialogs
extends RefCounted

const ENTRY_TICKET_DOLLARS := GameConfig.ENTRY_TICKET_DOLLARS

var _owner: Node

func _init(owner: Node) -> void:
	_owner = owner

func _sbtn(btn: Button, bg: Color, tc: Color) -> void:
	_owner.style_button_balatro(btn, bg, tc)

func _fb(bg: Color, brd: Color, bw: int, cr: int, clr: Color, pad: int, sep: int) -> StyleBoxFlat:
	return _owner.flat_border(bg, brd, bw, cr, clr, pad, sep)

func show_entry_ticket(overlay: Control, dollar: int, on_confirm: Callable, on_back: Callable) -> void:
	var panel := _ticket_panel(overlay)
	var vb := VBoxContainer.new()
	vb.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	vb.offset_left = 22; vb.offset_top = 18; vb.offset_right = -22; vb.offset_bottom = -18
	vb.add_theme_constant_override("separation", 12)
	panel.add_child(vb)

	_ticket_text_block(vb, dollar)
	_ticket_buttons(vb, panel, on_confirm, on_back)


## 门票弹窗：居中面板
func _ticket_panel(overlay: Control) -> Panel:
	var panel := Panel.new()
	panel.add_theme_stylebox_override("panel", _fb(
		_owner.COL_PANEL, _owner.COL_GOLD.darkened(0.3), 2, 10, Color(0, 0, 0, 0.55), 12, 4))
	panel.set_anchors_and_offsets_preset(Control.PRESET_CENTER)
	panel.offset_left = -230
	panel.offset_top = -150
	panel.offset_right = 230
	panel.offset_bottom = 150
	panel.z_index = 5
	overlay.add_child(panel)
	return panel


## 门票弹窗：标题与说明文案
func _ticket_text_block(vb: VBoxContainer, dollar: int) -> void:
	var title := Label.new()
	title.text = "鸿运赌场 · 入场"
	title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	title.add_theme_font_size_override("font_size", 18)
	title.add_theme_color_override("font_color", _owner.COL_GOLD)
	vb.add_child(title)

	vb.add_child(_hsep())

	var body := Label.new()
	body.text = "入场需要 %d 金币。购买门票后即可进入赌场。\n\n当前金币：%d\n\n是否购买赌场门票？" % [ENTRY_TICKET_DOLLARS, dollar]
	body.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	body.add_theme_font_size_override("font_size", 13)
	body.add_theme_color_override("font_color", _owner.COL_TEXT)
	body.autowrap_mode = TextServer.AUTOWRAP_WORD
	vb.add_child(body)

	vb.add_child(_hsep())


## 门票弹窗：购买 / 暂不进入 / 回到主界面
func _ticket_buttons(vb: VBoxContainer, panel: Panel, on_confirm: Callable, on_back: Callable) -> void:
	var btns := HBoxContainer.new()
	btns.alignment = BoxContainer.ALIGNMENT_CENTER
	btns.add_theme_constant_override("separation", 14)
	vb.add_child(btns)

	var confirm_btn := Button.new()
	confirm_btn.text = "购买门票"
	confirm_btn.custom_minimum_size = Vector2(130, 40)
	_sbtn(confirm_btn, _owner.COL_GOLD, Color(0.10, 0.08, 0.06))
	btns.add_child(confirm_btn)

	var cancel_btn := Button.new()
	cancel_btn.text = "暂不进入"
	cancel_btn.custom_minimum_size = Vector2(110, 40)
	_sbtn(cancel_btn, Color(0.18, 0.17, 0.19), Color(0.88, 0.86, 0.83))
	btns.add_child(cancel_btn)

	var back_btn := Button.new()
	back_btn.text = "回到主界面"
	back_btn.custom_minimum_size = Vector2(110, 40)
	_sbtn(back_btn, Color(0.18, 0.17, 0.19), Color(0.88, 0.86, 0.83))
	btns.add_child(back_btn)

	confirm_btn.pressed.connect(func():
		## 只关闭弹窗本身；overlay 可能是游戏根节点，不能一并释放
		panel.queue_free()
		on_confirm.call())
	cancel_btn.pressed.connect(func(): panel.queue_free())
	back_btn.pressed.connect(func():
		panel.queue_free()
		on_back.call())


func show_rules(overlay: Control, rules_text: String) -> void:
	var panel := Panel.new()
	panel.add_theme_stylebox_override("panel", _fb(
		_owner.COL_PANEL, _owner.COL_GOLD.darkened(0.3), 2, 10, Color(0, 0, 0, 0.5), 10, 4))
	panel.set_anchors_and_offsets_preset(Control.PRESET_CENTER)
	panel.offset_left = -280
	panel.offset_top = -320
	panel.offset_right = 280
	panel.offset_bottom = 320
	panel.z_index = 5
	overlay.add_child(panel)

	var vb := VBoxContainer.new()
	vb.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	vb.offset_left = 16; vb.offset_top = 14; vb.offset_right = -16; vb.offset_bottom = -14
	vb.add_theme_constant_override("separation", 10)
	panel.add_child(vb)

	var title := Label.new()
	title.text = "游戏规则"
	title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	title.add_theme_font_size_override("font_size", 20)
	title.add_theme_color_override("font_color", _owner.COL_GOLD)
	vb.add_child(title)

	var scroll := ScrollContainer.new()
	scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
	vb.add_child(scroll)
	var body := Label.new()
	body.text = rules_text
	body.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	body.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	body.add_theme_font_size_override("font_size", 13)
	body.add_theme_color_override("font_color", _owner.COL_TEXT)
	scroll.add_child(body)

	var close_btn := Button.new()
	close_btn.text = "关闭"
	close_btn.custom_minimum_size = Vector2(140, 40)
	close_btn.size_flags_horizontal = Control.SIZE_SHRINK_CENTER
	_sbtn(close_btn, Color(0.18, 0.17, 0.19), Color(0.88, 0.86, 0.83))
	vb.add_child(close_btn)
	close_btn.pressed.connect(func(): panel.queue_free())


## 训练设置面板的选择状态；ai_idx 用单元素数组，便于单选回调写入
class TrainingPick:
	var ai_levels: Array[int] = []
	var ai_idx: Array[int] = [0]
	var rule_keys: Array[String] = []
	var rule_checks: Array[CheckBox] = []
	var show_hand_cb: CheckBox


func show_training_setup(overlay: Control, on_start: Callable) -> void:
	var panel := Panel.new()
	panel.add_theme_stylebox_override("panel", _fb(
		_owner.COL_PANEL, _owner.COL_GOLD.darkened(0.3), 2, 10, Color(0, 0, 0, 0.55), 12, 4))
	panel.set_anchors_and_offsets_preset(Control.PRESET_CENTER)
	panel.offset_left = -280
	panel.offset_top = -340
	panel.offset_right = 280
	panel.offset_bottom = 340
	panel.z_index = 5
	overlay.add_child(panel)

	var scroll := ScrollContainer.new()
	scroll.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	scroll.offset_left = 16; scroll.offset_top = 14; scroll.offset_right = -16; scroll.offset_bottom = -14
	panel.add_child(scroll)

	var vb := VBoxContainer.new()
	vb.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	vb.add_theme_constant_override("separation", 8)
	scroll.add_child(vb)

	_build_training_title(vb)
	var pick := _build_training_ai_section(vb)
	_build_training_rule_section(vb, pick)
	pick.show_hand_cb = _build_show_hand_check(vb)
	_build_training_actions(vb, overlay, panel, pick, on_start)


## 训练设置：标题
func _build_training_title(vb: VBoxContainer) -> void:
	var title := Label.new()
	title.text = "训练模式"
	title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	title.add_theme_font_size_override("font_size", 20)
	title.add_theme_color_override("font_color", _owner.COL_GOLD)
	vb.add_child(title)


## 训练设置：分节标题（居中 + 弱色）
func _build_section_label(vb: VBoxContainer, text: String) -> void:
	var lbl := Label.new()
	lbl.text = text
	lbl.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	lbl.add_theme_font_size_override("font_size", 13)
	lbl.add_theme_color_override("font_color", _owner.COL_TEXT_DIM)
	vb.add_child(lbl)


## 训练设置：AI 对手单选
func _build_training_ai_section(vb: VBoxContainer) -> TrainingPick:
	var pick := TrainingPick.new()
	pick.ai_levels = [AICore.Level.AI1_RULE, AICore.Level.AI2_GREEDY,
		AICore.Level.AI3_TRAINED, AICore.Level.AI4_TRAINED]
	var ai_names := ["AI1 · 规则引擎（原 Boss）", "AI2 · GA 训练权重",
		"AI3 · 默认权重", "AI4 · 备选权重"]
	_build_section_label(vb, "—— AI 对手 ——")
	var ai_group := ButtonGroup.new()
	for i in range(len(ai_names)):
		var radio := CheckBox.new()
		radio.text = ai_names[i]
		radio.button_group = ai_group
		radio.add_theme_font_size_override("font_size", 12)
		radio.add_theme_color_override("font_color", _owner.COL_TEXT)
		if i == 0:
			radio.button_pressed = true
		var captured := i
		radio.pressed.connect(func(): pick.ai_idx[0] = captured)
		vb.add_child(radio)
	vb.add_child(_hsep())
	return pick


## 训练设置：关卡限制多选（结果写入 pick）
func _build_training_rule_section(vb: VBoxContainer, pick: TrainingPick) -> void:
	pick.rule_keys = ["mask_player_draws", "boss_always_first", "boss_bite_damage",
		"ghost_cheat", "blind_mix", "boss_steal_max", "boss_swap_low_cards", "ban_gui523"]
	var rule_labels := ["Lv2 掩牌：摸牌隐藏", "Lv3 先手：BOSS 每轮先手",
		"Lv4 疯狗：每张牌 ×3 额外负债", "Lv5 赖账：同花强压 + 禁同点",
		"Lv6 不识数：50% 蒙混过关", "Lv7 黑老大：弃最大牌",
		"Lv8 换牌：4/6 随机替换", "Lv9 禁 7gui523"]
	_build_section_label(vb, "—— 关卡限制（可多选）——")
	for j in range(len(rule_labels)):
		var cb := CheckBox.new()
		cb.text = rule_labels[j]
		cb.add_theme_font_size_override("font_size", 12)
		cb.add_theme_color_override("font_color", _owner.COL_TEXT)
		vb.add_child(cb)
		pick.rule_checks.append(cb)
	vb.add_child(_hsep())


## 训练设置：展示对手手牌开关
func _build_show_hand_check(vb: VBoxContainer) -> CheckBox:
	var cb := CheckBox.new()
	cb.text = "展示 AI 手牌（正面可见）"
	cb.add_theme_font_size_override("font_size", 12)
	cb.add_theme_color_override("font_color", _owner.COL_TEXT)
	vb.add_child(cb)
	vb.add_child(_hsep())
	return cb


## 训练设置：开始 / 返回按钮行（开始按钮汇总并回调外层）
func _build_training_actions(vb: VBoxContainer, overlay: Control, panel: Panel,
		pick: TrainingPick, on_start: Callable) -> void:
	var btns := HBoxContainer.new()
	btns.alignment = BoxContainer.ALIGNMENT_CENTER
	btns.add_theme_constant_override("separation", 12)
	vb.add_child(btns)

	var start_btn := Button.new()
	start_btn.text = "开始训练"
	start_btn.custom_minimum_size = Vector2(130, 40)
	_sbtn(start_btn, _owner.COL_GREEN, Color(0.95, 0.95, 0.93))
	btns.add_child(start_btn)

	var back_btn := Button.new()
	back_btn.text = "返回"
	back_btn.custom_minimum_size = Vector2(100, 40)
	_sbtn(back_btn, Color(0.18, 0.17, 0.19), Color(0.88, 0.86, 0.83))
	btns.add_child(back_btn)

	start_btn.pressed.connect(func():
		overlay.queue_free()
		panel.queue_free()
		var restrictions: Dictionary = {}
		for j in range(pick.rule_keys.size()):
			restrictions[pick.rule_keys[j]] = pick.rule_checks[j].button_pressed
		on_start.call(pick.ai_levels[pick.ai_idx[0]], restrictions, pick.show_hand_cb.button_pressed))

	back_btn.pressed.connect(func(): panel.queue_free())


static func _hsep() -> HSeparator:
	var s := HSeparator.new()
	s.add_theme_constant_override("separation", 6)
	return s