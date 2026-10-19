extends "res://scripts/ui/play_base.gd"
class_name PlayAnim

## — 第 2 层 play_anim：动画 · 手牌渲染 · 桌面渲染
##
## 纯容器布局：手牌/BOSS 牌全部放进 HBoxContainer 的固定尺寸槽位，
## 容器负责居中与间距；卡只在槽位内部做上浮/缩放/飞入动画，不算任何坐标。


func _card_size() -> Vector2:
	return Vector2(_mobile_hand_w, _mobile_hand_h) if _is_mobile else Vector2(HAND_W, HAND_H)


func _boss_card_size() -> Vector2:
	return Vector2(_mobile_boss_w, _mobile_boss_h) if _is_mobile else Vector2(BOSS_CARD_W, BOSS_CARD_H)


## 槽位：固定尺寸的 Control，HBox 只排槽位；卡作为槽位子节点可自由上浮/缩放
func _make_slot(card_size: Vector2) -> Control:
	var slot := Control.new()
	slot.custom_minimum_size = card_size
	slot.size_flags_horizontal = Control.SIZE_SHRINK_CENTER
	slot.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	slot.mouse_filter = Control.MOUSE_FILTER_IGNORE
	return slot


## 槽位法手牌行渲染：按 card_id 增量复用槽位，展示序 = Card.sorted_order
func _render_hand_row(row: HBoxContainer, hand: Array, idx_base: int, id_prefix: String,
		tint: Color, z_base: int, animate: bool, base_delay: int) -> void:
	var card_size := _card_size()
	var order: Array = Card.sorted_order(hand)
	var by_id := {}
	for slot in row.get_children():
		if not slot.is_queued_for_deletion() and slot.has_meta("card_id"):
			by_id[slot.get_meta("card_id")] = slot
	var deck_center: Vector2 = _player_deck.global_position + _player_deck.size * 0.5

	for p in order.size():
		var real: int = order[p]
		var key: int = idx_base + real
		var card: Card = hand[real]
		var cid := "%s%d" % [id_prefix, Card.id_for(card.point, card.suit)]
		var sel := _selected.has(key)
		var slot: Control = by_id.get(cid, null)
		var v: CardVisual = null
		var is_new := false
		if slot == null:
			slot = _make_slot(card_size)
			slot.set_meta("card_id", cid)
			v = _hand_card(card, key, sel, card_size)
			slot.add_child(v)
			row.add_child(slot)
			is_new = true
		else:
			by_id.erase(cid)
			if slot.get_child_count() == 0:
				## 拖拽出牌/排序后槽位被掏空：原地静默重建，不飞入
				v = _hand_card(card, key, sel, card_size)
				slot.add_child(v)
			else:
				v = slot.get_child(0) as CardVisual
		row.move_child(slot, p)

		## 记录稳定全局索引（点击/选中/拖拽按它命中，而非 children 顺序）
		v.set_meta("hand_index", key)
		v.kill_layout_tween()
		## 飞入代际标记：使尚未起飞的延迟飞入失效（刷新抢占）
		var gen: int = int(v.get_meta("_fly_gen", 0)) + 1
		v.set_meta("_fly_gen", gen)

		v.mouse_filter = Control.MOUSE_FILTER_STOP
		v.set_selected(sel)
		v.modulate = tint if not sel else Color(1.0, 1.0, 1.0, 1.0)
		v.set_meta("base_modulate", v.modulate)   ## 悬停退出时恢复此色调
		var layer_z := z_base + p + (100 if sel else 0)   ## 选中卡临时提到最高层
		v.base_z = layer_z
		v.z_index = layer_z

		## 手牌拟真抖动：每牌稳定的微旋转(-3~3°)与垂直偏移(-5~5px)
		## 选中/拖拽时平滑归零（目标取 0），避免拖拽歪斜
		var jit = _hand_jitter.get(cid, {})
		if jit.is_empty():
			jit = {
				"rot": randf_range(-3.0, 3.0),
				"y": randf_range(-5.0, 5.0),
			}
			_hand_jitter[cid] = jit
		var target_pos: Vector2
		var target_rot: float
		if sel:
			target_pos = Vector2(0.0, -SELECTED_LIFT)
			target_rot = 0.0
		else:
			target_pos = Vector2(0.0, jit["y"])
			target_rot = jit["rot"]
		var target_scale := Vector2(SELECTED_SCALE, SELECTED_SCALE) if sel else Vector2.ONE

		if animate or is_new:
			## 从牌堆中心飞入 + 淡入（起点需等槽位布局完成，见 _fly_in_after_layout）
			## 压 a=0 在 modulate=tint 赋值之后，否则 fade tween 首帧捕获初值被覆盖
			v.modulate.a = 0.0
			if animate:
				_fly_in_after_layout(slot, v, deck_center, target_pos, target_scale, target_rot,
					float(base_delay + p) * DEAL_DELAY, DEAL_DURATION, DEAL_DURATION * 0.5, gen)
			else:
				_fly_in_after_layout(slot, v, deck_center, target_pos, target_scale, target_rot,
					0.0, LAYOUT_TWEEN, 0.15, gen)
		else:
			v.set_fake3d_rot(0.0, 0.0)
			var tw := v.create_tween()
			tw.set_ease(Tween.EASE_OUT).set_trans(Tween.TRANS_CUBIC)
			tw.set_parallel(true)
			tw.tween_property(v, "position", target_pos, LAYOUT_TWEEN)
			tw.tween_property(v, "scale", target_scale, LAYOUT_TWEEN)
			tw.tween_property(v, "rotation", deg_to_rad(target_rot), LAYOUT_TWEEN)
			v._layout_tween = tw

	## 消失的卡（已打出）：连槽位一起释放，无动画，避免残影
	for cid in by_id:
		var dead: Control = by_id[cid]
		if is_instance_valid(dead):
			row.remove_child(dead)
			dead.queue_free()


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
	tw.tween_property(v, "scale", target_scale, dur)
	tw.tween_property(v, "rotation", deg_to_rad(target_rot), dur)
	tw.tween_property(v, "modulate:a", 1.0, fade_dur)
	v._layout_tween = tw


