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

const _TILT_MAX := 5.0

## 手机端自适应
const MOBILE_MAX_WIDTH := 540.0
var _is_mobile := false

var _mobile_card_w: float = 0.0
var _mobile_card_h: float = 0.0
var _mobile_hand_w: float = 0.0
var _mobile_hand_h: float = 0.0
var _mobile_boss_w: float = 0.0
var _mobile_boss_h: float = 0.0
var _mobile_gap: float = -12.0
var _mobile_instant_gap: float = 12.0
var _mobile_boss_gap: float = -18.0

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


## 检测并应用手机竖屏布局（桌面端也统一走此布局）
func setup_responsive() -> void:
	var vs := get_viewport().get_visible_rect().size
	_is_mobile = vs.x < MOBILE_MAX_WIDTH
	## 桌面端也走竖排 VBox 布局，让手牌吃到全屏宽度
	_calc_mobile_card_sizes(vs)
	_restructure_for_mobile(vs)


func _calc_mobile_card_sizes(vs: Vector2) -> void:
	## 5张主力牌铺满90%宽度，牌间有微小重叠
	var total := vs.x * 0.9
	_mobile_hand_w = total / 4.4
	_mobile_hand_h = _mobile_hand_w * 1.5
	_mobile_card_w = _mobile_hand_w * 0.72
	_mobile_card_h = _mobile_card_w * 1.4
	_mobile_boss_w = _mobile_hand_w * 0.62
	_mobile_boss_h = _mobile_boss_w * 1.4
	_mobile_gap = _mobile_hand_w * -0.16
	_mobile_instant_gap = _mobile_hand_w * 0.14
	_mobile_boss_gap = _mobile_boss_w * -0.35


func _restructure_for_mobile(vs: Vector2) -> void:
	## 拆掉横向 HBox → 竖向 VBox
	var root := $"."
	var old_layout := $"MainLayout"
	var left := $"MainLayout/LeftPanel"
	var right := $"MainLayout/RightPanel"

	old_layout.remove_child(left)
	old_layout.remove_child(right)	
	root.remove_child(old_layout)

	var vbox := VBoxContainer.new()
	vbox.name = "MainLayout"
	vbox.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	vbox.offset_left = 6.0
	vbox.offset_top = 4.0
	vbox.offset_right = -6.0
	vbox.offset_bottom = -4.0
	vbox.add_theme_constant_override("separation", 6)
	root.add_child(vbox)

	left.custom_minimum_size = Vector2(0, 0)
	left.size_flags_vertical = Control.SIZE_FILL | Control.SIZE_EXPAND
	left.size_flags_horizontal = Control.SIZE_FILL
	vbox.add_child(left)

	## 调整各区域最小高度匹配双层手牌尺寸
	var boss_zone := left.get_node("BossZone") as Control
	var player_zone := left.get_node("PlayerZone") as Control
	var table_zone := left.get_node("TableZone") as Control
	var two_layer_h: float = vs.y * 0.20 * 1.55 + 16.0
	if player_zone:
		player_zone.custom_minimum_size = Vector2(0, two_layer_h)
	if table_zone:
		table_zone.size_flags_vertical = Control.SIZE_FILL | Control.SIZE_EXPAND

	## ── Boss 区左右分区 ──
	## 左边: Boss 手牌（瓦片形）；右边: 3 个操作按钮
	var boss_row := boss_zone.get_node("BossRow") as Control
	var boss_deck := boss_row.get_node("BossDeck") as Control
	boss_row.remove_child(boss_deck)

	var boss_hbox := HBoxContainer.new()
	boss_hbox.name = "BossHBox"
	boss_hbox.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	boss_hbox.add_theme_constant_override("separation", 8)
	boss_zone.remove_child(boss_row)
	boss_row.size_flags_horizontal = Control.SIZE_FILL | Control.SIZE_EXPAND
	boss_hbox.add_child(boss_row)

	## Boss 区右侧 3 按钮
	var boss_btn_vbox := VBoxContainer.new()
	boss_btn_vbox.name = "BossActionBar"
	boss_btn_vbox.add_theme_constant_override("separation", 4)
	boss_btn_vbox.alignment = BoxContainer.ALIGNMENT_CENTER
	var btn_grid := right.get_node("RightVBox/ButtonGrid")
	btn_grid.remove_child(_pass_btn)
	btn_grid.remove_child(_shop_btn)
	btn_grid.remove_child(_new_game_btn)
	## 不要按钮在移动端隐藏，改用桌面双击触发
	_pass_btn.visible = false
	for b: Button in [_shop_btn, _new_game_btn]:
		b.custom_minimum_size = Vector2(44, 44)
		b.size_flags_horizontal = Control.SIZE_SHRINK_CENTER
		b.add_theme_font_size_override("font_size", 13)
		boss_btn_vbox.add_child(b)
	boss_hbox.add_child(boss_btn_vbox)
	boss_zone.add_child(boss_hbox)
	boss_zone.custom_minimum_size = Vector2(0, 60)

	## 隐藏不需要的按钮
	_play_btn.visible = false
	_hp_swap_btn.visible = false
	_return_btn.visible = false

	## ── 双方牌堆移到桌面（加到 root，PanelContainer 会覆盖子节点锚点） ──
	boss_deck.name = "BossDeckOnTable"
	boss_deck.z_index = 40
	boss_deck.mouse_filter = Control.MOUSE_FILTER_STOP
	root.add_child(boss_deck)

	var player_row := player_zone.get_node("PlayerRow") as Control
	var player_deck := player_row.get_node("PlayerDeck") as Control
	player_row.remove_child(player_deck)
	player_deck.name = "PlayerDeckOnTable"
	player_deck.z_index = 40
	player_deck.mouse_filter = Control.MOUSE_FILTER_STOP
	root.add_child(player_deck)

	## ── 日志 + HP 放在 Boss 和桌面之间 ──
	right.visible = false
	btn_grid.visible = false
	var log_title := right.get_node("RightVBox/LogTitle")
	log_title.visible = false

	var hp_label := right.get_node("RightVBox/HPLabel")
	right.get_node("RightVBox").remove_child(hp_label)
	hp_label.custom_minimum_size = Vector2(0, 18)
	hp_label.add_theme_font_size_override("font_size", 10)
	hp_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER

	var log_scroll := right.get_node("RightVBox/LogScroll") as ScrollContainer
	right.get_node("RightVBox").remove_child(log_scroll)
	log_scroll.custom_minimum_size = Vector2(0, 28)
	log_scroll.size_flags_horizontal = Control.SIZE_FILL | Control.SIZE_EXPAND
	log_scroll.size_flags_vertical = Control.SIZE_FILL

	var log_bar := VBoxContainer.new()
	log_bar.name = "LogBar"
	log_bar.add_theme_constant_override("separation", 2)
	log_bar.add_child(hp_label)
	log_bar.add_child(log_scroll)

	var table_idx := table_zone.get_index()
	left.add_child(log_bar)
	left.move_child(log_bar, table_idx)

	old_layout.queue_free()


