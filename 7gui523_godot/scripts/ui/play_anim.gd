extends "res://scripts/ui/play_base.gd"
class_name PlayAnim

@onready var _damage_delay_bar: ProgressBar = $MainVBox/TableArea/CenterBox/DamageDelayBar

func _setup_damage_delay_bar() -> void:
	if not is_instance_valid(_damage_delay_bar):
		return
	var delay_bg := flat_border(Color(0.0, 0.0, 0.0, 0.0), Color(1.0, 1.0, 1.0, 0.18), 1, 3)
	var delay_fill := flat_border(Color(1.0, 1.0, 1.0, 0.94), Color(1.0, 1.0, 1.0, 1.0), 1, 3)
	_damage_delay_bar.add_theme_stylebox_override("background", delay_bg)
	_damage_delay_bar.add_theme_stylebox_override("fill", delay_fill)

## — 第 2 层 play_anim：动画 · 手牌渲染 · 桌面渲染 · 伤害浮字
##
## 纯容器布局：手牌/BOSS 牌全部放进 HBoxContainer 的固定尺寸槽位，
## 容器负责居中与间距；卡只在槽位内部做上浮/缩放/飞入动画，
## 伤害浮字挂在动画覆盖层上，不参与容器布局。


func _card_size() -> Vector2:
	return Vector2(_mobile_hand_w, _mobile_hand_h) if _device_class != DeviceClass.DESKTOP else Vector2(HAND_W, HAND_H)


func _boss_card_size() -> Vector2:
	return Vector2(_mobile_boss_w, _mobile_boss_h) if _device_class != DeviceClass.DESKTOP else Vector2(BOSS_CARD_W, BOSS_CARD_H)


## 槽位：固定尺寸的 Control，HBox 只排槽位；卡作为槽位子节点可自由上浮/缩放
func _make_slot(card_size: Vector2) -> Control:
	var slot := Control.new()
	slot.custom_minimum_size = card_size
	slot.size_flags_horizontal = Control.SIZE_SHRINK_CENTER
	slot.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	slot.mouse_filter = Control.MOUSE_FILTER_IGNORE
	return slot


## 手牌槽位装配的中间结果：槽位节点、卡视觉、是否新建、以及本次布局目标
class HandSlot:
	var node: Control
	var visual: CardVisual
	var is_new := false
	var gen := 0
	var target_pos := Vector2.ZERO
	var target_scale := Vector2.ONE
	var target_rot := 0.0


## 槽位法手牌行渲染：按「实例 id」增量复用槽位，展示序 = Card.sorted_order
## 用实例 id 而非 card.id：商店可买到同花色同点数的重复牌（同 id），
## 同 id 会互相覆盖槽位映射，导致少渲染一张或整体错位。
func _render_hand_row(row: HBoxContainer, hand: Array[Card], idx_base: int, id_prefix: String,
		tint: Color, z_base: int, animate: bool, base_delay: int, fan: bool = true) -> void:
	var card_size := _card_size()
	## 第二关抹牌虫：先给新摸到的牌定隐藏状态（首轮发牌不隐藏）
	if _state.mask_player_draws and _state.initial_deal_finished:
		for c in hand:
			if not _state.masked_player_cards.has(c):
				_state.masked_player_cards[c] = randf() < (1.0 / 3.0)
	var order: Array[int] = _hand_display_order(hand, id_prefix)
	var n: int = order.size()
	var by_id := {}
	for slot in row.get_children():
		if not slot.is_queued_for_deletion() and slot.has_meta("card_id"):
			by_id[slot.get_meta("card_id")] = slot
	var deck_center: Vector2 = _player_deck.global_position + _player_deck.size * 0.5

	for p in order.size():
		var real: int = order[p]
		var key: int = idx_base + real
		var card: Card = hand[real]
		var cid := "%s_%d" % [id_prefix, card.get_instance_id()]
		var sel := _state.selected.has(key)
		var hs := _take_hand_slot(row, by_id, card, key, cid, card_size, sel)
		row.move_child(hs.node, p)
		_apply_hand_slot_state(hs, key, cid, sel, p, n, card_size, tint, z_base, fan)
		_play_hand_slot_layout(hs, deck_center, animate, base_delay + p)

	## 消失的卡（已打出）：连槽位一起释放，无动画，避免残影
	_free_stale_slots(row, by_id)


## 手牌展示序：玩家拖拽重排过则按数组顺序显示（立刻补区仍按战力排），背面牌统一靠右
func _hand_display_order(hand: Array[Card], id_prefix: String) -> Array[int]:
	if not (_state.manual_hand_order and id_prefix == ""):
		return _pin_masked_last(hand, Card.sorted_order(hand))
	var order: Array[int] = []
	for i in hand.size():
		order.append(i)
	return _pin_masked_last(hand, order)


