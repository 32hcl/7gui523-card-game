extends Control
class_name PlayBase

## — 第 1 层 play_base：常量 · 节点引用 · 基础视觉工具
##
## 底层的全部 @onready 节点、状态、常量、主题、牌堆构建、手牌卡构建与视觉工具。
## 不依赖动画层或商店层。

const CARD_W := 64
const CARD_H := 90
const HAND_W := 82   ## 手牌卡显示尺寸（与桌面牌统一）
const HAND_H := 116

const BOSS_CARD_W := 72
const BOSS_CARD_H := 100
const TABLE_CARD_W := 88  ## 桌面出牌区（比手牌大，保证台上清晰可见）
const TABLE_CARD_H := 124
const SELECTED_LIFT := 30.0
const SELECTED_SCALE := 1.12
const INSTANT_ROW_GAP := 46.0  ## 上下排净距：覆盖选中上浮+缩放，保证上排底边不被遮挡
const INSTANT_TINT := Color(0.72, 0.82, 1.0, 1.0)  ## 立刻补区淡蓝色调
const DEAL_DELAY := 0.07
const DEAL_DURATION := 0.32
const INITIAL_DEAL_DELAY := 0.13
const INITIAL_DEAL_DURATION := 0.58
const ROUND_DAMAGE_DELAY := 0.90
const PLAYER_DISCARD_LIMIT := 10
const DISCARD_CHIP_COST := 3      ## 弃牌代价：每张 3 筹码（两条弃牌入口统一）
const INVALID_PLAY_PENALTY := 2   ## 非法出牌扣 2 筹码（回合内允许扣成负数）
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
const DRAG_RETURN_TAIL := 0.05           ## 飞回动画后额外停留
const INVALID_SHAKE_FLY := 0.22          ## 非法出牌飞到桌面停留
const INVALID_SHAKE_DURATION := 0.45     ## 非法出牌桌面晃动时长
const TABLE_ANIM_TAIL := 0.02            ## 桌牌动画额外余量
const AI_THINK_DELAY := 0.7              ## AI思考到实际执行的间隔
const HINT_BG_ALPHA := 0.10              ## 拖拽/弃牌提示背景透明度
const HINT_BORDER_ALPHA := 0.65          ## 出牌落点提示边框透明度
const HINT_DISCARD_BORDER_ALPHA := 0.75  ## 弃牌提示边框透明度
const FAN_EXPAND_FACTOR := 0.15         ## 牌堆点击判定扩展系数

const _TILT_MAX := 15.0   ## 悬停伪3D 倾斜上限（对齐 Demo card_UI.tscn angle_x_max/angle_y_max）

## 手牌扇形（对齐 Demo hand_pos_y_curve / hand_rot_z_curve）
const HAND_ARC_RATIO := 0.1      ## 弧高系数：中间比两端高 2*HAND_ARC_RATIO*牌高
const HAND_FAN_ROT_MAX := 15.0   ## 左右端 z 旋转（度），中间 0

## 悬停 Q 弹（Demo to_scale 默认 TRANS_ELASTIC，此处保留弹性但缩短时长）
const HOVER_SCALE := 1.15        ## 悬停放大倍数（相对当前布局基准缩放）
const HOVER_SCALE_DUR := 0.40    ## 弹性缩放时长

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
const TABLET_MAX_WIDTH := 1024.0          ## 平板最大宽度，超出即为桌面
enum DeviceClass { DESKTOP, TABLET, PHONE }
var _device_class := DeviceClass.DESKTOP

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
@onready var _chips_player_lbl: Label = $MainVBox/TopBar/PlayerChips
@onready var _chips_boss_lbl: Label = $MainVBox/TopBar/BossChips

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

## 手牌区（双 HBoxContainer：上排补牌 / 下排主力）
@onready var _hand_area: Panel = $MainVBox/HandArea
@onready var _top_hand: HBoxContainer = $MainVBox/HandArea/HandVBox/TopHand
@onready var _bottom_hand: HBoxContainer = $MainVBox/HandArea/HandVBox/BottomHand

## GameOver
@onready var _game_over_layer: Control = $GameOver
@onready var _game_over_msg: Label = $GameOver/Center/Box/V/Message

## 游戏状态容器：所有游戏运行时状态集中于此。
## play.gd _ready() 中赋值 _state = GameState.new()
## 访问方式：_state.round, _state.human, _state.phase 等
var _state: GameState

## 手牌随机抖动（拟真）：按 card_id 稳定存储旋转角与垂直偏移
var _hand_jitter: Dictionary = {}

