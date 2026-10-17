extends PlayBase
class_name PlayAnim

## — 第 2 层 play_anim：动画 · 手牌渲染 · 桌面渲染
##
## 手牌增量渲染、发牌动画、桌面飞行/溶解动画、BOSS 手牌、全部视图刷新。

func _render_hand_block(hand: Array, anchor_x: float, rot_scale: float, tint: Color,
		idx_base: int, id_prefix: String, z_base: int,
		by_id: Dictionary, animate: bool, deal_from: Vector2, base_delay: int, overall: int = 0) -> void:
	var order := _sorted_order(hand)
	var m := hand.size()
	var rot_eff := 1.0 if overall > 0 else rot_scale
	for p in m:
		var real: int = order[p]
		var key: int = idx_base + real
		var card: Card = hand[real]
		var cid := "%s%d" % [id_prefix, Card.id_for(card.point, card.suit)]
		var v: CardVisual = by_id.get(cid, null)
		var sel := _selected.has(key)
		if v == null:
			v = _hand_card(card, key, sel)
			v.set_meta("card_id", cid)
			_player_hand.add_child(v)
		else:
			by_id.erase(cid)
		## 记录稳定全局索引（点击/选中/拖拽按它命中，而非 children 顺序）
		v.set_meta("hand_index", key)
		v.kill_layout_tween()
		var is_new := not v.has_meta("_faded_in")
		if animate or is_new:
			v.set_meta("_faded_in", true)
			if not animate:
				v.modulate.a = 0.0
				var tin := v.create_tween()
				tin.tween_property(v, "modulate:a", 1.0, 0.15)

		var target := _fan_target(overall, m, p, idx_base + p, anchor_x, rot_eff, key)
		var z_rot: float = target["rot"]
		var target_scale := Vector2(SELECTED_SCALE, SELECTED_SCALE) if sel else Vector2.ONE
		v.mouse_filter = Control.MOUSE_FILTER_STOP
		v.set_selected(sel)
		v.modulate = tint if not sel else Color(1.0, 1.0, 1.0, 1.0)
		var layer_z := z_base + p
		v.base_z = layer_z
		v.z_index = 100 if v.get_meta("_hovered", false) else layer_z

		if animate:
			_deal_card_anim(v, deal_from, target["pos"], z_rot,
				float(base_delay + p) * DEAL_DELAY, sel, card, tint)
		else:
			v.show_texture(CardVisual.texture_for_id(Card.id_for(card.point, card.suit)))
			v.enable_fake3d(true)
			v.set_fake3d_rot(0.0, 0.0)
			if is_new:
				v.position = deal_from
			var tw := v.create_tween()
			tw.set_ease(Tween.EASE_OUT).set_trans(Tween.TRANS_CUBIC)
			tw.set_parallel(true)
			tw.tween_property(v, "position", target["pos"], LAYOUT_TWEEN)
			tw.tween_property(v, "rotation", deg_to_rad(z_rot), LAYOUT_TWEEN)
			tw.tween_property(v, "scale", target_scale, LAYOUT_TWEEN)
			v._layout_tween = tw


## 手牌增量刷新：按 card_id 增量，增删有 layout 过渡
func _refresh_hand_smooth() -> void:
	if not is_instance_valid(_round):
		return
	var by_id := {}
	for child in _player_hand.get_children():
		if child is CardVisual and not child.is_queued_for_deletion() and child.has_meta("card_id"):
			by_id[child.get_meta("card_id")] = child

	var n := _human.normal_hand.size()
	var m := _human.instant_hand.size()
	var met := _hand_metrics(n, m)

	var deal_from: Vector2 = _player_hand.get_global_transform_with_canvas().affine_inverse() \
		* (_player_deck.global_position + _player_deck.size * 0.5)

	## 7 张整体连续扇形（统一弧面）+ INSTANT_GAP 标注 5+2；补区保留淡蓝身份
	_render_hand_block(_human.normal_hand, met["start_x"], 1.0, Color.WHITE, 0, "", 10, by_id, false, deal_from, 0, met["total"])
	_render_hand_block(_human.instant_hand, met["instant_x"], 0.5, INSTANT_TINT, n, "i_", 30, by_id, false, deal_from, 0, met["total"])

	## 消失的卡（已打出）：直接释放，无动画，避免残影
	for cid in by_id:
		var dead: CardVisual = by_id[cid]
		if is_instance_valid(dead):
			dead.queue_free()