## 取槽位：命中已有槽位则复用，否则新建槽位并挂一张新手牌卡
func _take_hand_slot(row: HBoxContainer, by_id: Dictionary, card: Card, key: int, cid: String,
		card_size: Vector2, sel: bool) -> HandSlot:
	var hs := HandSlot.new()
	hs.node = by_id.get(cid, null)
	if hs.node == null:
		hs.node = _make_slot(card_size)
		hs.node.set_meta("card_id", cid)
		hs.visual = _hand_card(card, key, sel, card_size)
		if _state.masked_player_cards.get(card, false):
			hs.visual.show_back()
		hs.node.add_child(hs.visual)
		row.add_child(hs.node)
		hs.is_new = true
		return hs
	by_id.erase(cid)
	if hs.node.get_child_count() == 0:
		## 拖拽出牌/排序后槽位被掏空：原地静默重建，不飞入
		hs.visual = _hand_card(card, key, sel, card_size)
		hs.node.add_child(hs.visual)
	else:
		hs.visual = hs.node.get_child(0) as CardVisual
	return hs


## 刷新一张手牌卡的选中态、层级、阴影、扇形与抖动，并算出布局目标
func _apply_hand_slot_state(hs: HandSlot, key: int, cid: String, sel: bool, p: int, n: int,
		card_size: Vector2, tint: Color, z_base: int, fan: bool) -> void:
	var v := hs.visual
	## 记录稳定全局索引（点击/选中/拖拽按它命中，而非 children 顺序）
	v.set_meta("hand_index", key)
	v.kill_layout_tween()
	## 飞入代际标记：使尚未起飞的延迟飞入失效（刷新抢占）
	hs.gen = int(v.get_meta("_fly_gen", 0)) + 1
	v.set_meta("_fly_gen", hs.gen)

	v.mouse_filter = Control.MOUSE_FILTER_STOP
	v.set_selected(sel)
	v.modulate = tint if not sel else Color(1.0, 1.0, 1.0, 1.0)
	v.set_meta("base_modulate", v.modulate)   ## 悬停退出时恢复此色调
	var layer_z := z_base + p + (100 if sel else 0)   ## 选中卡临时提到最高层
	v.base_z = layer_z
	v.z_index = layer_z
	## 屏幕中心光源下，阴影 x 由 CardVisual._update_light_and_shadow 每帧接管；
	## 这里只给一个向下的基础 y，选中时略加深，避免阴影贴死牌底。
	var shadow_offset := Vector2(0.0, 7.0)
	if sel:
		shadow_offset += Vector2(0.0, 1.0)
	v.set_shadow_offset(shadow_offset)

	## 扇形：Demo hand_pos_y_curve 两端 -0.1、中点 +0.1（中点抬高），
	## hand_rot_z_curve 由 -15° → 0° → 15°；立刻补区平放（fan=false）
	var t: float = 0.5 if n <= 1 else float(p) / float(n - 1)
	var arc_curve: float = HAND_ARC_RATIO * (1.0 - 8.0 * pow(t - 0.5, 2.0))
	var arc_y: float = (-arc_curve * card_size.y) if fan else 0.0
	var fan_rot: float = lerpf(-HAND_FAN_ROT_MAX, HAND_FAN_ROT_MAX, t) if fan else 0.0

	## 手牌拟真抖动：每牌稳定的微旋转(-3~3°)与垂直偏移(-5~5px)
	## 选中/拖拽时平滑归零（目标取 0），避免拖拽歪斜
	var jit = _hand_jitter.get(cid, {})
	if jit.is_empty():
		jit = {
			"rot": randf_range(-3.0, 3.0),
			"y": randf_range(-5.0, 5.0),
		}
		_hand_jitter[cid] = jit
	if sel:
		hs.target_pos = Vector2(0.0, -SELECTED_LIFT + arc_y)
		hs.target_rot = 0.0
	else:
		hs.target_pos = Vector2(0.0, float(jit["y"]) + arc_y)
		hs.target_rot = fan_rot + float(jit["rot"])
	hs.target_scale = Vector2(SELECTED_SCALE, SELECTED_SCALE) if sel else Vector2.ONE
	v.set_meta("_base_scale", hs.target_scale)   ## 悬停弹性缩放的基准


