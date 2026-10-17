extends Control
class_name PlayBase

## — 第 1 层 play_base：常量 · 节点引用 · 基础视觉工具
##
## 底层的全部 @onready 节点、状态、常量、主题、牌堆构建、手牌卡构建与视觉工具。
## 不依赖动画层或商店层。

const CARD_W := 64
const CARD_H := 90
const HAND_W := 88   ## 手牌卡显示尺寸
const HAND_H := 124
const MAX_LOG := 120

const HAND_CARD_GAP := -30.0
const INSTANT_GAP := 18.0
const BOSS_CARD_W := 72
const BOSS_CARD_H := 100
const BOSS_GAP := -30.0
const BOSS_INSTANT_GAP := 26.0
const SELECTED_LIFT := 26.0
const SELECTED_SCALE := 1.12
const INSTANT_TINT := Color(0.72, 0.82, 1.0, 1.0)  ## 立刻补区淡蓝色调
const DEAL_DELAY := 0.07
const DEAL_DURATION := 0.32
const HAND_ROT_MAX := 5.0
const HAND_ARC_HALF := 0.03
const LAYOUT_TWEEN := 0.2

const PLAY_ANIM_DURATION := 0.30
const PLAY_ANIM_STAGGER := 0.055
const PLAY_ANIM_ARC := 60.0
const DISSOLVE_DURATION := 0.5

const DRAG_THRESHOLD := 8.0
const DRAG_SPREAD := 150.0
const DRAG_ARC_Y := 18.0
const DRAG_ROT := 4.0
const DRAG_RETURN_DUR := 0.18

const _TILT_MAX := 14.0

## 顶栏
@onready var _title_lbl: Label = $MainLayout/LeftPanel/TopBar/TitleLabel
@onready var _round_lbl: Label = $MainLayout/LeftPanel/TopBar/RoundLabel

## Boss 区
@onready var _boss_hand: Control = $MainLayout/LeftPanel/BossZone/BossRow/BossHand
@onready var _boss_deck: Control = $MainLayout/LeftPanel/BossZone/BossRow/BossDeck

## 桌面区
@onready var _table_cards: Control = $MainLayout/LeftPanel/TableZone/TableVBox/TableCards
@onready var _hand_type_lbl: Label = $MainLayout/LeftPanel/TableZone/TableVBox/TableInfo/HandTypeLabel

## 玩家区
@onready var _player_hand: Control = $MainLayout/LeftPanel/PlayerZone/PlayerRow/PlayerHand
@onready var _player_deck: Control = $MainLayout/LeftPanel/PlayerZone/PlayerRow/PlayerDeck

## 右面板
@onready var _log_lbl: RichTextLabel = $MainLayout/RightPanel/RightVBox/LogScroll/LogText

@onready var _hp_lbl: Label = $MainLayout/RightPanel/RightVBox/HPLabel
@onready var _play_btn: Button = $MainLayout/RightPanel/RightVBox/ButtonGrid/PlayBtn
@onready var _pass_btn: Button = $MainLayout/RightPanel/RightVBox/ButtonGrid/PassBtn
@onready var _new_game_btn: Button = $MainLayout/RightPanel/RightVBox/ButtonGrid/NewGameBtn

## GameOver
@onready var _game_over_layer: Control = $GameOver
@onready var _game_over_msg: Label = $GameOver/Center/Box/V/Message

@onready var _deck_count_lbl: Label = $MainLayout/LeftPanel/TopBar/DeckCountLabel
@onready var _shop_btn: Button = $MainLayout/RightPanel/RightVBox/ButtonGrid/ShopBtn
@onready var _hp_swap_btn: Button = $MainLayout/RightPanel/RightVBox/ButtonGrid/HpSwapBtn
@onready var _return_btn: Button = $MainLayout/RightPanel/RightVBox/ButtonGrid/ReturnBtn