## 拖拽出牌
var _dragging: bool = false
## 拖拽牌卡的槽位快照：记录拖拽前的位置、旋转、缩放、层级与父节点，
## 用于合法出牌失败时的飞回动画或拖拽取消时原样复归。
class DragOrigin extends RefCounted:
	var parent: Node = null
	var pos: Vector2 = Vector2.ZERO
	var rot: float = 0.0
	var scale: Vector2 = Vector2.ONE
	var z: int = 0


var _drag_press_pos := Vector2.ZERO
var _drag_first_index := -1
var _drag_visuals: Array[CardVisual] = []
var _drag_indices: Array[int] = []
var _drag_origin: Array[DragOrigin] = []
var _drop_hint: Panel = null
var _discard_hint: Panel = null

## 牌堆显示：玩家/BOSS 牌堆叠置牌背 + 剩余张数角标（也是发牌/抽卡动画的起始位）
var _player_deck_lbl: Label = null
var _boss_deck_lbl: Label = null
## 牌堆是否已播过"抽空溶解"，避免每帧刷新重复触发
var _player_deck_empty := false
var _boss_deck_empty := false

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
## 非法出牌动画进行中（期间拒绝新的拖拽/出牌）
var _invalid_anim_running := false
## 最近一次出牌被拒的原因（_do_play 写入，_drag_drop_play 消费）
var _invalid_reason := ""
## 当前已渲染桌面牌的签名（未变化时 _refresh_table 跳过溶解重建）
var _table_render_sig := "@init"
var _dream_materials: Array[ShaderMaterial] = []
var _dream_time: float = 0.0

## — 设置 —
var _pixelate_enabled: bool = false      ## 桌面特效像素化开关（默认关闭）
var _card_sort_mode: int = Player.CardSortMode.SAME_RANK  ## 补牌排序模式
var _settings_dialog: RefCounted = null                 ## 设置弹窗引用（防 GC）
var _audio: AudioManager = null                         ## 音效管理器（_ready 中创建）

## — 训练模式 —
var _training := TrainingSession.new()


## 检测并应用设备适配布局（手机/平板/桌面三档）
func setup_responsive() -> void:
	var vs := get_viewport().get_visible_rect().size
	var short_side := minf(vs.x, vs.y)
	var aspect := vs.x / maxf(vs.y, 1.0)
	if short_side < MOBILE_MAX_WIDTH and aspect < 1.5:
		_device_class = DeviceClass.PHONE
	elif short_side < TABLET_MAX_WIDTH or (vs.x < TABLET_MAX_WIDTH and aspect < 1.5):
		_device_class = DeviceClass.TABLET
	else:
		_device_class = DeviceClass.DESKTOP
	var compact := _device_class != DeviceClass.DESKTOP
	_calc_mobile_card_sizes(vs, compact)
	## 移动端手牌宽度公式允许牌槽重叠；同步收紧 HBox 间距，避免 5 张牌溢出。
	## 桌面端保留场景中的 14px 间距。
	var hand_gap := -_mobile_hand_w * 0.15 if compact else 14.0
	_top_hand.add_theme_constant_override("separation", roundi(hand_gap))
	_bottom_hand.add_theme_constant_override("separation", roundi(hand_gap))
	## 手牌两行容器高度跟随卡牌尺寸，保证 BottomHand 完全在可视范围内
	var card_h: float = _mobile_hand_h if compact else float(HAND_H)
	_top_hand.custom_minimum_size = Vector2(0, card_h)
	_bottom_hand.custom_minimum_size = Vector2(0, card_h)
	## 手牌区高度 = HandVBox 顶部留白(50) + 两行卡高 + 行距 + 底部留白(4)
	_hand_area.custom_minimum_size = Vector2(0, card_h * 2.0 + INSTANT_ROW_GAP + 54.0)