## 单张手牌入场：新建/动画中走牌堆飞入，否则走容器内回弹过渡
func _play_hand_slot_layout(hs: HandSlot, deck_center: Vector2, animate: bool,
		stagger: int) -> void:
	var v := hs.visual
	if animate or hs.is_new:
		## 从牌堆中心飞入 + 淡入（起点需等槽位布局完成，见 _fly_in_after_layout）
		## 压 a=0 在 modulate=tint 赋值之后，否则 fade tween 首帧捕获初值被覆盖
		v.modulate.a = 0.0
		if animate:
			var deal_delay := INITIAL_DEAL_DELAY if not _state.initial_deal_finished else DEAL_DELAY
			var deal_duration := INITIAL_DEAL_DURATION if not _state.initial_deal_finished else DEAL_DURATION
			_fly_in_after_layout(hs.node, v, deck_center, hs.target_pos, hs.target_scale,
				hs.target_rot, float(stagger) * deal_delay, deal_duration, deal_duration * 0.5, hs.gen)
		else:
			_fly_in_after_layout(hs.node, v, deck_center, hs.target_pos, hs.target_scale,
				hs.target_rot, 0.0, LAYOUT_TWEEN, 0.15, hs.gen)
		return
	v.set_fake3d_rot(0.0, 0.0)
	var tw := v.create_tween()
	tw.set_parallel(true)
	tw.set_ease(Tween.EASE_OUT).set_trans(Tween.TRANS_CUBIC)
	tw.tween_property(v, "position", hs.target_pos, LAYOUT_TWEEN)
	## 缩放/旋转用 BACK 回弹（对齐 Demo to_scale/to_rot 的 TRANS_BACK）
	tw.tween_property(v, "scale", hs.target_scale, LAYOUT_TWEEN) \
		.set_trans(Tween.TRANS_BACK).set_ease(Tween.EASE_OUT)
	tw.tween_property(v, "rotation", deg_to_rad(hs.target_rot), LAYOUT_TWEEN) \
		.set_trans(Tween.TRANS_BACK).set_ease(Tween.EASE_OUT)
	v._layout_tween = tw


## 释放本行中已不在手牌里的槽位
func _free_stale_slots(row: HBoxContainer, by_id: Dictionary) -> void:
	for cid in by_id:
		var dead: Control = by_id[cid]
		if is_instance_valid(dead):
			row.remove_child(dead)
			dead.queue_free()


## 背面朝上的牌不参与排序：从显示序列里挑出来，统一追加到最右端。
## 背面牌彼此按手牌数组下标稳定排列。
func _pin_masked_last(hand: Array[Card], order: Array[int]) -> Array[int]:
	var front: Array[int] = []
	var back: Array[int] = []
	for idx in order:
		if _state.masked_player_cards.get(hand[idx], false):
			back.append(idx)
		else:
			front.append(idx)
	if back.is_empty():
		return order
	back.sort()
	front.append_array(back)
	return front


## 飞入动画：等两帧让 HBox 完成槽位布局，再换算牌堆中心到槽位局部坐标
func _fly_in_after_layout(slot: Control, v: CardVisual, deck_center_global: Vector2,
		target_pos: Vector2, target_scale: Vector2, target_rot: float,
		delay: float, dur: float, fade_dur: float, gen: int) -> void:
	await get_tree().process_frame
	await get_tree().process_frame
	if not is_instance_valid(slot) or not is_instance_valid(v):
		return
	## 期间被再次刷新（gen 已递增）则放弃本次飞入，避免覆盖新动画
	if int(v.get_meta("_fly_gen", -1)) != gen:
		return
	v.position = slot.get_global_transform_with_canvas().affine_inverse() * deck_center_global
	var tw := v.create_tween()
	tw.set_ease(Tween.EASE_OUT).set_trans(Tween.TRANS_CUBIC)
	tw.tween_interval(delay)
	tw.set_parallel(true)
	tw.tween_property(v, "position", target_pos, dur)
	tw.tween_property(v, "scale", target_scale, dur) \
		.set_trans(Tween.TRANS_BACK).set_ease(Tween.EASE_OUT)
	tw.tween_property(v, "rotation", deg_to_rad(target_rot), dur)
	tw.tween_property(v, "modulate:a", 1.0, fade_dur)
	v._layout_tween = tw


## 手牌增量刷新：按 card_id 增量，增删有 layout 过渡
func _refresh_hand_smooth() -> void:
	if not is_instance_valid(_state.round):
		return
	var n := _state.human.normal_hand.size()
	_render_hand_row(_bottom_hand, _state.human.normal_hand, 0, "", Color.WHITE, 30, false, 0)
	_render_hand_row(_top_hand, _state.human.instant_hand, n, "i_", INSTANT_TINT, 10, false, 0, false)