func _setup_ui_theme() -> void:
	var accent := Color(0.20, 0.38, 0.58)
	var accent_dim := Color(0.15, 0.28, 0.42)
	var crimson := Color(0.90, 0.24, 0.30)
	var crimson_deep := Color(0.62, 0.10, 0.16)
	var green := Color(0.22, 0.55, 0.32)
	var panel := Color(0.14, 0.13, 0.15, 0.98)

	style_button_balatro(_pass_btn, crimson, Color(0.95, 0.90, 0.88))
	style_button_balatro($GameOver/Center/Box/V/AgainBtn, crimson, Color(0.95, 0.90, 0.88))
	style_button_balatro(_new_game_btn, Color(0.18, 0.17, 0.19), Color(0.88, 0.86, 0.83))
	style_button_balatro(_shop_btn, accent, Color(0.95, 0.95, 0.93))

	var text_c := Color(0.90, 0.88, 0.85)
	_title_lbl.add_theme_color_override("font_color", text_c)
	_round_lbl.add_theme_color_override("font_color", text_c.lightened(0.08))
	for lbl: Label in [_title_lbl, _round_lbl]:
		lbl.add_theme_color_override("font_outline_color", Color(0.0, 0.0, 0.0, 0.5))
		lbl.add_theme_constant_override("outline_size", 4)
		lbl.add_theme_color_override("font_shadow_color", Color(0, 0, 0, 0.5))
		lbl.add_theme_constant_override("shadow_offset_x", 2)
		lbl.add_theme_constant_override("shadow_offset_y", 2)

	$GameOver/Center/Box.add_theme_stylebox_override("panel", balatro_panel_style())

	_setup_decor()
	_setup_ambiance()