## 刷新手牌（animate 开头发牌飞入）
func _refresh_hand_with_animation(animate: bool) -> void:
	for child in _player_hand.get_children():
		child.queue_free()

	var n := _human.normal_hand.size()
	var m := _human.instant_hand.size()
	if n == 0 and m == 0:
		return

	var met := _hand_metrics(n, m)
	## 抽卡起点：从玩家牌堆中心飞入手牌区（转成 _player_hand 局部坐标）
	## Control 无 to_local（Node2D 专属），改用全局画布变换求逆
	var deal_from: Vector2 = _player_hand.get_global_transform_with_canvas().affine_inverse() \
		* (_player_deck.global_position + _player_deck.size * 0.5)
	## 开局重建，by_id 为空；7 张整体扇形，普通区与补区都从牌堆中心飞出
	var empty_ids := {}
	_render_hand_block(_human.normal_hand, met["start_x"], 1.0, Color.WHITE, 0, "", 10, empty_ids, animate, deal_from, 0, met["total"])
	_render_hand_block(_human.instant_hand, met["instant_x"], 0.5, INSTANT_TINT, n, "i_", 30, empty_ids, animate, deal_from, n, met["total"])


func _deal_card_anim(visual: CardVisual, from_pos: Vector2, target_pos: Vector2,
		z_rot: float, delay: float, selected_after: bool, card: Card, tint: Color = Color.WHITE) -> void:
	var card_tex := CardVisual.texture_for_id(Card.id_for(card.point, card.suit))
	visual.show_texture(card_tex)
	visual.enable_fake3d(true)
	visual.set_selected(selected_after)
	visual.position = from_pos
	visual.rotation = deg_to_rad(z_rot) * 0.5
	visual.modulate = tint
	visual.modulate.a = 0.0
	visual.set_fake3d_rot(0.0, 0.0)
	var tw := visual.create_tween()
	tw.set_ease(Tween.EASE_OUT).set_trans(Tween.TRANS_CUBIC)
	tw.tween_interval(delay)
	tw.parallel().tween_property(visual, "position", target_pos, DEAL_DURATION)
	tw.parallel().tween_property(visual, "rotation", deg_to_rad(z_rot), DEAL_DURATION)
	tw.parallel().tween_property(visual, "modulate:a", 1.0, DEAL_DURATION * 0.5)
	if selected_after:
		tw.parallel().tween_property(visual, "scale", Vector2(SELECTED_SCALE, SELECTED_SCALE), DEAL_DURATION)
	visual._layout_tween = tw


## BOSS 手牌渲染（全牌背）
func _refresh_boss_hand() -> void:
	_refresh_boss_hand_with_animation(false)