## 刷新手牌（animate 开头发牌飞入）
func _refresh_hand_with_animation(animate: bool) -> void:
	if animate:
		_state.manual_hand_order = false   ## 新一关发牌：恢复自动排序
	for row: HBoxContainer in [_top_hand, _bottom_hand]:
		_clear_children(row)
	if _state.human.normal_hand.is_empty() and _state.human.instant_hand.is_empty():
		return
	var n := _state.human.normal_hand.size()
	_render_hand_row(_bottom_hand, _state.human.normal_hand, 0, "", Color.WHITE, 30, animate, 0)
	_render_hand_row(_top_hand, _state.human.instant_hand, n, "i_", INSTANT_TINT, 10, animate, n, false)


## BOSS 手牌渲染（全牌背）
func _refresh_boss_hand() -> void:
	_refresh_boss_hand_with_animation(false)


func _refresh_boss_hand_with_animation(animate: bool) -> void:
	## 普通区(5张)与全伤害区(2张)分两个 HBox 渲染，中间留出明显空隙
	_render_boss_back_row(_boss_hand, _state.ai.normal_hand.size(), animate, "_boss_known_normal")
	_render_boss_back_row(_boss_hand_instant, _state.ai.instant_hand.size(), animate, "_boss_known_instant")


## 渲染一组 BOSS 牌背到指定 HBox（known_var 用 meta 存在节点上，避免 GDScript 无引用传参）
## 训练模式 show_ai_hand=true 时改为显示牌面正面，使用与玩家相同的排序逻辑
func _render_boss_back_row(row: HBoxContainer, count: int, animate: bool, known_key: String) -> void:
	var card_size := _boss_card_size()
	var deck_center: Vector2 = _boss_deck.global_position + _boss_deck.size * 0.5
	var known: int = int(row.get_meta(known_key, 0))
	var show_face := _training.is_active and _training.show_ai_hand
	var ai_hand: Array[Card] = []
	if show_face:
		if known_key == "_boss_known_normal":
			ai_hand = _state.ai.normal_hand
		else:
			ai_hand = _state.ai.instant_hand

	if show_face:
		## 训练模式正面显示：用 sorted_order + by_id 追踪每张牌，对齐玩家手牌渲染逻辑
		_render_boss_face_row(row, ai_hand, card_size, deck_center, animate, known, known_key)
		return

	## —— 标准牌背渲染（无训练模式 / show_ai_hand=false） ——
	while row.get_child_count() > count:
		var s: Control = row.get_child(row.get_child_count() - 1)
		row.remove_child(s)
		s.queue_free()
	while row.get_child_count() < count:
		var slot := _make_slot(card_size)
		var cv := CardVisual.new(card_size)
		cv.show_back()
		cv.enable_fake3d(false)
		cv.mouse_filter = Control.MOUSE_FILTER_IGNORE
		slot.add_child(cv)
		row.add_child(slot)
		var idx := row.get_child_count() - 1
		if animate or idx >= known:
			cv.modulate.a = 0.0
			var deal_delay := INITIAL_DEAL_DELAY if not _state.initial_deal_finished else DEAL_DELAY
			_boss_fly_in_after_layout(slot, cv, deck_center,
				float(idx) * deal_delay if animate else 0.0)
	row.set_meta(known_key, count)


## 训练模式：按 sorted_order 排序后渲染 AI 手牌正面（与玩家侧一致的显示顺序）
func _render_boss_face_row(row: HBoxContainer, hand: Array[Card], card_size: Vector2,
		deck_center: Vector2, animate: bool, known: int, known_key: String) -> void:
	var order: Array[int] = Card.sorted_order(hand)
	var by_id := {}
	for slot in row.get_children():
		if not slot.is_queued_for_deletion() and slot.has_meta("card_id"):
			by_id[slot.get_meta("card_id")] = slot

	for p in order.size():
		var real: int = order[p]
		var card: Card = hand[real]
		var cid := "boss_%d" % card.get_instance_id()
		var slot: Control = by_id.get(cid, null)
		var cv: CardVisual = null
		var is_new := false
		if slot == null:
			slot = _make_slot(card_size)
			slot.set_meta("card_id", cid)
			cv = CardVisual.new(card_size)
			cv.show_id(card.id)
			cv.enable_fake3d(false)
			cv.mouse_filter = Control.MOUSE_FILTER_IGNORE
			slot.add_child(cv)
			row.add_child(slot)
			is_new = true
		else:
			by_id.erase(cid)
			if slot.get_child_count() > 0:
				cv = slot.get_child(0) as CardVisual
			else:
				cv = CardVisual.new(card_size)
				cv.show_id(card.id)
				cv.enable_fake3d(false)
				cv.mouse_filter = Control.MOUSE_FILTER_IGNORE
				slot.add_child(cv)
				is_new = true
		row.move_child(slot, p)

		if is_new and (animate or p >= known):
			cv.modulate.a = 0.0
			var deal_delay := INITIAL_DEAL_DELAY if not _state.initial_deal_finished else DEAL_DELAY
			_boss_fly_in_after_layout(slot, cv, deck_center,
				float(p) * deal_delay if animate else 0.0)

	## 删除不属于 hand 的槽位
	for leftover in by_id.values():
		if is_instance_valid(leftover):
			var slot: Control = leftover
			row.remove_child(slot)
			slot.queue_free()

	row.set_meta(known_key, order.size())