func _calc_mobile_card_sizes(vs: Vector2, compact: bool) -> void:
	if not compact:
		return
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
	_hand_area.add_theme_stylebox_override("panel", flat_border(Color(0.14, 0.13, 0.15, 0.52), panel_border, 1, 8))
	## 桌面绿毡边框保留，填充透明让涡旋 shader 透出
	$MainVBox/TableArea.add_theme_stylebox_override(
		"panel", flat_border(Color.TRANSPARENT, COL_GREEN.darkened(0.30), 1, 8))

	## 信息层级：计分板金色强调；玩家筹码绿色、BOSS 筹码红色
	_deck_count_lbl.add_theme_color_override("font_color", COL_TEXT)
	_table_score_lbl.add_theme_color_override("font_color", COL_GOLD)
	_chips_player_lbl.add_theme_color_override("font_color", COL_GREEN.lightened(0.25))
	_chips_boss_lbl.add_theme_color_override("font_color", COL_CRIMSON.lightened(0.15))

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
	if container == _player_deck:
		_player_deck_empty = false
	elif container == _boss_deck:
		_boss_deck_empty = false
	var dw := deck_size.x
	var dh := deck_size.y
	var csize := container.size
	if csize.x < 1.0:
		csize = container.custom_minimum_size
	var center := (csize - Vector2(dw, dh)) * 0.5

	var backs: Array = []
	for i in 3:
		var back := CardVisual.new(Vector2(dw, dh))
		back.show_back()
		back.enable_fake3d(false)
		back.mouse_filter = Control.MOUSE_FILTER_IGNORE
		back.position = center + Vector2(i * 2.0, i * 2.0)
		back.pivot_offset = Vector2(dw, dh) * 0.5
		back.set_meta("_home_y", back.position.y)
		back.z_index = i
		container.add_child(back)
		backs.append(back)
	container.set_meta("_backs", backs)

	var lbl := Label.new()
	lbl.text = "牌堆\n0"
	lbl.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	lbl.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	lbl.add_theme_color_override("font_color", COL_GOLD)
	lbl.add_theme_color_override("font_outline_color", Color(0.0, 0.0, 0.0, 0.85))
	lbl.add_theme_constant_override("outline_size", 4)
	lbl.mouse_filter = Control.MOUSE_FILTER_IGNORE
	lbl.z_index = 10
	lbl.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	container.add_child(lbl)
	return lbl


## 洗牌动画（对齐 Demo card_pile.shuffle：逐张抬起放大，再 CIRC 落回）
## 延迟错开让三张牌背呈现"叠洗"层次
func _play_deck_shuffle() -> void:
	for pair in [[_player_deck, 0.0], [_boss_deck, 0.15]]:
		var container: Control = pair[0]
		var base_delay: float = pair[1]
		var backs: Array = container.get_meta("_backs", [])
		for i in backs.size():
			var back: CardVisual = backs[i]
			if not is_instance_valid(back):
				continue
			var home_y: float = float(back.get_meta("_home_y", back.position.y))
			var d := 0.22
			var tw := back.create_tween()
			tw.tween_interval(base_delay + float(i) * 0.05)
			tw.set_parallel(true)
			tw.tween_property(back, "position:y", home_y - 26.0, d) \
				.set_ease(Tween.EASE_OUT).set_trans(Tween.TRANS_CUBIC)
			tw.tween_property(back, "scale", Vector2(1.18, 1.18), d) \
				.set_ease(Tween.EASE_OUT).set_trans(Tween.TRANS_CUBIC)
			tw.chain().set_parallel(true)
			tw.tween_property(back, "position:y", home_y, d) \
				.set_ease(Tween.EASE_IN).set_trans(Tween.TRANS_CIRC)
			tw.tween_property(back, "scale", Vector2.ONE, d) \
				.set_ease(Tween.EASE_IN).set_trans(Tween.TRANS_CIRC)


## 牌堆抽空：整套牌背燃烧溶解（对齐 Demo card_pile.delete_all_cards）
## 只做"空→溶解"单向，非空时由 _build_deck_visual 重建
func _update_deck_empty(container: Control, is_empty: bool) -> void:
	var already: bool = _player_deck_empty if container == _player_deck else _boss_deck_empty
	if not is_empty or already:
		return
	if container == _player_deck:
		_player_deck_empty = true
	elif container == _boss_deck:
		_boss_deck_empty = true
	else:
		return
	var backs: Array = container.get_meta("_backs", [])
	for i in backs.size():
		var back: CardVisual = backs[i]
		if is_instance_valid(back):
			back.dissolve(0.45 + 0.10 * float(i))


func _setup_ambiance() -> void:
	## 全局暗底（兜底，BOSS/手牌区深面板下方可见）
	var bg := ColorRect.new()
	bg.color = COL_BG
	bg.mouse_filter = Control.MOUSE_FILTER_IGNORE
	bg.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	bg.z_index = -30
	add_child(bg)
	move_child(bg, 0)

	## 单一全屏梦幻背景：覆盖 BOSS 区、桌面区、牌堆区和手牌区。
	var vortex := _make_vortex_bg()
	vortex.z_index = -20
	add_child(vortex)
	move_child(vortex, 0)

	## 手牌区只增加半透明蒙版，保持全屏背景连续。
	var hand_mask := ColorRect.new()
	hand_mask.color = Color(0.03, 0.04, 0.08, 0.50)
	hand_mask.mouse_filter = Control.MOUSE_FILTER_IGNORE
	hand_mask.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	_hand_area.add_child(hand_mask)
	_hand_area.move_child(hand_mask, 1)