func _refresh_boss_hand_with_animation(animate: bool) -> void:
	var n := _ai.normal_hand.size()
	var m := _ai.instant_hand.size()
	var total := n + m
	var exist: Array[CardVisual] = []
	for child in _boss_hand.get_children():
		if child is CardVisual and not child.is_queued_for_deletion():
			exist.append(child)
	## 差异回收：多出的牌背释放
	while exist.size() > total:
		exist.pop_back().queue_free()
	## 差异补建：缺少的牌背新加（牌背相同，无前后衔接）
	while exist.size() < total:
		var cv := CardVisual.new(Vector2(BOSS_CARD_W, BOSS_CARD_H))
		cv.show_back()
		cv.enable_fake3d(false)
		cv.mouse_filter = Control.MOUSE_FILTER_IGNORE
		_boss_hand.add_child(cv)
		exist.append(cv)
	if total == 0:
		return

	## 布局：普通区(5)与立刻补区(2)同高排列
	var container_w := _boss_hand.size.x
	var card_w := BOSS_CARD_W
	var gap := BOSS_GAP
	var normal_w := float(n) * card_w + float(maxi(n - 1, 0)) * gap
	var instant_w := float(m) * card_w + float(maxi(m - 1, 0)) * gap
	var all_w := normal_w + (BOSS_INSTANT_GAP if m > 0 else 0.0) + instant_w
	var start_x: float = (container_w - all_w) * 0.5 if container_w > all_w else 0.0
	var instant_x := start_x + normal_w + (BOSS_INSTANT_GAP if (m > 0 and n > 0) else 0.0)

	## 抽卡起点：从 BOSS 牌堆中心飞入手牌区
	var deal_from: Vector2 = _boss_hand.get_global_transform_with_canvas().affine_inverse() \
		* (_boss_deck.global_position + _boss_deck.size * 0.5)

	## 逐张对位到目标位置；仅开局(animate)时新卡从牌堆飞入
	for i in total:
		var cv: CardVisual = exist[i]
		var x: float = start_x + float(i) * (card_w + gap) if i < n \
			else instant_x + float(i - n) * (card_w + gap)
		if animate:
			cv.position = deal_from
			cv.modulate.a = 0.0
			var tw := cv.create_tween()
			tw.set_ease(Tween.EASE_OUT).set_trans(Tween.TRANS_CUBIC)
			tw.tween_interval(float(i) * DEAL_DELAY)
			tw.parallel().tween_property(cv, "position", Vector2(x, 0.0), DEAL_DURATION)
			tw.parallel().tween_property(cv, "modulate:a", 1.0, DEAL_DURATION * 0.4)
		elif cv.position != Vector2(x, 0.0):
			var tw := cv.create_tween()
			tw.set_ease(Tween.EASE_OUT).set_trans(Tween.TRANS_CUBIC)
			tw.tween_property(cv, "position", Vector2(x, 0.0), LAYOUT_TWEEN)


func _refresh_table() -> void:
	var par := CardTypeResult.parse_any_card_type(_round.table_cards)
	if par != null and par.type != CardTypeResult.CardType.INVALID:
		_round_lbl.text = CardTypeNames.name_for(par.type)

	if _table_anim_running:
		return

	_dissolve_old_table_cards()
	_table_cards_anim.clear()
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

	_table_cards.reset_size()
	await get_tree().process_frame
	await get_tree().process_frame
	if not is_instance_valid(self) or not _table_anim_running:
		return

	var half := Vector2(CARD_W, CARD_H) * 0.5
	var n := _table_cards_anim.size()
	var total := 0.0
	for i in n:
		var real: Control = _table_cards_anim[i]
		if not is_instance_valid(real):
			continue
		if i >= _round.table_cards.size():
			continue
		var card: Card = _round.table_cards[i]

		var target := real.global_position + real.size * 0.5

		var ghost := CardVisual.new(Vector2(CARD_W, CARD_H))
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
	else:
		_round_lbl.text = "%s 回合" % _ai.name


func _refresh_status(last_action: String) -> void:
	var cur := _round.whose_turn()
	var who := "你的回合" if cur == _human else "%s 回合" % _ai.name
	var extra := ("" if last_action.is_empty() else " ｜ %s" % last_action)
	_hand_type_lbl.text = "手牌 %d/5 · %s%s" % [_human.normal_hand.size(), who, extra]
	if not last_action.is_empty():
		_log(last_action)


func _refresh_top() -> void:
	_title_lbl.text = _ai.name
	_deck_count_lbl.text = "第 %d 关 · 筹码 %d | 我方 手牌 %d·牌堆 %d | 对方 手牌 %d·牌堆 %d" % \
		[_campaign.current_level, _campaign.player_chips,
		_human.normal_hand.size(), _human_deck.total(),
		_ai.normal_hand.size(), _ai_deck.total()]
	_hp_lbl.text = "◆ 你 %d/%d HP · 对方 %d/%d HP" % \
		[_human_hp, _campaign.config.player_max_hp, _ai_hp, _campaign.config.boss_max_hp]
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