func _boss_fly_in_after_layout(slot: Control, cv: CardVisual, deck_center_global: Vector2, delay: float) -> void:
	await get_tree().process_frame
	await get_tree().process_frame
	if not is_instance_valid(slot) or not is_instance_valid(cv):
		return
	cv.position = slot.get_global_transform_with_canvas().affine_inverse() * deck_center_global
	var tw := cv.create_tween()
	tw.set_ease(Tween.EASE_OUT).set_trans(Tween.TRANS_CUBIC)
	tw.tween_interval(delay)
	tw.set_parallel(true)
	tw.tween_property(cv, "position", Vector2.ZERO, DEAL_DURATION)
	tw.tween_property(cv, "modulate:a", 1.0, DEAL_DURATION * 0.4)


func _refresh_table() -> void:
	var par := CardTypeResult.parse_any_card_type(_state.round.table_cards)
	if par != null and par.type != CardTypeResult.CardType.INVALID:
		_round_lbl.text = CardTypeNames.name_for(par.type)
		_round_lbl.add_theme_color_override("font_color", COL_GOLD)

	## 桌面筹码板：本小局双方已下注的累计筹码
	_table_score_lbl.text = "桌面筹码: %d" % _state.round.table_chips

	if _table_anim_running:
		return

	## 桌面牌签名：与当前渲染一致且无待播出牌动画时跳过重建，
	## 避免 pass/回合切换刷新把桌面牌溶解一遍再瞬间摆回（视觉噪音）
	var sig := ""
	for card in _state.round.table_cards:
		sig += "%d," % card.id
	if sig == _table_render_sig and not _table_anim_pending:
		return

	_dissolve_old_table_cards()
	_table_cards_anim.clear()
	_table_render_sig = sig
	if _state.round.table_cards.is_empty():
		return

	var animate := _table_anim_pending
	_table_anim_pending = false
	for card in _state.round.table_cards:
		var visual := _face(card)
		_table_cards.add_child(visual)
		if animate:
			visual.modulate.a = 0.0
			_table_cards_anim.append(visual)

	if animate and not _table_cards_anim.is_empty():
		_run_table_anim()


func _dissolve_old_table_cards() -> void:
	for child in _table_cards.get_children():
		if child is CardVisual:
			var cv: CardVisual = child
			var orig: Vector2 = cv.global_position
			_table_cards.remove_child(cv)
			_anim_overlay.add_child(cv)
			cv.global_position = orig
			cv.dissolve(DISSOLVE_DURATION)
		else:
			child.queue_free()


## 桌面出牌动画的几何快照：牌尺寸、间距与容器落点基准
class TableGeom:
	var card_w := 0.0
	var card_h := 0.0
	var half := Vector2.ZERO
	var sep := 0.0
	var total_child_w := 0.0
	var container_size := Vector2.ZERO
	var container_pos := Vector2.ZERO


func _run_table_anim() -> void:
	_table_anim_running = true

	var cw: float = minf(_mobile_card_w * 1.12, TABLE_CARD_W) if _device_class != DeviceClass.DESKTOP else TABLE_CARD_W
	var ch: float = minf(_mobile_card_h * 1.12, TABLE_CARD_H) if _device_class != DeviceClass.DESKTOP else TABLE_CARD_H
	if not _table_anim_src_ok():
		return

	await _await_table_layout(cw, ch)

	var n := _table_cards_anim.size()
	var geom := _table_anim_geom(n, cw, ch)
	var total := 0.0
	for i in n:
		total = maxf(total, _fly_table_card(i, geom))

	await get_tree().create_timer(total + TABLE_ANIM_TAIL).timeout
	_table_anim_running = false
	if _table_anim_pending and is_instance_valid(_state.round):
		_refresh_table()


## 出牌起点缺失时无法飞牌：直接揭示桌面牌，并结束动画状态
func _table_anim_src_ok() -> bool:
	if _play_src_global != Vector2.ZERO:
		return true
	push_warning("桌面动画：出牌起点未记录（_play_src_global 为零），跳过飞牌动画")
	for real in _table_cards_anim:
		if is_instance_valid(real):
			real.modulate.a = 1.0
	_table_anim_running = false
	return false