## 手牌增量刷新：按 card_id 增量，增删有 layout 过渡
func _refresh_hand_smooth() -> void:
	if not is_instance_valid(_round):
		return
	var n := _human.normal_hand.size()
	_render_hand_row(_bottom_hand, _human.normal_hand, 0, "", Color.WHITE, 30, false, 0)
	_render_hand_row(_top_hand, _human.instant_hand, n, "i_", INSTANT_TINT, 10, false, 0)


## 刷新手牌（animate 开头发牌飞入）
func _refresh_hand_with_animation(animate: bool) -> void:
	for row: HBoxContainer in [_top_hand, _bottom_hand]:
		for slot in row.get_children():
			row.remove_child(slot)
			slot.queue_free()
	if _human.normal_hand.is_empty() and _human.instant_hand.is_empty():
		return
	var n := _human.normal_hand.size()
	_render_hand_row(_bottom_hand, _human.normal_hand, 0, "", Color.WHITE, 30, animate, 0)
	_render_hand_row(_top_hand, _human.instant_hand, n, "i_", INSTANT_TINT, 10, animate, n)


## BOSS 手牌渲染（全牌背）
func _refresh_boss_hand() -> void:
	_refresh_boss_hand_with_animation(false)


func _refresh_boss_hand_with_animation(animate: bool) -> void:
	## 普通区(5张)与全伤害区(2张)分两个 HBox 渲染，中间留出明显空隙
	_render_boss_back_row(_boss_hand, _ai.normal_hand.size(), animate, "_boss_known_normal")
	_render_boss_back_row(_boss_hand_instant, _ai.instant_hand.size(), animate, "_boss_known_instant")