## 涡旋背景 ColorRect（带 shader 材质）
func _make_vortex_bg() -> ColorRect:
	var rect := ColorRect.new()
	rect.color = COL_BG
	rect.mouse_filter = Control.MOUSE_FILTER_IGNORE
	rect.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	var mat := ShaderMaterial.new()
	mat.shader = load("res://shaders/vortex_bg.gdshader")
	mat.set_shader_parameter("time_val", 0.0)
	mat.set_shader_parameter("spin_time", 0.0)
	_dream_materials.append(mat)
	rect.material = mat
	## 创建时立即按当前设置写入像素化参数，避免开局前的首帧沿用 shader 默认值
	_apply_pixelation()
	return rect


func _update_dream_speed() -> void:
	if _state.human == null:
		return
	## 负筹码越多（负债越深），背景涡旋越快、对比越强
	var debt := maxi(0, -_state.human.chips)
	var danger := clampf(float(debt) / 500.0, 0.0, 1.0)
	var sp := 0.25 + danger * 0.8
	var ct := 3.5 + danger * 2.5
	for mat in _dream_materials:
		if is_instance_valid(mat):
			mat.set_shader_parameter("spin_amount", sp)
			mat.set_shader_parameter("contrast", ct)

## 应用像素化设置到所有涡旋 shader
func _apply_pixelation() -> void:
	var psf: float = 180.0 if _pixelate_enabled else 1200.0
	var qm: float = 0.7 if _pixelate_enabled else 0.0
	for mat in _dream_materials:
		if is_instance_valid(mat):
			mat.set_shader_parameter("pixel_size_fac", psf)
			mat.set_shader_parameter("quantize_mix", qm)

## 公共接口：开关像素化效果（供 SettingsDialog 调用）
func set_pixelation(enabled: bool) -> void:
	_pixelate_enabled = enabled
	_apply_pixelation()

## 公共接口：读取像素化开关状态
func is_pixelation_enabled() -> bool:
	return _pixelate_enabled


func _hand_area_center() -> Vector2:
	return _bottom_hand.global_position + _bottom_hand.size * 0.5


## 按“稳定手牌索引”取牌：普通区在前、立刻补区在后（与点击/拖拽索引口径一致）
func _hand_card_at(index: int) -> Card:
	if index < 0:
		return null
	if index < _state.human.normal_hand.size():
		return _state.human.normal_hand[index]
	var ji := index - _state.human.normal_hand.size()
	return _state.human.instant_hand[ji] if ji < _state.human.instant_hand.size() else null


## 清空容器：先脱离父节点再释放，避免布局与动画残留
func _clear_children(container: Node) -> void:
	for child in container.get_children():
		container.remove_child(child)
		child.queue_free()


func _boss_area_center() -> Vector2:
	return _boss_hand.global_position + _boss_hand.size * 0.5


## 根据 AI 实际打出的牌属于哪个手牌区（normal/instant），选对的 HBox 中心
func _boss_src_center(cards: Array[Card]) -> Vector2:
	if cards.is_empty():
		return _boss_area_center()
	var first_card: Card = cards[0]
	var found_instant := _boss_hand_instant.get_child_count() > 0 and first_card in _state.ai.instant_hand
	var src_row: HBoxContainer = _boss_hand_instant if found_instant else _boss_hand
	return src_row.global_position + src_row.size * 0.5


func _face(card: Card) -> Control:
	var table_size := Vector2(TABLE_CARD_W, TABLE_CARD_H)
	if _device_class != DeviceClass.DESKTOP:
		var visual := CardVisual.new(Vector2(
			minf(_mobile_card_w * 1.12, TABLE_CARD_W),
			minf(_mobile_card_h * 1.12, TABLE_CARD_H)))
		visual.show_texture(CardVisual.texture_for_id(card.id))
		visual.mouse_filter = Control.MOUSE_FILTER_IGNORE
		visual.size_flags_horizontal = Control.SIZE_SHRINK_CENTER
		visual.size_flags_vertical = Control.SIZE_SHRINK_CENTER
		return visual
	var visual := CardVisual.new(table_size)
	visual.show_texture(CardVisual.texture_for_id(card.id))
	visual.mouse_filter = Control.MOUSE_FILTER_IGNORE
	visual.size_flags_horizontal = Control.SIZE_SHRINK_CENTER
	visual.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	return visual


