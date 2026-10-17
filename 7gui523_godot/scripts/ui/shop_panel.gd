extends Control
class_name ShopPanel

## ═══════════════════════════════════════════════════════════════
## ShopPanel — 商店面板（独立场景，编辑器直拖布局）
## 自带全屏遮罩 → 居中面板 → 可滚动商品列表。
## 渲染/购买/刷新逻辑全部委托给外层 play_ui，本层只管 UI。
## ═══════════════════════════════════════════════════════════════

signal item_buy_requested(item_type: int)
signal enchant_buy_requested(index: int)
signal refresh_requested
signal close_requested
signal next_level_requested

@onready var _chips_lbl: Label = %ChipsLabel
@onready var _fixed_box: VBoxContainer = %FixedItems
@onready var _refresh_box: VBoxContainer = %RefreshItems
@onready var _enchant_box: VBoxContainer = %EnchantItems
@onready var _refresh_btn: Button = %RefreshBtn
@onready var _enchant_header: Label = %EnchantHeader
@onready var _refresh_sep: Label = %RefreshSep
@onready var _refresh_cost_lbl: Label = %RefreshCostLabel
@onready var _panel_bg: PanelContainer = $Center/Panel
@onready var _close_btn: Button = %CloseBtn
@onready var _close_bottom_btn: Button = $Center/Panel/VBox/BottomRow/CloseBottomBtn
@onready var _next_btn: Button = %NextLevelBtn

var _chips: int = 0
var _disabled := false


func _ready() -> void:
	_close_btn.pressed.connect(func(): close_requested.emit())
	_close_bottom_btn.pressed.connect(func(): close_requested.emit())
	_refresh_btn.pressed.connect(func(): refresh_requested.emit())
	_next_btn.pressed.connect(func(): next_level_requested.emit())
	_apply_theme()
	_adjust_for_mobile()


func _apply_theme() -> void:
	var gold := Color(1.0, 0.84, 0.42)
	var dark := Color(0.25, 0.18, 0.08)
	var crimson := Color(0.35, 0.10, 0.08)

	_panel_bg.add_theme_stylebox_override("panel", _flat_style(
		Color(0.12, 0.06, 0.04, 0.98), gold, 2, 12,
		Color(0, 0, 0, 0.5), 24, 8))

	_style_btn(_refresh_btn, dark, gold)
	_style_btn(_close_btn, dark, gold)
	_style_btn(_close_bottom_btn, Color(0.30, 0.12, 0.10), Color(0.70, 0.52, 0.18))
	_style_btn(_next_btn, Color(0.90, 0.24, 0.30), gold)

	_style_header(%FixedHeader, gold)
	_style_header(_enchant_header, gold)
	_style_header(_refresh_sep, Color(0.55, 0.5, 0.45))
	_style_header(%TitleLabel, gold)

	_chips_lbl.add_theme_color_override("font_color", Color(1.0, 0.9, 0.6))
	_refresh_cost_lbl.add_theme_color_override("font_color", Color(0.75, 0.7, 0.6))


func _adjust_for_mobile() -> void:
	var vs := get_viewport().get_visible_rect().size
	if vs.x >= 768:
		return
	_panel_bg.custom_minimum_size = Vector2(vs.x * 0.92, vs.y * 0.82)
	%TitleLabel.add_theme_font_size_override("font_size", 15)
	_chips_lbl.add_theme_font_size_override("font_size", 13)
	for lbl in [_enchant_header, %FixedHeader]:
		lbl.add_theme_font_size_override("font_size", 11)


## ═══════════════════════════════════════════════════════════════
## 公开方法
## ═══════════════════════════════════════════════════════════════

func populate(fixed: Array[Shop.ShopItem], refreshable: Array[Shop.ShopItem],
		enchant_items: Array, chips: int, enchant_unlocked: bool, level_spent: int) -> void:
	_chips = chips
	_chips_lbl.text = "💰 %d" % chips

	_clear(_fixed_box)
	_clear(_refresh_box)
	_clear(_enchant_box)

	_refresh_sep.visible = true
	_refresh_cost_lbl.visible = true
	_refresh_btn.visible = true

	for it in fixed:
		_fixed_box.add_child(_make_row(it, chips))

	if refreshable.is_empty():
		_refresh_sep.visible = false
		_refresh_cost_lbl.visible = false
		_refresh_btn.visible = false

	for it in refreshable:
		_refresh_box.add_child(_make_row(it, chips))

	if not enchant_unlocked:
		var lock := Label.new()
		lock.text = "🔒 本关已消费 %d / 32 筹码，达成后解锁" % level_spent
		lock.add_theme_color_override("font_color", Color(0.6, 0.55, 0.5))
		lock.add_theme_font_size_override("font_size", 12)
		_enchant_box.add_child(lock)
	else:
		for i in enchant_items.size():
			_enchant_box.add_child(_make_enchant_row(enchant_items[i], i, chips))

	_refresh_btn.disabled = (_disabled or refreshable.is_empty())


