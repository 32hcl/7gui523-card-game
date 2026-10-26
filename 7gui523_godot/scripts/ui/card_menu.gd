class_name CardMenu
extends Control

## 独立卡牌主菜单：5 张卡牌扇形排列，悬停置顶，信号解耦
##
## 构成规则（用户指定的具体参数）:
##   卡牌 200x300，pivot_offset 设在底部中心 (100,300)
##   总扇角 30°，5 张均匀分布：-15° -7.5° 0° +7.5° +15°
##   X 间距 80px，Y 统一（底部对齐）
##   悬停时旋转拉回 0°、放大 1.12x、上移、z_index=100
##   退出防抖 0.1s
##
## 节点结构（.tscn 中预置）:
##   CardMenu (Control, root, mouse_filter=STOP)
##     DimBG (ColorRect, mouse_filter=IGNORE)
##     CardContainer (Control, 布局容器)
##       HitboxN (Control, mouse_filter=STOP, 透明水平判定区)
##       Card0..Card4 (Control, mouse_filter=PASS)
##         CardBody (Panel, IGNORE)
##         CardLabel (Label, IGNORE)
##         CardTween (Tween)
##
## 使用方式（外部调用方）:
##   var menu := preload("res://scenes/card_menu.tscn").instantiate()
##   menu.card_selected.connect(_on_card_selected)
##   add_child(menu)

# ═══════════════════════════════════════════════════════════════
# 信号
# ═══════════════════════════════════════════════════════════════

signal card_selected(index: int)

# ═══════════════════════════════════════════════════════════════
# 编辑器可配置：卡牌文本
# ═══════════════════════════════════════════════════════════════

@export var card_0_text: String = "New Card"
@export var card_1_text: String = "Settings"
@export var card_2_text: String = "Continue"
@export var card_3_text: String = "Bonus"
@export var card_4_text: String = "Exit"

# ═══════════════════════════════════════════════════════════════
# 编辑器可配置：扇形布局
# ═══════════════════════════════════════════════════════════════

@export var fan_angle_degrees: float = 30.0       ## 扇形总角度（最左到最右）
@export var card_spacing_x: float = 80.0           ## 相邻卡牌 X 轴间距（像素）
@export var card_y_margin: float = 40.0            ## 卡牌底部距容器底边的距离

# ═══════════════════════════════════════════════════════════════
# 编辑器可配置：悬停动画
# ═══════════════════════════════════════════════════════════════

@export var hover_scale: float = 1.12              ## 悬停时放大比例
@export var hover_lift: float = -40.0              ## 悬停时上移像素（负=向上）
@export var hover_duration: float = 0.20           ## 悬停动画时长（秒）
@export var hover_z_index: int = 100               ## 悬停时 z_index

# ═══════════════════════════════════════════════════════════════
# 编辑器可配置：防抖
# ═══════════════════════════════════════════════════════════════

@export var enter_debounce_ms: float = 50.0        ## 进入防抖延迟(毫秒)
@export var exit_debounce_ms: float = 100.0        ## 退出防抖延迟(毫秒)

# ═══════════════════════════════════════════════════════════════
# 常量
# ═══════════════════════════════════════════════════════════════

const DIM_COLOR := Color(0.08, 0.08, 0.10, 0.92)
const CARD_BG := Color(0.14, 0.13, 0.15, 0.85)
const CARD_BORDER := Color(1.0, 0.84, 0.42, 0.45)
const TEXT_COLOR := Color(0.90, 0.88, 0.85, 1.0)
const CARD_SIZE := Vector2(200, 300)
const CARD_PIVOT := Vector2(100, 300)
const CARD_COUNT := 5

# ═══════════════════════════════════════════════════════════════
# 内部状态
# ═══════════════════════════════════════════════════════════════

var _cards: Array = []
var _card_bodies: Array = []
var _card_labels: Array = []
var _card_tweens: Array = []
var _card_base_positions: Array = []
var _card_base_rotations: Array = []
var _card_base_z: Array = []
var _enter_gen: Array = []
var _hovered_index: int = -1

# ═══════════════════════════════════════════════════════════════
# 生命周期
# ═══════════════════════════════════════════════════════════════

func _ready() -> void:
	_collect_children()
	_setup_card_visuals()
	_apply_card_texts()
	_connect_signals()
	await get_tree().process_frame
	_recalculate_layout()


func _collect_children() -> void:
	var container := $CardContainer
	for i in range(CARD_COUNT):
		var card := container.get_node("Card%d" % i)
		if not card:
			continue
		_cards.append(card)
		_card_bodies.append(card.get_node("CardBody"))
		_card_labels.append(card.get_node("CardLabel"))
		_card_tweens.append(card.get_node("CardTween"))
		_card_base_z.append(card.z_index)
		_card_base_positions.append(card.position)
		_card_base_rotations.append(card.rotation)
		_enter_gen.append(0)


func _setup_card_visuals() -> void:
	for i in range(min(_cards.size(), CARD_COUNT)):
		var card: Control = _cards[i]
		card.pivot_offset = CARD_PIVOT

		var body := _card_bodies[i] as Panel
		if body:
			var sb := StyleBoxFlat.new()
			sb.bg_color = CARD_BG
			sb.border_color = CARD_BORDER
			sb.set_border_width_all(2)
			sb.set_corner_radius_all(10)
			body.add_theme_stylebox_override("panel", sb)

		var label := _card_labels[i] as Label
		if label:
			label.add_theme_font_size_override("font_size", 18)
			label.add_theme_color_override("font_color", TEXT_COLOR)
			label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
			label.vertical_alignment = VERTICAL_ALIGNMENT_CENTER