func _setup_deck_display() -> void:
	## 布局已稳定，用 table_zone 实际区域计算牌堆绝对坐标
	var tz: Control = $MainLayout/LeftPanel/TableZone as Control
	if tz and tz.size.x > 1:
		var tz_pos := tz.global_position - global_position
		_boss_deck.position = tz_pos + Vector2(4, 4)
		_boss_deck.size = Vector2(56, 80)
		## 玩家牌堆：尺寸与手牌一致，放在桌面右下角，向上移动避免挡住底部文字（原始分等）
		_player_deck.position = tz_pos + Vector2(tz.size.x - _mobile_hand_w - 8, tz.size.y - _mobile_hand_h - 46)
		_player_deck.size = Vector2(_mobile_hand_w, _mobile_hand_h)

	_player_deck_lbl = _build_deck_visual(_player_deck, Vector2(_mobile_hand_w * 0.72, _mobile_hand_h * 0.72))
	_boss_deck_lbl = _build_deck_visual(_boss_deck, Vector2(56, 80) if not _is_mobile else Vector2(_mobile_boss_w * 0.9, _mobile_boss_h * 0.9))


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
	var bg := ColorRect.new()
	bg.color = Color(0.08, 0.08, 0.10)
	bg.mouse_filter = Control.MOUSE_FILTER_IGNORE
	bg.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	add_child(bg)
	move_child(bg, 0)


func _setup_decor() -> void:
	pass


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


func _hand_metrics(n: int, m: int, normal_hand: Array = [], instant_hand: Array = []) -> Dictionary:
	## 排序感知式扇形布局：
	##   同点牌紧贴（OVERLAP_SAME_RANK）→ 视觉分组
	##   不同点牌间距大（OVERLAP_DIFF_RANK）→ 组间呼吸感
	##   弧线 y 坐标基于排序位置而非原始下标
	const OVERLAP_SAME_RANK := 0.45          ## 同点：每张露出 45%（重叠 55%）
	const OVERLAP_DIFF_RANK := 0.60          ## 不同点：每张露出 60%（重叠 40%）
	const ARC_PEAK := 10.0

	var card_h: float = HAND_H
	var card_w: float = card_h * (float(CARD_W) / float(CARD_H))
	var hand_w := _player_hand.size.x
	var hand_h := _player_hand.size.y
	var edge_pad := 8.0

	## ── 主力区：按排序后位置计算 x/y，填入 normal_xpos[原始下标] ──
	var normal_xpos: Array = []
	normal_xpos.resize(n)
	var normal_ypos: Array = []
	normal_ypos.resize(n)

	var normal_by_pos: Array = []      ## 排序后第 p 位 → 原始下标
	if n > 0:
		normal_by_pos = Card.sorted_order(normal_hand)

		var cur_x := 0.0
		for p in range(n):
			var ri: int = normal_by_pos[p]
			normal_xpos[ri] = cur_x
			var ty := 0.5 if n <= 1 else float(p) / float(n - 1)
			normal_ypos[ri] = hand_h - card_h - 8.0 - ARC_PEAK * 4.0 * ty * (1.0 - ty)
			if p < n - 1:
				var next_ri: int = normal_by_pos[p + 1]
				var same_rank := Card.get_rank(normal_hand[ri].point) == Card.get_rank(normal_hand[next_ri].point)
				cur_x += card_w * (OVERLAP_SAME_RANK if same_rank else OVERLAP_DIFF_RANK)

		var total_w := cur_x + card_w
		var offset := maxf((hand_w - total_w) * 0.5, edge_pad)
		for i in range(n):
			normal_xpos[i] += offset

	## ── 立刻补区 ──
	var igap: float = card_w * 0.30 if m > 1 else 0.0
	var instant_y: float = hand_h - card_h - 8.0 - card_h * 0.28

	## ── 合并排序 ──
	var normal_order: Array[int] = []
	var instant_positions: Array[float] = []
	var do_combined := not normal_hand.is_empty() and not instant_hand.is_empty() and n > 0 and m > 0

	if do_combined:
		var all: Array = normal_hand + instant_hand
		var all_order: Array = Card.sorted_order(all)

		## 统计 gap：gap[N] = 第 N 张主力之前（或最末之后）的立刻补数
		var gap_counts: Array[int] = []
		gap_counts.resize(n + 1)
		gap_counts.fill(0)
		var n_seen := 0
		for ci in all_order:
			if ci < n:
				normal_order.append(ci)
				n_seen += 1
			else:
				gap_counts[n_seen] += 1
		while normal_order.size() < n:
			for ki in range(n):
				if not normal_order.has(ki):
					normal_order.append(ki)
					break

		## instant 落点：按排序映射修正边界（修复 gi→原始下标混淆）
		var instant_own_order: Array = Card.sorted_order(instant_hand)
		var inst_gap: Array[int] = []
		inst_gap.resize(m)
		inst_gap.fill(-1)
		var nc2 := 0
		for ci in all_order:
			if ci < n:
				nc2 += 1
			else:
				inst_gap[ci - n] = nc2

		var assigned: Array[int] = []
		assigned.resize(n + 1)
		assigned.fill(0)
		instant_positions.resize(m)
		for rp in m:
			var real: int = instant_own_order[rp]
			var gi: int = inst_gap[real]   ## 排序后的间隙编号（0..n）
			gi = clampi(gi, 0, n)
			var cnt := gap_counts[gi]
			var nth := assigned[gi]
			assigned[gi] += 1

			## 通过排序映射取真实的 x 坐标
			var left_x: float
			if gi > 0:
				left_x = normal_xpos[normal_by_pos[gi - 1]] + card_w
			else:
				left_x = normal_xpos[normal_by_pos[0]] - card_w * OVERLAP_DIFF_RANK   ## 紧贴首张主力左边
			var right_x: float
			if gi < n:
				right_x = normal_xpos[normal_by_pos[gi]]
			else:
				right_x = normal_xpos[normal_by_pos[n - 1]] + card_w * OVERLAP_DIFF_RANK   ## 紧贴末张主力右边

			var gc: float
			if gi > 0 and gi < n:
				gc = lerpf(left_x, right_x, 0.5)
			elif gi == 0:
				gc = left_x
			else:
				gc = right_x
			gc = clampi(gc, edge_pad, hand_w - card_w - edge_pad)
			if cnt <= 1:
				instant_positions[real] = gc
			else:
				var spread := gc + (float(nth) - float(cnt - 1) * 0.5) * igap
				instant_positions[real] = clampi(spread, edge_pad, hand_w - card_w - edge_pad)

	return {"card_w": card_w, "card_h": card_h, "igap": igap,
		"start_x": normal_xpos[0] if n > 0 else 0.0,
		"instant_x": normal_xpos[0] if n > 0 else 0.0,
		"total": n + m,
		"normal_xpos": normal_xpos, "normal_ypos": normal_ypos,
		"instant_y": instant_y,
		"instant_positions": instant_positions, "normal_order": normal_order}