## 等待嵌套容器（MainVBox→TableArea→CenterBox→TableCards）布局稳定
func _await_table_layout(cw: float, ch: float) -> void:
	## 通过验证循环等待容器尺寸达标
	for _retry in 8:
		await get_tree().process_frame
		if _table_cards.size.x >= cw and _table_cards.size.y >= ch:
			return
	push_warning("桌面布局未稳定（尺寸 %s），使用当前几何继续动画" % _table_cards.size)


## 依据容器几何一次性算出每张牌的落点参数
func _table_anim_geom(n: int, cw: float, ch: float) -> TableGeom:
	var geom := TableGeom.new()
	geom.card_w = cw
	geom.card_h = ch
	geom.half = Vector2(cw, ch) * 0.5
	geom.sep = float(_table_cards.get_theme_constant("separation"))
	geom.total_child_w = float(n) * cw + float(n - 1) * geom.sep
	geom.container_size = _table_cards.size
	geom.container_pos = _table_cards.global_position
	return geom


## 单张桌面牌：生成飞行幽灵卡，返回落定时刻（相对动画起点）
func _fly_table_card(i: int, geom: TableGeom) -> float:
	var real: Control = _table_cards_anim[i]
	if not is_instance_valid(real) or i >= _state.round.table_cards.size():
		return 0.0

	var start_x := (geom.container_size.x - geom.total_child_w) * 0.5  ## alignment = CENTER
	var target := geom.container_pos + Vector2(
		start_x + geom.card_w * 0.5 + float(i) * (geom.card_w + geom.sep),
		geom.container_size.y * 0.5
	)
	var card: Card = _state.round.table_cards[i]

	var ghost := CardVisual.new(Vector2(geom.card_w, geom.card_h))
	ghost.show_texture(CardVisual.texture_for_id(card.id))
	ghost.mouse_filter = Control.MOUSE_FILTER_IGNORE
	ghost.z_index = 100
	ghost.pivot_offset = geom.half
	_anim_overlay.add_child(ghost)
	ghost.global_position = _play_src_global - geom.half
	ghost.scale = Vector2(1.18, 1.18)
	ghost.rotation = deg_to_rad(-20.0)

	var mid := (_play_src_global + target) * 0.5 + Vector2(0, -PLAY_ANIM_ARC)
	var delay := float(i) * PLAY_ANIM_STAGGER

	var move_tw := ghost.create_tween()
	move_tw.tween_interval(delay)
	move_tw.tween_property(ghost, "global_position", mid - geom.half, PLAY_ANIM_DURATION * 0.45) \
		.set_ease(Tween.EASE_OUT).set_trans(Tween.TRANS_QUAD)
	## 二段用 BOUNCE 落地（对齐 Demo go_to_play：t2 = TRANS_BOUNCE / EASE_OUT）
	move_tw.tween_interval(PLAY_ANIM_DURATION * 0.05)
	move_tw.tween_property(ghost, "global_position", target - geom.half, PLAY_ANIM_DURATION * 0.50) \
		.set_ease(Tween.EASE_OUT).set_trans(Tween.TRANS_BOUNCE)

	var fx_tw := ghost.create_tween()
	fx_tw.tween_interval(delay)
	fx_tw.set_parallel(true)
	fx_tw.tween_property(ghost, "scale", Vector2.ONE, PLAY_ANIM_DURATION) \
		.set_ease(Tween.EASE_OUT).set_trans(Tween.TRANS_BOUNCE)
	fx_tw.tween_property(ghost, "rotation", 0.0, PLAY_ANIM_DURATION) \
		.set_ease(Tween.EASE_OUT).set_trans(Tween.TRANS_BOUNCE)

	var land := delay + PLAY_ANIM_DURATION
	get_tree().create_timer(land).timeout.connect(_on_ghost_landed.bind(real, ghost, card.score))
	return land


## 背面牌翻面：横向压扁 → 在侧面切换成正面前 → 弹回缩放。
## 拖出背面牌时调用，保证拖拽与桌面出牌动画里的牌都是正面朝上。
func _flip_visual_to_front(v: CardVisual, card: Card) -> void:
	if not is_instance_valid(v) or card == null:
		return
	var base_x: float = v.scale.x
	var tw := v.create_tween()
	tw.set_trans(Tween.TRANS_CUBIC).set_ease(Tween.EASE_IN_OUT)
	tw.tween_property(v, "scale:x", 0.02, 0.12)
	tw.tween_callback(func(): v.show_texture(CardVisual.texture_for_id(card.id)))
	tw.tween_property(v, "scale:x", base_x, 0.16) \
		.set_trans(Tween.TRANS_BACK).set_ease(Tween.EASE_OUT)