# ═══════════════════════════════════════════════════════════════
# 文本注入
# ═══════════════════════════════════════════════════════════════

func _apply_card_texts() -> void:
	var texts := [card_0_text, card_1_text, card_2_text, card_3_text, card_4_text]
	for i in range(min(_cards.size(), CARD_COUNT)):
		_card_labels[i].text = texts[i]


# ═══════════════════════════════════════════════════════════════
# 扇形布局（pivot 底部中心 + X 间距 + 旋转）
# ═══════════════════════════════════════════════════════════════

func _recalculate_layout() -> void:
	var container: Control = $CardContainer
	var cw := container.size.x
	var ch := container.size.y

	var base_y := ch - CARD_PIVOT.y - card_y_margin
	var half_span := deg_to_rad(fan_angle_degrees / 2.0)

	for i in range(min(_cards.size(), CARD_COUNT)):
		var t: float = (i - 2.0) / 2.0
		var x_pos := cw / 2.0 + t * card_spacing_x * 2.0
		var pos := Vector2(x_pos - CARD_PIVOT.x, base_y)
		var rot := t * half_span

		_cards[i].position = pos
		_cards[i].rotation = rot
		_card_base_positions[i] = pos
		_card_base_rotations[i] = rot
		_card_base_z[i] = i
		_cards[i].z_index = i


# ═══════════════════════════════════════════════════════════════
# 信号连接
# ═══════════════════════════════════════════════════════════════

func _connect_signals() -> void:
	for i in range(min(_cards.size(), CARD_COUNT)):
		var idx: int = i
		var card: Control = _cards[i]
		card.mouse_entered.connect(_on_card_mouse_entered.bind(idx))
		card.mouse_exited.connect(_on_card_mouse_exited.bind(idx))
		card.gui_input.connect(_on_card_gui_input.bind(idx))


# ═══════════════════════════════════════════════════════════════
# 鼠标交互
# ═══════════════════════════════════════════════════════════════

func _bounds_ok(idx: int) -> bool:
	return idx >= 0 and idx < _cards.size() and idx < CARD_COUNT


func _on_card_mouse_entered(idx: int) -> void:
	if not _bounds_ok(idx) or idx == _hovered_index:
		return

	_enter_gen[idx] += 1
	var my_gen: int = _enter_gen[idx]

	if _hovered_index != -1:
		_restore_card(_hovered_index)

	_hovered_index = idx

	var timer := get_tree().create_timer(enter_debounce_ms / 1000.0)
	timer.timeout.connect(_on_enter_confirmed.bind(idx, my_gen))


func _on_enter_confirmed(idx: int, gen: int) -> void:
	if not _bounds_ok(idx):
		return
	if _hovered_index != idx or _enter_gen[idx] != gen:
		return

	var card: Control = _cards[idx]
	var tween: Tween = _card_tweens[idx] as Tween
	if tween and tween.is_valid():
		tween.kill()

	tween = card.create_tween()
	tween.set_parallel(true)
	tween.set_ease(Tween.EASE_OUT)
	tween.set_trans(Tween.TRANS_BACK)
	tween.tween_property(card, "position", _card_base_positions[idx] + Vector2(0, hover_lift), hover_duration)
	tween.tween_property(card, "scale", Vector2.ONE * hover_scale, hover_duration)
	tween.tween_property(card, "rotation_degrees", 0.0, hover_duration)
	card.z_index = hover_z_index
	_card_tweens[idx] = tween


func _on_card_mouse_exited(idx: int) -> void:
	if not _bounds_ok(idx) or _hovered_index != idx:
		return

	var my_gen: int = _enter_gen[idx]

	var timer := get_tree().create_timer(exit_debounce_ms / 1000.0)
	timer.timeout.connect(_on_exit_confirmed.bind(idx, my_gen))


func _on_exit_confirmed(idx: int, gen: int) -> void:
	if not _bounds_ok(idx):
		return
	if _enter_gen[idx] != gen:
		return
	if _hovered_index != idx:
		return

	_hovered_index = -1
	_restore_card(idx)


func _restore_card(idx: int) -> void:
	if not _bounds_ok(idx):
		return

	var card: Control = _cards[idx]
	var tween: Tween = _card_tweens[idx] as Tween
	if tween and tween.is_valid():
		tween.kill()

	tween = card.create_tween()
	tween.set_parallel(true)
	tween.set_ease(Tween.EASE_OUT)
	tween.set_trans(Tween.TRANS_BACK)
	tween.tween_property(card, "position", _card_base_positions[idx], hover_duration)
	tween.tween_property(card, "scale", Vector2.ONE, hover_duration)
	tween.tween_property(card, "rotation", _card_base_rotations[idx], hover_duration)
	card.z_index = _card_base_z[idx]
	_card_tweens[idx] = tween


# ═══════════════════════════════════════════════════════════════
# 点击处理
# ═══════════════════════════════════════════════════════════════

func _on_card_gui_input(event: InputEvent, idx: int) -> void:
	if not _bounds_ok(idx):
		return
	if event is InputEventMouseButton and event.pressed and event.button_index == MOUSE_BUTTON_LEFT:
		accept_event()
		card_selected.emit(idx)