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

const BOSS_CARD_W := 72
const BOSS_CARD_H := 100
const SELECTED_LIFT := 30.0
const SELECTED_SCALE := 1.12
const INSTANT_ROW_GAP := 46.0  ## 上下排净距：覆盖选中上浮+缩放，保证上排底边不被遮挡
const INSTANT_TINT := Color(0.72, 0.82, 1.0, 1.0)  ## 立刻补区淡蓝色调
const DEAL_DELAY := 0.07
const DEAL_DURATION := 0.32
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

const _TILT_MAX := 5.0

## 统一色板：4 主色（蓝/红/绿/金）+ 中性色，全部 UI 取色只许用这套
const COL_BG := Color(0.08, 0.08, 0.10)
const COL_PANEL := Color(0.14, 0.13, 0.15)
const COL_TEXT := Color(0.90, 0.88, 0.85)
const COL_TEXT_DIM := Color(0.62, 0.60, 0.57)
const COL_ACCENT := Color(0.20, 0.38, 0.58)
const COL_CRIMSON := Color(0.90, 0.24, 0.30)
const COL_GREEN := Color(0.22, 0.55, 0.32)
const COL_GOLD := Color(1.0, 0.84, 0.42)

## 手机端自适应
const MOBILE_MAX_WIDTH := 540.0
var _is_mobile := false

var _mobile_card_w: float = 0.0
var _mobile_card_h: float = 0.0
var _mobile_hand_w: float = 0.0
var _mobile_hand_h: float = 0.0
var _mobile_boss_w: float = 0.0
var _mobile_boss_h: float = 0.0

## 顶栏
@onready var _title_lbl: Label = $MainVBox/TopBar/TitleLabel
@onready var _round_lbl: Label = $MainVBox/TopBar/RoundLabel
@onready var _deck_count_lbl: Label = $MainVBox/TopBar/DeckCountLabel
@onready var _hp_player_lbl: Label = $MainVBox/TopBar/PlayerHP
@onready var _hp_boss_lbl: Label = $MainVBox/TopBar/BossHP

## Boss 区
@onready var _boss_hand: HBoxContainer = $MainVBox/BossZone/BossRow/BossHand
@onready var _boss_hand_instant: HBoxContainer = $MainVBox/BossZone/BossRow/BossHandInstant

## 桌面区（绿毡 Panel，子节点用锚点+容器定位）
@onready var _table_cards: HBoxContainer = $MainVBox/TableArea/CenterBox/TableCards
@onready var _table_score_lbl: Label = $MainVBox/TableArea/CenterBox/TableScoreLabel
@onready var _drop_target: VBoxContainer = $MainVBox/TableArea/CenterBox
@onready var _boss_deck: Control = $MainVBox/TableArea/BossDeck
@onready var _player_deck: Control = $MainVBox/TableArea/PlayerDeck
@onready var _shop_btn: Button = $MainVBox/HandArea/ShopBtn
@onready var _new_game_btn: Button = $MainVBox/HandArea/NewGameBtn

## 手牌区（双 HBoxContainer：上排全伤害 / 下排主力）
@onready var _hand_area: Panel = $MainVBox/HandArea
@onready var _top_hand: HBoxContainer = $MainVBox/HandArea/HandVBox/TopHand
@onready var _bottom_hand: HBoxContainer = $MainVBox/HandArea/HandVBox/BottomHand

## GameOver
@onready var _game_over_layer: Control = $GameOver
@onready var _game_over_msg: Label = $GameOver/Center/Box/V/Message

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

## 手牌随机抖动（拟真）：按 card_id 稳定存储旋转角与垂直偏移
var _hand_jitter: Dictionary = {}

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

## 游戏阶段：battle / shop / level_end / game_over
var _phase := "battle"

## 出牌动画：飞行幽灵卡所在的全屏遮罩层
var _anim_overlay: Control
## 本次出牌的飞行起点（全局坐标，从手牌区容器中心取）
var _play_src_global := Vector2.ZERO
## 待逐个揭示的桌面卡（动画期间 modulate.a=0）
var _table_cards_anim: Array = []
## 是否有出牌动画待播放
var _table_anim_pending := false
## 出牌动画是否进行中（期间禁止重建桌面，避免打断动画）
var _table_anim_running := false
## 当前已渲染桌面牌的签名（未变化时 _refresh_table 跳过溶解重建）
var _table_render_sig := "@init"


