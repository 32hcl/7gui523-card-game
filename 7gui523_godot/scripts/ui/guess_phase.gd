class_name GuessPhase
extends RefCounted

const GUESS_FLIP_DELAY := 0.6            ## 猜先翻牌延迟
const GUESS_REVEAL_DELAY := 1.5          ## 猜先揭晓停留
const GUESS_BIG_PROB := 0.5              ## 猜先出大鬼概率
const FLIP_UP_DURATION := 0.14           ## 翻牌上半段(立牌→平转)
const FLIP_DOWN_DURATION := 0.32         ## 翻牌下半段(弹回正面)

## 猜先阶段组件。
## 由 play.gd 在关卡开始前调用 run()，等待玩家猜大小鬼，返回是否猜对（玩家先手）。

## 猜先界面节点集合（构建与揭晓分两步处理）
class GuessUI:
	var overlay: Control
	var title: Label
	var card_vis: CardVisual
	var mid_row: HBoxContainer
	var hint: Label
	var big_btn: Button
	var small_btn: Button


var _play: Node


func _init(play: Node) -> void:
	_play = play


## 执行猜先流程，返回 true 表示玩家先手
func run() -> bool:
	var zone: Control = _play.get_node("MainVBox/TableArea")
	var ui := _build_overlay(zone)

	var result := [0]   ## 0=未选 , 1=猜大鬼 , 2=猜小鬼
	ui.big_btn.pressed.connect(func(): result[0] = 1)
	ui.small_btn.pressed.connect(func(): result[0] = 2)
	while result[0] == 0:
		await _play.get_tree().process_frame

	return await _reveal(ui, result[0] == 1)


## 构建猜先覆盖层：透明镂空，桌面背景自然透出
func _build_overlay(zone: Control) -> GuessUI:
	var ui := GuessUI.new()
	ui.overlay = _build_root(zone)
	var outer := _build_panel(ui.overlay)
	_build_title(outer, ui)
	_build_card(outer, ui)
	_build_buttons(outer, ui)
	_build_hint(outer, ui)
	return ui


## 猜先覆盖层根节点：全屏 Control + 全透明点击隔离层
func _build_root(zone: Control) -> Control:
	var overlay := Control.new()
	overlay.name = "GuessOverlay"
	overlay.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	overlay.z_index = 60
	overlay.mouse_filter = Control.MOUSE_FILTER_IGNORE
	zone.add_child(overlay)

	var dim := ColorRect.new()
	dim.color = Color(0.0, 0.0, 0.0, 0.0)
	dim.mouse_filter = Control.MOUSE_FILTER_STOP
	dim.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	overlay.add_child(dim)
	return overlay


## 猜先中央面板：透明镂空无底色，返回内部纵向容器
func _build_panel(overlay: Control) -> VBoxContainer:
	var panel := Panel.new()
	panel.mouse_filter = Control.MOUSE_FILTER_PASS
	panel.add_theme_stylebox_override("panel", _play.flat_border(
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
	return outer


## 猜先标题
func _build_title(outer: VBoxContainer, ui: GuessUI) -> void:
	var title := Label.new()
	title.text = "猜牌决定先手"
	title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	title.add_theme_font_size_override("font_size", 20)
	title.add_theme_color_override("font_color", Color(1.0, 0.84, 0.42))
	title.add_theme_color_override("font_outline_color", Color(0.3, 0.2, 0.08, 1.0))
	title.add_theme_constant_override("outline_size", 5)
	outer.add_child(title)
	ui.title = title


## 猜先牌位：点击前显示牌背，翻牌后用伪3D 立牌翻面揭晓大鬼/小鬼
func _build_card(outer: VBoxContainer, ui: GuessUI) -> void:
	var card_vis := CardVisual.new(Vector2(100, 140))
	card_vis.show_back()
	card_vis.enable_fake3d(true)
	card_vis.mouse_filter = Control.MOUSE_FILTER_IGNORE
	card_vis.custom_minimum_size = Vector2(100, 140)
	card_vis.size_flags_horizontal = Control.SIZE_SHRINK_CENTER
	outer.add_child(card_vis)
	ui.card_vis = card_vis


## 猜先按钮行：猜大鬼 / 猜小鬼
func _build_buttons(outer: VBoxContainer, ui: GuessUI) -> void:
	var mid_row := HBoxContainer.new()
	mid_row.alignment = BoxContainer.ALIGNMENT_CENTER
	mid_row.add_theme_constant_override("separation", 24)
	var big_btn := Button.new()
	big_btn.text = "猜大鬼"
	big_btn.custom_minimum_size = Vector2(120, 46)
	var small_btn := Button.new()
	small_btn.text = "猜小鬼"
	small_btn.custom_minimum_size = Vector2(120, 46)
	_play.style_button_balatro(big_btn, Color(0.90, 0.24, 0.30), Color(1.0, 0.84, 0.42))
	_play.style_button_balatro(small_btn, Color(0.20, 0.38, 0.58), Color(1.0, 0.84, 0.42))
	mid_row.add_child(big_btn)
	mid_row.add_child(small_btn)
	outer.add_child(mid_row)
	ui.mid_row = mid_row
	ui.big_btn = big_btn
	ui.small_btn = small_btn


## 猜先提示小字
func _build_hint(outer: VBoxContainer, ui: GuessUI) -> void:
	var hint := Label.new()
	hint.text = "大鬼（上）与小鬼（下）其中一张，猜对先手"
	hint.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	hint.add_theme_font_size_override("font_size", 12)
	hint.add_theme_color_override("font_color", Color(0.62, 0.60, 0.57))
	outer.add_child(hint)
	ui.hint = hint


## 揭晓猜先结果：伪3D 立牌翻面，返回是否猜对
func _reveal(ui: GuessUI, guessed_big: bool) -> bool:
	ui.mid_row.visible = false
	ui.title.text = "揭晓..."
	ui.hint.text = ""

	var actual_big: bool = randf() < GUESS_BIG_PROB
	var correct: bool = guessed_big == actual_big

	await _play.get_tree().create_timer(GUESS_FLIP_DELAY).timeout
	ui.card_vis.enable_fake3d(true)
	var flip_up: Tween = ui.card_vis.to_rot(0.0, 90.0, FLIP_UP_DURATION, Tween.TRANS_CUBIC, Tween.EASE_IN_OUT)
	if flip_up:
		await flip_up.finished
	ui.card_vis.show_id(53 if actual_big else 52)   ## 53=大鬼 , 52=小鬼
	ui.card_vis.to_rot(0.0, 0.0, FLIP_DOWN_DURATION, Tween.TRANS_BACK, Tween.EASE_OUT)
	ui.title.text = "✓ 猜对了！你先手" if correct else "✗ 猜错了！电脑先手"
	ui.title.add_theme_color_override("font_color", Color(0.22, 0.55, 0.32) if correct else Color(0.90, 0.24, 0.30))

	await _play.get_tree().create_timer(GUESS_REVEAL_DELAY).timeout
	ui.overlay.queue_free()
	return correct