var _round: Round
var _human: Player.PlayerData
var _ai: Player.PlayerData
var _human_deck: Deck.DeckManager
var _ai_deck: Deck.DeckManager

## 战役 / 商店 / 记牌器
var _campaign: Campaign
var _shop: Shop
var _tracker: AICore.CardTracker

var _human_hp := 100
var _ai_hp := 100

## 玩家当前选中的手牌下标
var _selected: Array[int] = []

## 拖拽出牌
var _drag_pending: bool = false
var _dragging: bool = false
var _drag_press_pos := Vector2.ZERO
var _drag_first_index := -1
var _drag_visuals: Array[CardVisual] = []
var _drag_indices: Array[int] = []
var _drag_origin: Array[Dictionary] = []
var _drop_hint: Panel = null

## 牌堆显示：玩家/BOSS 牌堆叠置牌背 + 剩余张数角标（也是发牌/抽卡动画的原点）
var _player_deck_lbl: Label = null
var _boss_deck_lbl: Label = null

## AI 是否正在思考
var _ai_busy := false
var _finished := false

var _log_lines: Array[String] = []

## 游戏阶段：battle / shop / level_end / game_over
var _phase := "battle"

## 出牌动画：飞行幽灵卡所在的全屏遮罩层
var _anim_overlay: Control
## 本次出牌的飞行起点（全局坐标）
var _play_src_global := Vector2.ZERO
## 待逐个揭示的桌面卡（动画期间 modulate.a=0）
var _table_cards_anim: Array = []
## 是否有出牌动画待播放
var _table_anim_pending := false
## 出牌动画是否进行中（期间禁止重建桌面，避免打断动画）
var _table_anim_running := false


## 金&朱红赌场主题
func _setup_ui_theme() -> void:
	var gold := Color(1.0, 0.84, 0.42)
	var gold_dim := Color(0.70, 0.52, 0.18)
	var crimson := Color(0.90, 0.24, 0.30)
	var crimson_deep := Color(0.62, 0.10, 0.16)
	var table_green := Color(0.06, 0.42, 0.19)
	var panel := Color(0.12, 0.06, 0.04, 0.96)

	style_button_casino(_play_btn, crimson, gold)
	style_button_casino(_pass_btn, crimson, gold)
	style_button_casino($GameOver/Center/Box/V/AgainBtn, crimson, gold)
	style_button_casino(_new_game_btn, Color(0.30, 0.12, 0.10), gold_dim)
	style_button_casino(_shop_btn, Color(0.25, 0.18, 0.08), gold_dim)
	style_button_casino(_hp_swap_btn, Color(0.20, 0.25, 0.15), gold_dim)
	style_button_casino(_return_btn, Color(0.30, 0.12, 0.10), gold_dim)

	_title_lbl.add_theme_color_override("font_color", gold)
	_round_lbl.add_theme_color_override("font_color", gold)
	for lbl: Label in [_title_lbl, _round_lbl]:
		lbl.add_theme_color_override("font_outline_color", gold_dim)
		lbl.add_theme_constant_override("outline_size", 6)
		lbl.add_theme_color_override("font_shadow_color", Color(0.15, 0.08, 0.02, 0.6))
		lbl.add_theme_constant_override("shadow_offset_x", 2)
		lbl.add_theme_constant_override("shadow_offset_y", 4)

	$GameOver/Center/Box.add_theme_stylebox_override("panel", flat_border(panel, gold, 2, 12, Color(0, 0, 0, 0.6), 24, 8))

	_setup_decor()
	_setup_ambiance()


func _setup_deck_display() -> void:
	_player_deck_lbl = _build_deck_visual(_player_deck, Vector2(70, 100))
	_boss_deck_lbl = _build_deck_visual(_boss_deck, Vector2(56, 80))