## 检测并应用手机竖屏布局（纯容器布局，无坐标计算）
func setup_responsive() -> void:
	var vs := get_viewport().get_visible_rect().size
	_is_mobile = vs.x < MOBILE_MAX_WIDTH
	_calc_mobile_card_sizes(vs)
	## 手牌两行容器高度跟随卡牌尺寸，保证 BottomHand 完全在可视范围内
	var card_h: float = _mobile_hand_h if _is_mobile else float(HAND_H)
	_top_hand.custom_minimum_size = Vector2(0, card_h)
	_bottom_hand.custom_minimum_size = Vector2(0, card_h)
	_hand_area.custom_minimum_size = Vector2(0, card_h * 2.0 + INSTANT_ROW_GAP + 12.0)


func _calc_mobile_card_sizes(vs: Vector2) -> void:
	## 5张主力牌铺满90%宽度，牌间有微小重叠
	var total := vs.x * 0.9
	_mobile_hand_w = total / 4.4
	_mobile_hand_h = _mobile_hand_w * 1.5
	_mobile_card_w = _mobile_hand_w * 0.72
	_mobile_card_h = _mobile_card_w * 1.4
	_mobile_boss_w = _mobile_hand_w * 0.62
	_mobile_boss_h = _mobile_boss_w * 1.4


func _setup_ui_theme() -> void:
	style_button_balatro($GameOver/Center/Box/V/AgainBtn, COL_CRIMSON, Color(0.95, 0.90, 0.88))
	style_button_balatro(_new_game_btn, Color(0.18, 0.17, 0.19), Color(0.88, 0.86, 0.83))
	style_button_balatro(_shop_btn, COL_ACCENT, Color(0.95, 0.95, 0.93))

	_title_lbl.add_theme_color_override("font_color", COL_GOLD)
	_round_lbl.add_theme_color_override("font_color", COL_TEXT)
	for lbl: Label in [_title_lbl, _round_lbl]:
		lbl.add_theme_color_override("font_outline_color", Color(0.0, 0.0, 0.0, 0.5))
		lbl.add_theme_constant_override("outline_size", 4)
		lbl.add_theme_color_override("font_shadow_color", Color(0, 0, 0, 0.5))
		lbl.add_theme_constant_override("shadow_offset_x", 2)
		lbl.add_theme_constant_override("shadow_offset_y", 2)

	## 区域分层：桌面区透明 + 涡旋 shader，BOSS/手牌区深面板
	var panel_border := Color(0.22, 0.21, 0.24)
	$MainVBox/BossZone.add_theme_stylebox_override("panel", flat_border(COL_PANEL, panel_border, 1, 8))
	_hand_area.add_theme_stylebox_override("panel", flat_border(COL_PANEL, panel_border, 1, 8))
	## 桌面绿毡边框保留，填充透明让涡旋 shader 透出
	$MainVBox/TableArea.add_theme_stylebox_override("panel", flat_border(Color.TRANSPARENT, COL_GREEN.darkened(0.30), 1, 8))

	## 信息层级：计分板金色强调（桌面视觉焦点），HP 绿/红双色
	_deck_count_lbl.add_theme_color_override("font_color", COL_TEXT)
	_table_score_lbl.add_theme_color_override("font_color", COL_GOLD)
	_hp_player_lbl.add_theme_color_override("font_color", COL_GREEN.lightened(0.25))
	_hp_boss_lbl.add_theme_color_override("font_color", COL_CRIMSON.lightened(0.15))

	$GameOver/Center/Box.add_theme_stylebox_override("panel", balatro_panel_style())
	$GameOver/Center/Box/V/Title.add_theme_color_override("font_color", COL_GOLD)

	_setup_ambiance()


func _setup_deck_display() -> void:
	## 牌堆位置由 play.tscn 锚点+容器决定（右上/左下），这里只构建视觉
	var psize := _player_deck.size
	if psize.x < 1.0:
		psize = _player_deck.custom_minimum_size
	var bsize := _boss_deck.size
	if bsize.x < 1.0:
		bsize = _boss_deck.custom_minimum_size
	_player_deck_lbl = _build_deck_visual(_player_deck, psize * 0.72)
	_boss_deck_lbl = _build_deck_visual(_boss_deck, bsize * 0.9)


func _build_deck_visual(container: Control, deck_size: Vector2 = Vector2(56, 80)) -> Label:
	for child in container.get_children():
		child.queue_free()
	var dw := deck_size.x
	var dh := deck_size.y
	var csize := container.size
	if csize.x < 1.0:
		csize = container.custom_minimum_size
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
	lbl.add_theme_color_override("font_color", COL_GOLD)
	lbl.add_theme_color_override("font_outline_color", Color(0.0, 0.0, 0.0, 0.85))
	lbl.add_theme_constant_override("outline_size", 4)
	lbl.mouse_filter = Control.MOUSE_FILTER_IGNORE
	lbl.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	container.add_child(lbl)
	return lbl