## 非法出牌：拖拽中的实体牌先飞到桌面（正面朝上）→ 激烈晃动 → 飞回手牌槽位。
## 手牌数据全程未改动，动画结束后由调用方刷新手牌。
func _play_invalid_shake_and_return() -> void:
	_invalid_anim_running = true
	_dragging = false
	_drag_first_index = -1
	_drop_hint.visible = false
	_discard_hint.visible = false
	var visuals: Array[CardVisual] = _drag_visuals.duplicate()
	var origins: Array[DragOrigin] = _drag_origin.duplicate()
	_drag_visuals.clear()
	_drag_indices.clear()
	_drag_origin.clear()
	if visuals.is_empty():
		_invalid_anim_running = false
		return

	## 桌面落点：与合法出牌同一套容器几何
	var cw: float = minf(_mobile_card_w * 1.12, TABLE_CARD_W) if _device_class != DeviceClass.DESKTOP else TABLE_CARD_W
	var ch: float = minf(_mobile_card_h * 1.12, TABLE_CARD_H) if _device_class != DeviceClass.DESKTOP else TABLE_CARD_H
	var n := visuals.size()
	var sep := float(_table_cards.get_theme_constant("separation"))
	var total_child_w := float(n) * cw + float(n - 1) * sep
	var container_size := _table_cards.size
	var container_pos := _table_cards.global_position
	var bases: Array[Vector2] = []

	for i in n:
		var v: CardVisual = visuals[i]
		if not is_instance_valid(v):
			bases.append(Vector2.ZERO)
			continue
		var target := container_pos + Vector2(
			(container_size.x - total_child_w) * 0.5 + cw * 0.5 + float(i) * (cw + sep),
			container_size.y * 0.5)
		var pos := target - v.size * 0.5
		bases.append(pos)
		var tw := v.create_tween()
		v._layout_tween = tw
		tw.set_ease(Tween.EASE_OUT).set_trans(Tween.TRANS_CUBIC)
		tw.tween_property(v, "global_position", pos, 0.18)
	await get_tree().create_timer(INVALID_SHAKE_FLY).timeout

	## 激烈晃动：角度 ±13°、横向 ±9px，快速来回
	for i in visuals.size():
		var v: CardVisual = visuals[i]
		if is_instance_valid(v):
			_shake_card(v, bases[i])
	await get_tree().create_timer(INVALID_SHAKE_DURATION).timeout

	## 飞回手牌原槽位
	for i in visuals.size():
		var v: CardVisual = visuals[i]
		if not is_instance_valid(v):
			continue
		var o: DragOrigin = origins[i]
		var cur := v.global_position
		if is_instance_valid(o.parent):
			v.reparent(o.parent)
		v.global_position = cur
		v.z_index = o.z
		var tw := v.create_tween()
		v._layout_tween = tw
		tw.set_ease(Tween.EASE_OUT).set_trans(Tween.TRANS_BACK)
		tw.set_parallel(true)
		tw.tween_property(v, "position", o.pos, DRAG_RETURN_DUR)
		tw.tween_property(v, "rotation", o.rot, DRAG_RETURN_DUR)
		tw.tween_property(v, "scale", o.scale, DRAG_RETURN_DUR)
	await get_tree().create_timer(DRAG_RETURN_DUR + DRAG_RETURN_TAIL).timeout
	_invalid_anim_running = false


## 单张牌激烈晃动：以 base 为基准做角度与位移抖动，最后摆正
func _shake_card(v: CardVisual, base: Vector2) -> void:
	if not is_instance_valid(v):
		return
	var tw := v.create_tween()
	tw.set_trans(Tween.TRANS_SINE).set_ease(Tween.EASE_IN_OUT)
	for k in 6:
		var dir := 1.0 if k % 2 == 0 else -1.0
		tw.set_parallel(true)
		tw.tween_property(v, "global_position", base + Vector2(9.0 * dir, -3.0), 0.055)
		tw.tween_property(v, "rotation", deg_to_rad(13.0) * dir, 0.055)
		tw.set_parallel(false)
	tw.set_parallel(true)
	tw.tween_property(v, "global_position", base, 0.06)
	tw.tween_property(v, "rotation", 0.0, 0.06)


## 结算延迟条：牌面和浮字先完成，随后再进入关卡结算。
func _play_damage_delay(duration: float = ROUND_DAMAGE_DELAY) -> void:
	if not is_instance_valid(_damage_delay_bar):
		await get_tree().create_timer(duration).timeout
		return
	_damage_delay_bar.visible = true
	_damage_delay_bar.value = 0.0
	var tw := create_tween()
	tw.tween_property(_damage_delay_bar, "value", 100.0, duration)
	await tw.finished
	_damage_delay_bar.visible = false