func _fan_target(overall: int, m: int, p: int, gp: int, anchor_x: float, rot_scale: float, key: int,
		card_w: float = float(HAND_W), gap: float = float(HAND_CARD_GAP), base_y: float = 0.0,
		x_override: float = NAN, y_override: float = NAN) -> Dictionary:
	var g := gp if (overall > 1) else p
	var t := 0.5 if overall <= 1 else float(g) / float(overall - 1)
	var rot_z := lerpf(-HAND_ROT_MAX, HAND_ROT_MAX, t) * rot_scale
	var x: float = x_override if not is_nan(x_override) else (anchor_x + float(p) * (card_w + gap))
	var py: float = y_override if not is_nan(y_override) else base_y
	var pos := Vector2(x, py)
	if _selected.has(key):
		pos += Vector2(0, -SELECTED_LIFT)
	return {"pos": pos, "rot": rot_z}


func _face(card: Card) -> Control:
	if _is_mobile:
		var visual := CardVisual.new(Vector2(_mobile_card_w, _mobile_card_h))
		visual.show_texture(CardVisual.texture_for_id(Card.id_for(card.point, card.suit)))
		visual.mouse_filter = Control.MOUSE_FILTER_IGNORE
		visual.size_flags_vertical = Control.SIZE_SHRINK_CENTER
		return visual
	var visual := CardVisual.new(Vector2(CARD_W, CARD_H))
	visual.show_texture(CardVisual.texture_for_id(Card.id_for(card.point, card.suit)))
	visual.mouse_filter = Control.MOUSE_FILTER_IGNORE
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
		visual.to_rot(0.0, 0.0)
		visual.modulate = Color(1.0, 1.0, 1.0, 1.0))
	visual.gui_input.connect(func(event):
		_on_gui(visual.get_meta("hand_index", index), event, visual))
	return visual


func _tilt_visual(visual: CardVisual) -> void:
	var local := visual.get_local_mouse_position()
	var sx := visual.size.x
	var sy := visual.size.y
	if local.x < sx * 0.12 or local.x > sx * 0.88 or local.y < sy * 0.12 or local.y > sy * 0.88:
		visual.to_rot(0.0, 0.0, 0.08, Tween.TRANS_QUAD, Tween.EASE_OUT)
		return
	var lx := clampf(inverse_lerp(0.12, 0.88, local.x / sx), 0.0, 1.0)
	var ly := clampf(inverse_lerp(0.12, 0.88, local.y / sy), 0.0, 1.0)
	var rx := lerpf(-_TILT_MAX, _TILT_MAX, ly)
	var ry := lerpf(_TILT_MAX, -_TILT_MAX, lx)
	visual.to_rot(rx, ry, 0.08, Tween.TRANS_QUAD, Tween.EASE_OUT)


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


func style_button_casino(btn: Button, bg: Color, border: Color) -> void:
	style_button_balatro(btn, bg, border)


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