## 渲染一组 BOSS 牌背到指定 HBox（known_var 用 meta 存在节点上，避免 GDScript 无引用传参）
func _render_boss_back_row(row: HBoxContainer, count: int, animate: bool, known_key: String) -> void:
	var card_size := _boss_card_size()
	var deck_center: Vector2 = _boss_deck.global_position + _boss_deck.size * 0.5
	while row.get_child_count() > count:
		var s: Control = row.get_child(row.get_child_count() - 1)
		row.remove_child(s)
		s.queue_free()
	var known: int = int(row.get_meta(known_key, 0))
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
			_boss_fly_in_after_layout(slot, cv, deck_center,
				float(idx) * DEAL_DELAY if animate else 0.0)
	row.set_meta(known_key, count)


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
	var par := CardTypeResult.parse_any_card_type(_round.table_cards)
	if par != null and par.type != CardTypeResult.CardType.INVALID:
		_round_lbl.text = CardTypeNames.name_for(par.type)
		_round_lbl.add_theme_color_override("font_color", COL_GOLD)

	## 计分板：当前桌面原始分 + 压制奖励，居中置于出牌区正下方
	var raw := Score.calculate_score(_round.table_cards)
	var bonus := 0
	if par != null and par.type != CardTypeResult.CardType.INVALID \
			and _round.last_play != null and not _round.last_play.cards.is_empty():
		bonus = CardTypeResult.calculate_pressure_bonus(par, _round.last_play)
	_table_score_lbl.text = "原始分: %d ｜ 合计: %d" % [raw, raw + bonus]

	if _table_anim_running:
		return

	## 桌面牌签名：与当前渲染一致且无待播出牌动画时跳过重建，
	## 避免 pass/回合切换刷新把桌面牌溶解一遍再瞬间摆回（视觉噪音）
	var sig := ""
	for card in _round.table_cards:
		sig += "%d," % Card.id_for(card.point, card.suit)
	if sig == _table_render_sig and not _table_anim_pending:
		return

	_dissolve_old_table_cards()
	_table_cards_anim.clear()
	_table_render_sig = sig
	if _round.table_cards.is_empty():
		return

	var animate := _table_anim_pending
	_table_anim_pending = false
	for card in _round.table_cards:
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


func _run_table_anim() -> void:
	_table_anim_running = true

	var cw: float = _mobile_card_w if _is_mobile else CARD_W
	var ch: float = _mobile_card_h if _is_mobile else CARD_H
	var half := Vector2(cw, ch) * 0.5
	var n := _table_cards_anim.size()

	var src_ok := _play_src_global != Vector2.ZERO
	if not src_ok:
		push_warning("PLAY_ANIM FALLBACK: _play_src_global is ZERO")
		for real in _table_cards_anim:
			if is_instance_valid(real):
				real.modulate.a = 1.0
		_table_anim_running = false
		return

	## 等待嵌套容器（MainVBox→TableArea→CenterBox→TableCards）布局稳定
	## 不做 reset_size()：瞬时归零 + 多层逐帧恢复 = 不可预测竞态
	## 改用验证循环：容器尺寸达标后才读取坐标
	var ok := false
	for _retry in 8:
		await get_tree().process_frame
		if _table_cards.size.x >= cw and _table_cards.size.y >= ch:
			ok = true
			break
	if not ok:
		push_warning("PLAY_ANIM: container layout timeout size=%s, using stale geometry" % _table_cards.size)

	## 基于容器几何直接计算每张牌的落点，不依赖子节点 global_position
	var sep := float(_table_cards.get_theme_constant("separation"))
	var total_child_w := float(n) * cw + float(n - 1) * sep
	var container_size := _table_cards.size
	var container_pos := _table_cards.global_position

	var total := 0.0
	for i in n:
		var real: Control = _table_cards_anim[i]
		if not is_instance_valid(real):
			continue
		if i >= _round.table_cards.size():
			continue

		var start_x := (container_size.x - total_child_w) * 0.5  ## alignment = CENTER
		var target := container_pos + Vector2(
			start_x + cw * 0.5 + float(i) * (cw + sep),
			container_size.y * 0.5
		)
		var card: Card = _round.table_cards[i]

		var ghost := CardVisual.new(Vector2(cw, ch))
		ghost.show_texture(CardVisual.texture_for_id(Card.id_for(card.point, card.suit)))
		ghost.mouse_filter = Control.MOUSE_FILTER_IGNORE
		ghost.z_index = 100
		ghost.pivot_offset = half
		_anim_overlay.add_child(ghost)
		ghost.global_position = _play_src_global - half
		ghost.scale = Vector2(1.18, 1.18)
		ghost.rotation = deg_to_rad(-20.0)

		var mid := (_play_src_global + target) * 0.5 + Vector2(0, -PLAY_ANIM_ARC)
		var delay := float(i) * PLAY_ANIM_STAGGER

		var move_tw := ghost.create_tween()
		move_tw.tween_interval(delay)
		move_tw.tween_property(ghost, "global_position", mid - half, PLAY_ANIM_DURATION * 0.45) \
			.set_ease(Tween.EASE_OUT).set_trans(Tween.TRANS_QUAD)
		move_tw.tween_property(ghost, "global_position", target - half, PLAY_ANIM_DURATION * 0.55) \
			.set_ease(Tween.EASE_IN).set_trans(Tween.TRANS_QUAD)

		var fx_tw := ghost.create_tween()
		fx_tw.tween_interval(delay)
		fx_tw.set_parallel(true)
		fx_tw.tween_property(ghost, "scale", Vector2.ONE, PLAY_ANIM_DURATION)
		fx_tw.tween_property(ghost, "rotation", 0.0, PLAY_ANIM_DURATION)

		var land := delay + PLAY_ANIM_DURATION
		get_tree().create_timer(land).timeout.connect(_on_ghost_landed.bind(real, ghost))

		total = max(total, land)

	await get_tree().create_timer(total + 0.02).timeout
	_table_anim_running = false
	if _table_anim_pending and is_instance_valid(_round):
		_refresh_table()