func _setup_ambiance() -> void:
	## 全局暗底（兜底，BOSS/手牌区深面板下方可见）
	var bg := ColorRect.new()
	bg.color = COL_BG
	bg.mouse_filter = Control.MOUSE_FILTER_IGNORE
	bg.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	add_child(bg)
	move_child(bg, 0)

	## 涡旋桌面背景：铺在 TableArea 面板之下
	var vortex := _make_vortex_bg()
	var table: Control = $MainVBox/TableArea
	table.add_child(vortex)
	table.move_child(vortex, 0)


## 涡旋背景 ColorRect（带 shader 材质）
func _make_vortex_bg() -> ColorRect:
	var rect := ColorRect.new()
	rect.color = COL_BG
	rect.mouse_filter = Control.MOUSE_FILTER_IGNORE
	rect.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	var mat := ShaderMaterial.new()
	mat.shader = load("res://shaders/vortex_bg.gdshader")
	## 用视口尺寸初始化 u_resolution（SCREEN_PIXEL_SIZE 在 canvas_item 中不可用）
	mat.set_shader_parameter("u_resolution", get_viewport().get_visible_rect().size)
	rect.material = mat
	return rect


func _hand_area_center() -> Vector2:
	return _bottom_hand.global_position + _bottom_hand.size * 0.5


func _boss_area_center() -> Vector2:
	return _boss_hand.global_position + _boss_hand.size * 0.5


## 根据 AI 实际打出的牌属于哪个手牌区（normal/instant），选对的 HBox 中心
func _boss_src_center(cards: Array) -> Vector2:
	if cards.is_empty():
		return _boss_area_center()
	var first_card: Card = cards[0]
	var found_instant := _boss_hand_instant.get_child_count() > 0 and first_card in _ai.instant_hand
	var src_row: HBoxContainer = _boss_hand_instant if found_instant else _boss_hand
	return src_row.global_position + src_row.size * 0.5


func _face(card: Card) -> Control:
	if _is_mobile:
		var visual := CardVisual.new(Vector2(_mobile_card_w, _mobile_card_h))
		visual.show_texture(CardVisual.texture_for_id(Card.id_for(card.point, card.suit)))
		visual.mouse_filter = Control.MOUSE_FILTER_IGNORE
		visual.size_flags_horizontal = Control.SIZE_SHRINK_CENTER
		visual.size_flags_vertical = Control.SIZE_SHRINK_CENTER
		return visual
	var visual := CardVisual.new(Vector2(CARD_W, CARD_H))
	visual.show_texture(CardVisual.texture_for_id(Card.id_for(card.point, card.suit)))
	visual.mouse_filter = Control.MOUSE_FILTER_IGNORE
	visual.size_flags_horizontal = Control.SIZE_SHRINK_CENTER
	visual.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	return visual


## 手牌卡（点击选中 / 假3D 悬停 / 拖拽）
func _hand_card(card: Card, index: int, selected: bool, card_size: Vector2 = Vector2.ZERO) -> CardVisual:
	var w: float = card_size.x if card_size.x > 0 else (_mobile_hand_w if _is_mobile else HAND_W)
	var h: float = card_size.y if card_size.y > 0 else (_mobile_hand_h if _is_mobile else HAND_H)
	var visual := CardVisual.new(Vector2(w, h))
	visual.pivot_offset = Vector2(w * 0.5, h * 0.5)
	visual.show_texture(CardVisual.texture_for_id(Card.id_for(card.point, card.suit)))
	visual.enable_fake3d(true)
	## z_index / base_z / scale / selected 由 _render_hand_block 统一写入，不在此处预设
	visual.mouse_entered.connect(func():
		visual.set_meta("_hovered", true)
		visual.modulate = Color(1.08, 1.05, 0.95, 1.0))
	visual.mouse_exited.connect(func():
		visual.set_meta("_hovered", false)
		visual.set_meta("_tilt_target", Vector2.ZERO)
		visual.to_rot(0.0, 0.0)
		## 恢复渲染时写入的基准色调（全伤害卡的淡蓝 INSTANT_TINT 等），而非硬重置白
		visual.modulate = visual.get_meta("base_modulate", Color(1.0, 1.0, 1.0, 1.0)))
	visual.gui_input.connect(func(event):
		_on_gui(visual.get_meta("hand_index", index), event, visual))
	return visual