func _build_deck_visual(container: Control, deck_size: Vector2 = Vector2(56, 80)) -> Label:
	for child in container.get_children():
		child.queue_free()
	var dw := deck_size.x
	var dh := deck_size.y
	var csize := container.size
	var center := (csize - Vector2(dw, dh)) * 0.5

	for i in 3:
		var back := CardVisual.new(Vector2(dw, dh))
		back.show_back()
		back.enable_fake3d(false)
		back.mouse_filter = Control.MOUSE_FILTER_IGNORE
		back.position = center + Vector2(i * 2.0, i * 2.0)
		back.z_index = i
		container.add_child(back)

	var lbl := Label.new()
	lbl.text = "牌堆\n0"
	lbl.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	lbl.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	lbl.add_theme_color_override("font_color", Color(1.0, 0.84, 0.42))
	lbl.add_theme_color_override("font_outline_color", Color(0.0, 0.0, 0.0, 0.85))
	lbl.add_theme_constant_override("outline_size", 4)
	lbl.mouse_filter = Control.MOUSE_FILTER_IGNORE
	lbl.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	container.add_child(lbl)
	return lbl


func _setup_ambiance() -> void:
	var ve := TextureRect.new()
	var gt := GradientTexture2D.new()
	var gr := Gradient.new()
	gr.colors = PackedColorArray([Color(0.17, 0.09, 0.055), Color(0.08, 0.04, 0.028)])
	gr.offsets = PackedFloat32Array([0.0, 1.0])
	gt.gradient = gr
	gt.width = 4
	gt.height = 256
	gt.fill_from = Vector2(0.5, 0.0)
	gt.fill_to = Vector2(0.5, 1.0)
	ve.texture = gt
	ve.mouse_filter = Control.MOUSE_FILTER_IGNORE
	ve.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	add_child(ve)
	move_child(ve, 1)

	var env := Environment.new()
	env.background_mode = Environment.BG_CANVAS
	env.glow_enabled = true
	env.glow_intensity = 0.55
	env.glow_bloom = 0.12
	env.glow_blend_mode = Environment.GLOW_BLEND_MODE_ADDITIVE
	var we := WorldEnvironment.new()
	we.environment = env
	add_child(we)


## 四角金饰 + 祥云分隔
func _setup_decor() -> void:
	var corner: Texture2D = load("res://art/ui/corner_gold.svg")
	if corner != null:
		_add_corners(corner)
	var xiang: Texture2D = load("res://art/ui/xiangyun_gold.svg")
	if xiang != null:
		var t := TextureRect.new()
		t.texture = xiang
		t.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
		t.custom_minimum_size = Vector2(0, 32)
		t.mouse_filter = Control.MOUSE_FILTER_IGNORE
		t.set_anchors_and_offsets_preset(Control.PRESET_CENTER_TOP)
		add_child(t)
		move_child(t, 3)


func _add_corners(tex: Texture2D) -> void:
	var cells := [
		{ "preset": Control.PRESET_TOP_LEFT, "l": 6.0, "t": 6.0, "r": 98.0, "b": 98.0, "rot": 0 },
		{ "preset": Control.PRESET_TOP_RIGHT, "l": -98.0, "t": 6.0, "r": -6.0, "b": 98.0, "rot": 90 },
		{ "preset": Control.PRESET_BOTTOM_RIGHT, "l": -98.0, "t": -98.0, "r": -6.0, "b": -6.0, "rot": 180 },
		{ "preset": Control.PRESET_BOTTOM_LEFT, "l": 6.0, "t": -98.0, "r": 98.0, "b": -6.0, "rot": 270 },
	]
	for i in cells.size():
		var c: Dictionary = cells[i]
		var t := TextureRect.new()
		t.texture = tex
		t.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
		t.mouse_filter = Control.MOUSE_FILTER_IGNORE
		t.set_anchors_and_offsets_preset(c.preset)
		t.offset_left = c.l
		t.offset_top = c.t
		t.offset_right = c.r
		t.offset_bottom = c.b
		add_child(t)
		t.pivot_offset = t.size * 0.5
		t.rotation_degrees = c.rot
		move_child(t, 1 + i)