func _on_ghost_landed(real: Control, ghost: Control) -> void:
	if is_instance_valid(real):
		real.modulate.a = 1.0
	if is_instance_valid(ghost):
		ghost.dissolve(DISSOLVE_DURATION * 0.6)


func _refresh_hand() -> void:
	_refresh_hand_smooth()


func _refresh_turn() -> void:
	if _finished:
		return
	if not _round.table_cards.is_empty():
		return
	var cur := _round.whose_turn()
	if cur == _human:
		_round_lbl.text = "你的回合 — 选牌出牌"
		_round_lbl.add_theme_color_override("font_color", COL_GREEN.lightened(0.25))
	else:
		_round_lbl.text = "%s 回合" % _ai.name
		_round_lbl.add_theme_color_override("font_color", COL_CRIMSON.lightened(0.15))


func _refresh_status(last_action: String) -> void:
	## 状态栏已删除：仅在有关键动作时临时覆盖回合标签显示
	if not last_action.is_empty():
		_round_lbl.text = last_action
		_round_lbl.add_theme_color_override("font_color", COL_GOLD)
		_log(last_action)


func _refresh_top() -> void:
	_title_lbl.text = _ai.name
	_deck_count_lbl.text = "第 %d 关 · 筹码 %d ｜ 我方 手牌%d/牌堆%d ｜ 对方 手牌%d/牌堆%d" % \
		[_campaign.current_level, _campaign.player_chips,
		_human.normal_hand.size(), _human_deck.total(),
		_ai.normal_hand.size(), _ai_deck.total()]
	_hp_player_lbl.text = "你 %d/%d HP" % [_human_hp, _campaign.config.player_max_hp]
	_hp_boss_lbl.text = "对方 %d/%d HP" % [_ai_hp, _campaign.config.boss_max_hp]
	if is_instance_valid(_player_deck_lbl):
		_player_deck_lbl.text = "牌堆\n%d" % _human_deck.total()
	if is_instance_valid(_boss_deck_lbl):
		_boss_deck_lbl.text = "牌堆\n%d" % _ai_deck.total()


func _refresh(last_action: String) -> void:
	if not is_instance_valid(_round):
		return
	_refresh_top()
	_refresh_table()
	_refresh_boss_hand()
	_refresh_hand()
	_refresh_turn()
	_refresh_status(last_action)