func _tilt_visual(visual: CardVisual) -> void:
	var local := visual.get_local_mouse_position()
	var sx := visual.size.x
	var sy := visual.size.y
	var target := Vector2.ZERO
	if local.x >= sx * 0.12 and local.x <= sx * 0.88 and local.y >= sy * 0.12 and local.y <= sy * 0.88:
		var lx := clampf(inverse_lerp(0.12, 0.88, local.x / sx), 0.0, 1.0)
		var ly := clampf(inverse_lerp(0.12, 0.88, local.y / sy), 0.0, 1.0)
		target = Vector2(lerpf(-_TILT_MAX, _TILT_MAX, ly), lerpf(_TILT_MAX, -_TILT_MAX, lx))
	## 目标角变化 <0.4° 时直接跳过：避免每个 motion 事件都杀/建 Tween 造成抖动和开销
	var prev: Vector2 = visual.get_meta("_tilt_target", Vector2(INF, INF))
	if prev.distance_to(target) < 0.4:
		return
	visual.set_meta("_tilt_target", target)
	visual.to_rot(target.x, target.y, 0.08, Tween.TRANS_QUAD, Tween.EASE_OUT)


func _on_gui(index: int, event: InputEvent, visual: CardVisual) -> void:
	if event is InputEventMouseMotion:
		if not _drag_pending and not _dragging:
			_tilt_visual(visual)
	elif event is InputEventMouseButton and event.pressed \
			and event.button_index == MOUSE_BUTTON_LEFT and not _dragging and not _drag_pending:
		## 与 3D tilt 共用内圈判断：外层 12% 不响应点击
		var local: Vector2 = visual.get_local_mouse_position()
		var sx: float = visual.size.x
		var sy: float = visual.size.y
		if local.x < sx * 0.12 or local.x > sx * 0.88 or local.y < sy * 0.12 or local.y > sy * 0.88:
			return
		_drag_pending = true
		_drag_press_pos = get_global_mouse_position()
		_drag_first_index = index


func flat_border(bg_c: Color, border_c: Color, bw: int = 2, radius: int = 6,
		shadow_c: Color = Color(0, 0, 0, 0), shadow_size: int = 0, shadow_off: int = 0, content_top: int = 0) -> StyleBoxFlat:
	var sb := StyleBoxFlat.new()
	sb.bg_color = bg_c
	sb.border_color = border_c
	sb.set_border_width_all(bw)
	sb.set_corner_radius_all(radius)
	if shadow_size > 0:
		sb.shadow_color = shadow_c
		sb.shadow_size = shadow_size
		sb.shadow_offset = Vector2(0, shadow_off)
	if content_top != 0:
		sb.content_margin_top = content_top
	return sb


func style_button_balatro(btn: Button, bg: Color, text_col: Color = Color(0.95, 0.95, 0.93)) -> void:
	var shadow_c := Color(0.0, 0.0, 0.0, 0.55)
	var border_c := bg.darkened(0.18)
	btn.add_theme_stylebox_override("normal", flat_border(bg, border_c, 1, 6, shadow_c, 4, 4, 2))
	btn.add_theme_stylebox_override("hover", flat_border(bg.lightened(0.08), border_c.lightened(0.05), 1, 6, shadow_c, 5, 4, 2))
	btn.add_theme_stylebox_override("pressed", flat_border(bg.darkened(0.12), border_c.darkened(0.08), 1, 6, shadow_c, 1, 1, 6))
	btn.add_theme_stylebox_override("disabled", flat_border(Color(0.18, 0.18, 0.19), Color(0.22, 0.22, 0.23), 1, 6))
	btn.add_theme_stylebox_override("focus", flat_border(Color(0, 0, 0, 0), Color(0.35, 0.35, 0.38), 1, 6))
	btn.add_theme_color_override("font_color", text_col)
	btn.add_theme_color_override("font_hover_color", Color(1.0, 1.0, 0.97))
	btn.add_theme_color_override("font_pressed_color", text_col.darkened(0.12))
	btn.add_theme_color_override("font_disabled_color", Color(0.45, 0.44, 0.43))


func balatro_panel_style() -> StyleBoxFlat:
	return flat_border(Color(0.14, 0.13, 0.15, 0.98), Color(0.22, 0.21, 0.24), 1, 8, Color(0, 0, 0, 0.5), 12, 6)


func _log(_msg: String) -> void:
	pass