func _hand_area_center() -> Vector2:
	return _player_hand.global_position + _player_hand.size * 0.5


func _boss_area_center() -> Vector2:
	return _boss_hand.global_position + _boss_hand.size * 0.5


## rank 降序排列
static func _sorted_order(hand: Array) -> Array:
	var order := range(hand.size())
	order.sort_custom(func(a: int, b: int) -> bool:
		var ra := Card.get_rank(hand[a].point)
		var rb := Card.get_rank(hand[b].point)
		if ra != rb:
			return ra > rb
		return hand[a].suit < hand[b].suit)
	return order


func _hand_metrics(n: int, m: int) -> Dictionary:
	var gap := HAND_CARD_GAP
	var normal_w := float(n) * HAND_W + float(maxi(n - 1, 0)) * gap
	var instant_w := float(m) * HAND_W + float(maxi(m - 1, 0)) * gap
	var all_w := normal_w + (INSTANT_GAP if m > 0 else 0.0) + instant_w
	var cw := _player_hand.size.x
	var start_x: float = (cw - all_w) * 0.5 if cw > all_w else 0.0
	var instant_x := start_x + normal_w + (INSTANT_GAP if (m > 0 and n > 0) else 0.0)
	return {"start_x": start_x, "instant_x": instant_x, "total": n + m}


## 扇形布局单卡目标
func _fan_target(overall: int, m: int, p: int, gp: int, anchor_x: float, rot_scale: float, key: int) -> Dictionary:
	var g := gp if (overall > 1) else p
	var t := 0.5 if overall <= 1 else float(g) / float(overall - 1)
	var rot_z := lerpf(-HAND_ROT_MAX, HAND_ROT_MAX, t) * rot_scale
	var c := lerpf(-HAND_ARC_HALF, HAND_ARC_HALF, t * 2.0) if t <= 0.5 \
			else lerpf(HAND_ARC_HALF, -HAND_ARC_HALF, (t - 0.5) * 2.0)
	var arc_y := -c * float(HAND_H)
	var pos := Vector2(anchor_x + float(p) * (HAND_W + HAND_CARD_GAP), arc_y)
	if _selected.has(key):
		pos += Vector2(sin(deg_to_rad(rot_z)), -cos(deg_to_rad(rot_z))) * SELECTED_LIFT
	return {"pos": pos, "rot": rot_z}


func _face(card: Card) -> Control:
	var visual := CardVisual.new(Vector2(CARD_W, CARD_H))
	visual.show_texture(CardVisual.texture_for_id(Card.id_for(card.point, card.suit)))
	visual.mouse_filter = Control.MOUSE_FILTER_IGNORE
	visual.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	return visual


## 手牌卡（点击选中 / 假3D 悬停 / 拖拽）
func _hand_card(card: Card, index: int, selected: bool) -> CardVisual:
	var visual := CardVisual.new(Vector2(HAND_W, HAND_H))
	visual.show_texture(CardVisual.texture_for_id(Card.id_for(card.point, card.suit)))
	visual.enable_fake3d(true)
	if selected:
		visual.set_selected(true)
		visual.scale = Vector2(SELECTED_SCALE, SELECTED_SCALE)
		visual.base_z = 10
		visual.z_index = 10
	else:
		visual.set_selected(false)
		visual.scale = Vector2(1.0, 1.0)
		visual.base_z = index
		visual.z_index = index
	visual.mouse_entered.connect(func():
		visual.set_meta("_hovered", true)
		visual.modulate = Color(1.08, 1.05, 0.95, 1.0)
		visual.z_index = 100)
	visual.mouse_exited.connect(func():
		visual.set_meta("_hovered", false)
		visual.to_rot(0.0, 0.0)
		visual.modulate = Color(1.0, 1.0, 1.0, 1.0)
		visual.z_index = visual.base_z)
	visual.gui_input.connect(func(event):
		_on_gui(visual.get_meta("hand_index", index), event, visual))
	return visual


