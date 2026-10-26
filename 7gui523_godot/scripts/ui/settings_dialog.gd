class_name SettingsDialog
extends RefCounted

## 设置弹窗：挂到任意 Control 上，关闭后自动清理。
## 用法: SettingsDialog.show(menu, play_scene)

var _panel: Panel
var _play_scene: Node


static func show(menu: Control, play_scene: Node) -> void:
	## 重复点击「设置」时先收掉上一个弹窗，避免面板层层叠加
	var prev: SettingsDialog = play_scene._settings_dialog
	if is_instance_valid(prev) and is_instance_valid(prev._panel):
		prev._panel.queue_free()
		play_scene._settings_dialog = null
	var d := SettingsDialog.new()
	d._build(menu, play_scene)
	## 防止 RefCounted 被 GC 回收导致信号断开
	play_scene._settings_dialog = d


func _build(menu: Control, play_scene: Node) -> void:
	_play_scene = play_scene

	_panel = Panel.new()
	_panel.add_theme_stylebox_override("panel", play_scene.flat_border(
		play_scene.COL_PANEL, play_scene.COL_GOLD.darkened(0.3), 2, 10, Color(0, 0, 0, 0.5), 10, 4))
	_panel.set_anchors_and_offsets_preset(Control.PRESET_CENTER)
	_panel.offset_left = -230
	_panel.offset_top = -200
	_panel.offset_right = 230
	_panel.offset_bottom = 200
	_panel.z_index = 5
	menu.add_child(_panel)

	var vb := VBoxContainer.new()
	vb.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	vb.offset_left = 20
	vb.offset_top = 16
	vb.offset_right = -20
	vb.offset_bottom = -16
	vb.add_theme_constant_override("separation", 12)
	_panel.add_child(vb)

	var title := Label.new()
	title.text = "游戏设置"
	title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	title.add_theme_font_size_override("font_size", 20)
	title.add_theme_color_override("font_color", play_scene.COL_GOLD)
	vb.add_child(title)

	var sub := Label.new()
	sub.text = "开局后不可更改"
	sub.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	sub.add_theme_font_size_override("font_size", 11)
	sub.add_theme_color_override("font_color", play_scene.COL_TEXT_DIM)
	vb.add_child(sub)

	_add_sep(vb)
	_sort_section(vb, play_scene)
	_add_sep(vb)
	_pixel_section(vb, play_scene)
	_add_sep(vb)

	var close_btn := Button.new()
	close_btn.text = "关闭"
	close_btn.custom_minimum_size = Vector2(140, 40)
	close_btn.size_flags_horizontal = Control.SIZE_SHRINK_CENTER
	play_scene.style_button_balatro(close_btn, Color(0.18, 0.17, 0.19), Color(0.88, 0.86, 0.83))
	vb.add_child(close_btn)
	close_btn.pressed.connect(_close_pressed)


func _close_pressed() -> void:
	_play_scene._apply_pixelation()
	_panel.queue_free()
	_play_scene._settings_dialog = null


func _add_sep(parent: Control) -> void:
	var sep := HSeparator.new()
	sep.add_theme_constant_override("separation", 8)
	parent.add_child(sep)


func _sort_section(parent: Control, s: Node) -> void:
	var lbl := Label.new()
	lbl.text = "补牌排序方式"
	lbl.add_theme_font_size_override("font_size", 14)
	lbl.add_theme_color_override("font_color", s.COL_TEXT)
	parent.add_child(lbl)

	var row := HBoxContainer.new()
	row.alignment = BoxContainer.ALIGNMENT_CENTER
	row.add_theme_constant_override("separation", 10)
	parent.add_child(row)

	var modes := [Player.CardSortMode.SAME_RANK, Player.CardSortMode.FULL_SORT, Player.CardSortMode.NO_SORT]
	var texts := ["同点归位", "全排序", "不排序"]
	var btns: Array[Button] = []

	for i in modes.size():
		var btn := Button.new()
		btn.text = texts[i]
		btn.custom_minimum_size = Vector2(100, 36)
		btns.append(btn)
		row.add_child(btn)
		var mode: int = modes[i]
		btn.pressed.connect(_on_sort_picked.bind(mode, btns, btn, s))
		if mode == s._card_sort_mode:
			s.style_button_balatro(btn, s.COL_GOLD, Color(0.12, 0.10, 0.08))
		else:
			s.style_button_balatro(btn, Color(0.18, 0.17, 0.19), Color(0.88, 0.86, 0.83))


func _on_sort_picked(mode: int, btns: Array, active_btn: Button, s: Node) -> void:
	s._card_sort_mode = mode
	for b: Button in btns:
		s.style_button_balatro(b, Color(0.18, 0.17, 0.19), Color(0.88, 0.86, 0.83))
	s.style_button_balatro(active_btn, s.COL_GOLD, Color(0.12, 0.10, 0.08))


func _pixel_section(parent: Control, s: Node) -> void:
	var lbl := Label.new()
	lbl.text = "桌面特效像素化"
	lbl.add_theme_font_size_override("font_size", 14)
	lbl.add_theme_color_override("font_color", s.COL_TEXT)
	parent.add_child(lbl)

	var row := HBoxContainer.new()
	row.alignment = BoxContainer.ALIGNMENT_CENTER
	row.add_theme_constant_override("separation", 16)
	parent.add_child(row)

	var on_btn := Button.new()
	on_btn.text = "◼ 开"
	on_btn.custom_minimum_size = Vector2(100, 36)
	var off_btn := Button.new()
	off_btn.text = "◻ 关"
	off_btn.custom_minimum_size = Vector2(100, 36)
	row.add_child(on_btn)
	row.add_child(off_btn)

	_refresh_pixel_btns(on_btn, off_btn, s)
	on_btn.pressed.connect(_on_pixel_on.bind(on_btn, off_btn, s))
	off_btn.pressed.connect(_on_pixel_off.bind(on_btn, off_btn, s))


func _refresh_pixel_btns(on_btn: Button, off_btn: Button, s: Node) -> void:
	if s._pixelate_enabled:
		s.style_button_balatro(on_btn, s.COL_GREEN, Color(0.12, 0.10, 0.08))
		s.style_button_balatro(off_btn, Color(0.18, 0.17, 0.19), Color(0.88, 0.86, 0.83))
	else:
		s.style_button_balatro(off_btn, s.COL_CRIMSON, Color(0.12, 0.10, 0.08))
		s.style_button_balatro(on_btn, Color(0.18, 0.17, 0.19), Color(0.88, 0.86, 0.83))


func _on_pixel_on(on_btn: Button, off_btn: Button, s: Node) -> void:
	s._pixelate_enabled = true
	_refresh_pixel_btns(on_btn, off_btn, s)


func _on_pixel_off(on_btn: Button, off_btn: Button, s: Node) -> void:
	s._pixelate_enabled = false
	_refresh_pixel_btns(on_btn, off_btn, s)