func set_disabled(v: bool) -> void:
	_disabled = v
	_refresh_btn.disabled = v


## ═══════════════════════════════════════════════════════════════
## 行构建
## ═══════════════════════════════════════════════════════════════

func _make_row(it: Shop.ShopItem, chips: int) -> Control:
	var row := HBoxContainer.new()
	row.add_theme_constant_override("separation", 8)
	row.custom_minimum_size = Vector2(0, 36)

	var name_lbl := Label.new()
	name_lbl.text = it.name
	name_lbl.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	name_lbl.add_theme_color_override("font_color", Color(1, 0.96, 0.85))
	row.add_child(name_lbl)

	var desc_lbl := Label.new()
	desc_lbl.text = it.desc
	desc_lbl.add_theme_color_override("font_color", Color(0.7, 0.65, 0.55))
	desc_lbl.add_theme_font_size_override("font_size", 11)
	desc_lbl.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	row.add_child(desc_lbl)

	var stock_lbl := Label.new()
	stock_lbl.text = "∞" if it.stock < 0 else "×%d" % it.stock
	stock_lbl.add_theme_color_override("font_color", Color(0.8, 0.75, 0.65))
	stock_lbl.custom_minimum_size = Vector2(32, 0)
	row.add_child(stock_lbl)

	var btn := Button.new()
	btn.text = "💰%d" % it.price
	btn.disabled = (it.stock == 0) or (chips < it.price)
	_style_btn(btn, Color(0.25, 0.45, 0.25), Color(1.0, 0.84, 0.42))
	var t := it.type
	btn.pressed.connect(func(): item_buy_requested.emit(t))
	row.add_child(btn)

	return row


func _make_enchant_row(ei, index: int, chips: int) -> Control:
	var row := HBoxContainer.new()
	row.add_theme_constant_override("separation", 8)
	row.custom_minimum_size = Vector2(0, 36)

	var name_lbl := Label.new()
	name_lbl.text = ei.display_name()
	name_lbl.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	name_lbl.add_theme_color_override("font_color", Color(1, 0.9, 0.7))
	row.add_child(name_lbl)

	var desc_lbl := Label.new()
	desc_lbl.text = ei.enchant.desc
	desc_lbl.add_theme_color_override("font_color", Color(0.7, 0.65, 0.55))
	desc_lbl.add_theme_font_size_override("font_size", 11)
	desc_lbl.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	row.add_child(desc_lbl)

	var btn := Button.new()
	btn.text = ei.display_price()
	btn.disabled = ei.sold_out or chips < ei.enchant.cost_chips
	_style_btn(btn, Color(0.35, 0.20, 0.45), Color(1.0, 0.84, 0.42))
	var idx := index
	btn.pressed.connect(func(): enchant_buy_requested.emit(idx))
	row.add_child(btn)

	return row


## ═══════════════════════════════════════════════════════════════
## 样式工具
## ═══════════════════════════════════════════════════════════════

func _style_btn(btn: Button, bg: Color, border: Color) -> void:
	var gold_glow := Color(border.r * 0.7, border.g * 0.55, border.b * 0.2, 0.55)
	btn.add_theme_stylebox_override("normal", _flat_style(bg, border, 2, 9, gold_glow, 8, 3))
	btn.add_theme_stylebox_override("hover", _flat_style(bg.lightened(0.12), border.lightened(0.15), 2, 9, gold_glow, 16, 4))
	btn.add_theme_stylebox_override("pressed", _flat_style(bg.darkened(0.18), border.darkened(0.25), 2, 9, Color(0, 0, 0, 0.4), 6, 2))
	btn.add_theme_stylebox_override("disabled", _flat_style(Color(0.24, 0.18, 0.16), Color(0.30, 0.24, 0.18), 2, 9))
	btn.add_theme_color_override("font_color", Color(1, 0.96, 0.85))
	btn.add_theme_color_override("font_hover_color", Color(1, 1, 0.92))
	btn.add_theme_color_override("font_pressed_color", Color(1, 0.82, 0.62))
	btn.add_theme_color_override("font_disabled_color", Color(0.55, 0.45, 0.35))


func _flat_style(bg_c: Color, border_c: Color, bw: int = 2, radius: int = 6,
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


func _style_header(lbl: Label, c: Color) -> void:
	lbl.add_theme_color_override("font_color", c)
	lbl.add_theme_font_size_override("font_size", 13)


func _clear(node: Control) -> void:
	for child in node.get_children():
		child.queue_free()