func _tilt_visual(visual: CardVisual) -> void:
	var local := visual.get_local_mouse_position()
	var lx := clampf(local.x / visual.size.x, 0.0, 1.0)
	var ly := clampf(local.y / visual.size.y, 0.0, 1.0)
	var rx := lerpf(_TILT_MAX, -_TILT_MAX, ly)
	var ry := lerpf(-_TILT_MAX, _TILT_MAX, lx)
	visual.to_rot(rx, ry, 0.08, Tween.TRANS_QUAD, Tween.EASE_OUT)


func _on_gui(index: int, event: InputEvent, visual: CardVisual) -> void:
	if event is InputEventMouseMotion:
		if not _drag_pending and not _dragging:
			_tilt_visual(visual)
	elif event is InputEventMouseButton and event.pressed \
			and event.button_index == MOUSE_BUTTON_LEFT and not _dragging:
		_drag_pending = true
		_drag_press_pos = get_global_mouse_position()
		_drag_first_index = index


func flat_border(bg_c: Color, border_c: Color, bw: int = 2, radius: int = 6,
		shadow_c: Color = Color(0, 0, 0, 0), shadow_size: int = 0, shadow_off: int = 0) -> StyleBoxFlat:
	var sb := StyleBoxFlat.new()
	sb.bg_color = bg_c
	sb.border_color = border_c
	sb.set_border_width_all(bw)
	sb.set_corner_radius_all(radius)
	if shadow_size > 0:
		sb.shadow_color = shadow_c
		sb.shadow_size = shadow_size
		sb.shadow_offset = Vector2(0, shadow_off)
	return sb


func style_button_casino(btn: Button, bg: Color, border: Color) -> void:
	var gold_glow := Color(border.r * 0.7, border.g * 0.55, border.b * 0.2, 0.55)
	btn.add_theme_stylebox_override("normal", flat_border(bg, border, 2, 9, gold_glow, 8, 3))
	btn.add_theme_stylebox_override("hover", flat_border(bg.lightened(0.12), border.lightened(0.15), 2, 9, gold_glow, 16, 4))
	btn.add_theme_stylebox_override("pressed", flat_border(bg.darkened(0.18), border.darkened(0.25), 2, 9, Color(0, 0, 0, 0.4), 6, 2))
	btn.add_theme_stylebox_override("disabled", flat_border(Color(0.24, 0.18, 0.16), Color(0.30, 0.24, 0.18), 2, 9))
	btn.add_theme_stylebox_override("focus", flat_border(Color(0, 0, 0, 0), Color(0.3, 0.3, 0.3), 3, 9))
	btn.add_theme_color_override("font_color", Color(1, 0.96, 0.85))
	btn.add_theme_color_override("font_hover_color", Color(1, 1, 0.92))
	btn.add_theme_color_override("font_pressed_color", Color(1, 0.82, 0.62))
	btn.add_theme_color_override("font_disabled_color", Color(0.55, 0.45, 0.35))


# ═══════════════════════════════════════════════════════════════
# 日志
# ═══════════════════════════════════════════════════════════════

func _log(msg: String) -> void:
	_log_lines.append(msg)
	while _log_lines.size() > MAX_LOG:
		_log_lines.pop_front()
	if is_instance_valid(_log_lbl):
		_log_lbl.clear()
		for line in _log_lines:
			_log_lbl.append_text("> " + line + "\n")
		_scroll_log_to_bottom.call_deferred()


func _scroll_log_to_bottom() -> void:
	if not is_instance_valid(_log_lbl):
		return
	var scroll := _log_lbl.get_parent()
	if scroll is ScrollContainer:
		scroll.scroll_vertical = int(scroll.get_v_scroll_bar().max_value)