## 手牌卡（点击选中 / 假3D 悬停 / 拖拽）
func _hand_card(card: Card, index: int, selected: bool, card_size: Vector2 = Vector2.ZERO) -> CardVisual:
	var w: float = card_size.x if card_size.x > 0 else (_mobile_hand_w if _device_class != DeviceClass.DESKTOP else HAND_W)
	var h: float = card_size.y if card_size.y > 0 else (_mobile_hand_h if _device_class != DeviceClass.DESKTOP else HAND_H)
	var visual := CardVisual.new(Vector2(w, h))
	visual.pivot_offset = Vector2(w * 0.5, h * 0.5)
	visual.show_texture(CardVisual.texture_for_id(card.id))
	visual.enable_fake3d(true)
	visual.tooltip_text = "%s%s" % [card.point, (" " + card.suit) if not card.suit.is_empty() else ""]
	## z_index / base_z / scale / selected 由 _render_hand_block 统一写入，不在此处预设
	visual.mouse_entered.connect(func():
		visual.set_meta("_hovered", true)
		visual.z_index = visual.base_z + 50   ## 重叠手牌悬停时压到相邻牌之上
		var base: Vector2 = visual.get_meta("_base_scale", Vector2.ONE)
		_hover_scale_to(visual, base * HOVER_SCALE)
		visual.modulate = Color(1.08, 1.05, 0.95, 1.0))
	visual.mouse_exited.connect(func():
		visual.set_meta("_hovered", false)
		visual.set_meta("_tilt_target", Vector2.ZERO)
		visual.z_index = visual.base_z
		_hover_scale_to(visual, visual.get_meta("_base_scale", Vector2.ONE))
		visual.to_rot(0.0, 0.0)
		## 恢复渲染时写入的基准色调（全伤害卡的淡蓝 INSTANT_TINT 等），而非硬重置白
		visual.modulate = visual.get_meta("base_modulate", Color(1.0, 1.0, 1.0, 1.0)))
	visual.gui_input.connect(func(event):
		_on_gui(visual.get_meta("hand_index", index), event, visual))
	return visual


## 悬停弹性缩放：弹性缓动制造回弹（Demo 悬停 to_scale(1.2) 的手感来源）
func _hover_scale_to(v: CardVisual, target: Vector2) -> void:
	var old: Variant = v.get_meta("_hover_tween", false)
	if old is Tween and (old as Tween).is_valid():
		(old as Tween).kill()
	var tw := v.create_tween()
	tw.set_ease(Tween.EASE_OUT).set_trans(Tween.TRANS_ELASTIC)
	tw.tween_property(v, "scale", target, HOVER_SCALE_DUR)
	v.set_meta("_hover_tween", tw)


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
	visual.to_rot(target.x, target.y, 0.15, Tween.TRANS_BACK, Tween.EASE_OUT)


func _on_gui(index: int, event: InputEvent, visual: CardVisual) -> void:
	if event is InputEventMouseMotion:
		if _drag_first_index < 0 and not _dragging:
			_tilt_visual(visual)
	elif event is InputEventMouseButton and event.pressed \
			and event.button_index == MOUSE_BUTTON_LEFT and not _dragging and _drag_first_index < 0:
		if _state.finished or _invalid_anim_running or not is_instance_valid(_state.round) or _state.round.whose_turn() != _state.human:
			return
		## 与 3D tilt 共用内圈判断：外层 12% 不响应点击
		var local: Vector2 = visual.get_local_mouse_position()
		var sx: float = visual.size.x
		var sy: float = visual.size.y
		if local.x < sx * 0.12 or local.x > sx * 0.88 or local.y < sy * 0.12 or local.y > sy * 0.88:
			return
		_drag_press_pos = get_global_mouse_position()
		_drag_first_index = index


## 面板/按钮样式统一由 UIStyle 实现，此处仅保留薄委托以维持既有调用点
func flat_border(bg_c: Color, border_c: Color, bw: int = 2, radius: int = 6,
		shadow_c: Color = Color(0, 0, 0, 0), shadow_size: int = 0,
		shadow_off: int = 0, content_top: int = 0) -> StyleBoxFlat:
	return UIStyle.flat_border(bg_c, border_c, bw, radius, shadow_c, shadow_size, shadow_off, content_top)


func style_button_balatro(btn: Button, bg: Color,
		text_col: Color = Color(0.95, 0.95, 0.93)) -> void:
	UIStyle.style_button(btn, bg, text_col)


func balatro_panel_style() -> StyleBoxFlat:
	return flat_border(Color(0.14, 0.13, 0.15, 0.98), Color(0.22, 0.21, 0.24), 1, 8, Color(0, 0, 0, 0.5), 12, 6)