func _on_ghost_landed(real: Control, ghost: Control, card_score: int) -> void:
	if is_instance_valid(real):
		real.modulate.a = 1.0
	if is_instance_valid(ghost):
		ghost.dissolve(DISSOLVE_DURATION * 0.6)
	if card_score > 0:
		_spawn_floating_chips(card_score, "筹码")


func _refresh_hand() -> void:
	_refresh_hand_smooth()


func _refresh_turn() -> void:
	if _state.finished:
		return
	if not _state.round.table_cards.is_empty():
		return
	var cur := _state.round.whose_turn()
	if cur == _state.human:
		_round_lbl.text = "你的回合 — 选牌出牌"
		_round_lbl.add_theme_color_override("font_color", COL_GREEN.lightened(0.25))
	else:
		_round_lbl.text = "%s 回合" % _state.ai.name
		_round_lbl.add_theme_color_override("font_color", COL_CRIMSON.lightened(0.15))


func _refresh_status(last_action: String) -> void:
	## 状态栏仅显示关键动作；伤害与压迫加分另用浮字标记。
	if not last_action.is_empty():
		_round_lbl.text = last_action
		_round_lbl.add_theme_color_override("font_color", COL_GOLD)


## 在桌面上方生成一次性筹码浮字，不进入任何容器布局。
## penalty=true 时显示红色「-N」（非法出牌扣筹码）。
func _spawn_floating_chips(amount: int, source: String = "筹码", penalty: bool = false) -> void:
	if amount <= 0 or not is_instance_valid(_anim_overlay):
		return
	var label := Label.new()
	label.text = "%s %s%d" % [source, "-" if penalty else "+", amount]
	label.mouse_filter = Control.MOUSE_FILTER_IGNORE
	label.z_index = 320
	label.add_theme_font_size_override("font_size", 24)
	label.add_theme_color_override("font_color", COL_CRIMSON if penalty else COL_GOLD)
	label.add_theme_color_override("font_shadow_color", Color(0.02, 0.02, 0.03, 0.9))
	label.add_theme_constant_override("shadow_offset_x", 2)
	label.add_theme_constant_override("shadow_offset_y", 3)
	_anim_overlay.add_child(label)
	label.reset_size()
	var center := _table_cards.get_global_rect().get_center()
	label.global_position = center - Vector2(label.size.x * 0.5, label.size.y * 0.5)
	var start := label.global_position
	var target := start + Vector2(0, -54)
	label.modulate.a = 0.0
	var tw := label.create_tween()
	tw.set_parallel(true)
	tw.set_ease(Tween.EASE_OUT).set_trans(Tween.TRANS_QUAD)
	tw.tween_property(label, "global_position", target, 0.65)
	tw.tween_property(label, "modulate:a", 1.0, 0.12)
	tw.tween_property(label, "modulate:a", 0.0, 0.28).set_delay(0.37)
	tw.chain().tween_callback(label.queue_free)


func _refresh_top() -> void:
	_title_lbl.text = _state.ai.name
	_deck_count_lbl.text = "第 %d 关 ｜ 我方 手牌%d/牌堆%d ｜ 对方 手牌%d/牌堆%d" % \
		[_state.campaign.current_level,
		_state.human.hand_size(), _state.human_deck.active_count,
		_state.ai.hand_size(), _state.ai_deck.active_count]
	_chips_player_lbl.text = "你 %d" % _state.human.chips
	_chips_boss_lbl.text = "对方 %d" % _state.ai.chips
	_update_dream_speed()
	## 显示"可抽牌堆"（active 区）；隐藏区是下关库存，不展示也不参与关内抽牌。
	if is_instance_valid(_player_deck_lbl):
		_player_deck_lbl.text = "牌堆\n%d" % _state.human_deck.active_count
	if is_instance_valid(_boss_deck_lbl):
		_boss_deck_lbl.text = "牌堆\n%d" % _state.ai_deck.active_count
	## 牌堆抽空 → 整套牌背燃烧溶解（只触发一次）
	_update_deck_empty(_player_deck, _state.human_deck.active_count == 0)
	_update_deck_empty(_boss_deck, _state.ai_deck.active_count == 0)


func _refresh(last_action: String) -> void:
	if not is_instance_valid(_state.round):
		return
	_refresh_top()
	_refresh_table()
	_refresh_boss_hand()
	_refresh_hand()
	_refresh_turn()
	_refresh_status(last_action)