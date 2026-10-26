extends "res://scripts/ui/play_ui.gd"
class_name PlayInput

## 第 4 层 play_input：输入入口与拖拽过程更新。

const DOUBLE_CLICK_MSEC := 300
var _last_table_click_msec := 0
var _last_table_click_pos := Vector2.ZERO

## 由 play.gd 提供的回合操作钩子。
func _on_pass_pressed() -> void:
	pass

func _do_play(_indices: Array[int]) -> bool:
	return false

## 由 play.gd 提供的弃牌钩子。
## immediate_draw=true  → 方式一：拖到牌堆，扣筹码 + 立即补牌
## immediate_draw=false → 方式二：选中+双击桌面，免费，下回合补牌
func _discard_cards(_indices: Array[int], _immediate_draw: bool) -> bool:
	return false

func _input(event: InputEvent) -> void:
	if event is InputEventMouseButton and event.pressed and event.button_index == MOUSE_BUTTON_LEFT \
			and not _dragging and not _drag_pending:
		var table_rect: Rect2 = _table_interact_rect()
		var hand_rect: Rect2 = _hand_area.get_global_rect()
		if table_rect.has_point(event.global_position) and not hand_rect.has_point(event.global_position) \
				and not _point_over_table_controls(event.global_position):
			var now: int = Time.get_ticks_msec()
			var dist: float = event.global_position.distance_to(_last_table_click_pos)
			if (now - _last_table_click_msec) < DOUBLE_CLICK_MSEC and dist < 44.0:
				get_viewport().set_input_as_handled()
				_last_table_click_msec = 0
				_last_table_click_pos = Vector2.ZERO
				_on_pass_pressed()
				return
			_last_table_click_msec = now
			_last_table_click_pos = event.global_position
	if not _drag_pending and not _dragging:
		return
	if event is InputEventMouseMotion:
		if _drag_pending and not _dragging and get_global_mouse_position().distance_to(_drag_press_pos) > DRAG_THRESHOLD:
			_drag_start(_drag_first_index)
	elif event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_LEFT and not event.pressed:
		if _dragging:
			get_viewport().set_input_as_handled()
			_drag_drop()
		elif _drag_pending:
			_drag_pending = false
			_on_card_clicked(_drag_first_index)
	elif event is InputEventMouseButton and event.pressed and event.button_index == MOUSE_BUTTON_RIGHT and _dragging:
		get_viewport().set_input_as_handled()
		_drag_cancel()
	elif event.is_action_pressed("ui_cancel") and _dragging:
		_drag_cancel()

func _process(_delta: float) -> void:
	_dream_time += _delta
	for mat in _dream_materials:
		if is_instance_valid(mat):
			mat.set_shader_parameter("time_val", _dream_time)
			mat.set_shader_parameter("spin_time", _dream_time)

	if not _dragging or _drag_visuals.is_empty():
		return
	if _finished or not is_instance_valid(_round) or _round.whose_turn() != _human:
		_drag_cancel()
		return
	var mouse := get_global_mouse_position()
	var table_rect: Rect2 = _table_interact_rect()
	var discard_rect: Rect2 = _player_deck.get_global_rect()
	var boss_deck_rect: Rect2 = _boss_deck.get_global_rect()
	var over := table_rect.has_point(mouse) and not discard_rect.has_point(mouse) and not boss_deck_rect.has_point(mouse)
	var over_discard := discard_rect.has_point(mouse)
	if _drop_hint.visible != over:
		_drop_hint.visible = over
	if _discard_hint.visible != over_discard:
		_discard_hint.visible = over_discard
	if over:
		_drop_hint.global_position = table_rect.position - Vector2(8, 8)
		_drop_hint.size = table_rect.size + Vector2(16, 16)
	if over_discard:
		_discard_hint.global_position = discard_rect.position - Vector2(8, 8)
		_discard_hint.size = discard_rect.size + Vector2(16, 16)
	var n := _drag_visuals.size()
	for i in n:
		var v := _drag_visuals[i]
		if not is_instance_valid(v):
			continue
		var off := Vector2.ZERO
		var rot := 0.0
		if n > 1:
			var t := float(i) / float(n - 1)
			off.x = (t - 0.5) * DRAG_SPREAD
			off.y = -absf(sin(t * PI)) * DRAG_ARC_Y
			rot = lerpf(-DRAG_ROT, DRAG_ROT, t)
		v.global_position = mouse + off - v.size * 0.5
		v.rotation = deg_to_rad(rot)

func _on_card_clicked(index: int) -> void:
	if _finished or _round.whose_turn() != _human:
		return
	var total := _human.normal_hand.size() + _human.instant_hand.size()
	if total <= index:
		return
	if _selected.has(index):
		_selected.erase(index)
	else:
		_selected.append(index)
	_refresh_hand()

func _point_over_table_controls(p: Vector2) -> bool:
	for c: Control in [_boss_deck, _player_deck, _shop_btn, _new_game_btn]:
		if is_instance_valid(c) and c.visible and c.get_global_rect().has_point(p):
			return true
	return false

## 桌面交互区：有桌面牌时取实际牌区；无牌时取桌面中央的牌高范围。
## 不使用整块 CenterBox，避免桌面判定区覆盖手牌。
func _table_interact_rect() -> Rect2:
	var table: Rect2 = $MainVBox/TableArea.get_global_rect()
	## 整个桌面都属于出牌候选区，左上/右下不再有空白死角。
	## 两个牌堆会在拖拽预览和落点判定中单独排除。
	return table

func _drag_start(index: int) -> void:
	_drag_pending = false
	if _finished or not is_instance_valid(_round) or _round.whose_turn() != _human:
		return
	var total := _human.normal_hand.size() + _human.instant_hand.size()
	if index < 0 or total <= index:
		return
	_dragging = true
	_drag_indices.clear()
	_drag_visuals.clear()
	_drag_origin.clear()
	if _selected.has(index) and _selected.size() > 1:
		_drag_indices = _selected.duplicate()
		_drag_indices.sort()
	else:
		_drag_indices = [index]
	for idx in _drag_indices:
		var v: CardVisual = null
		for row: HBoxContainer in [_bottom_hand, _top_hand]:
			for slot in row.get_children():
				if slot.is_queued_for_deletion() or slot.get_child_count() == 0:
					continue
				var ch := slot.get_child(0) as CardVisual
				if ch != null and ch.get_meta("hand_index", -1) == idx:
					v = ch
					break
			if v != null:
				break
		if v == null:
			continue
		_drag_origin.append({"parent": v.get_parent(), "pos": v.position, "rot": v.rotation, "scale": v.scale, "z": v.z_index})
		v.kill_layout_tween()
		v.reparent(_anim_overlay)
		v.z_index = 60
		v.set_fake3d_rot(0.0, 0.0)
		_drag_visuals.append(v)

func _drag_drop() -> void:
	var mouse := get_global_mouse_position()
	if _player_deck.get_global_rect().has_point(mouse):
		_drag_drop_discard()
	elif _boss_deck.get_global_rect().has_point(mouse):
		_drag_cancel()
	elif _hand_area.get_global_rect().has_point(mouse) and _drag_indices.size() == 1:
		_drag_sort(mouse.x)
	elif _table_interact_rect().has_point(mouse):
		_drag_drop_play()
	else:
		_drag_cancel()


func _drag_drop_discard() -> void:
	var indices := _drag_indices.duplicate()
	var saved := _selected.duplicate()
	_selected.clear()
	if not _discard_cards(indices, true):   ## 方式一：拖到牌堆 → 扣筹码 + 立即补牌
		_selected = saved
		_drag_cancel()
		return
	_dismiss_drag_visuals()

func _drag_drop_play() -> void:
	var indices := _drag_indices.duplicate()
	var saved := _selected.duplicate()
	_selected.clear()
	if not _do_play(indices):
		_selected = saved
		_drag_cancel()
		return
	_dismiss_drag_visuals()

func _drag_sort(mouse_x: float) -> void:
	var idx: int = _drag_indices[0]
	var hand: Array[Card] = _human.normal_hand
	if idx < 0 or idx >= hand.size():
		_drag_cancel()
		return
	var refs: Array[Dictionary] = []
	for slot in _bottom_hand.get_children():
		if slot.is_queued_for_deletion() or slot.get_child_count() == 0:
			continue
		var ch := slot.get_child(0) as CardVisual
		if ch == null:
			continue
		var ci: int = ch.get_meta("hand_index", -1)
		if ci < 0 or ci >= hand.size() or ci == idx:
			continue
		refs.append({"index": ci, "cx": ch.global_position.x + ch.size.x * 0.5})
	refs.sort_custom(func(a, b): return a.cx < b.cx)
	if refs.is_empty():
		_drag_cancel()
		return
	var slot := 0
	for r in refs:
		if mouse_x > r.cx:
			slot += 1
	if slot == idx:
		_drag_cancel()
		return
	## 显示顺序此前一直按战力排序，直接按下标插入会和眼睛看到的位置不一致。
	## 所以以「当前显示序列」为基准重排数组，并切到手动排序模式保持住。
	if refs.size() == hand.size() - 1:
		var seq: Array[int] = []
		for r in refs:
			seq.append(int(r["index"]))
		seq.insert(slot, idx)
		var old: Array[Card] = hand.duplicate()
		for k in seq.size():
			hand[k] = old[seq[k]]
	else:
		var card: Card = hand[idx]
		hand.remove_at(idx)
		hand.insert(slot, card)
	_manual_hand_order = true
	_selected.clear()
	_dismiss_drag_visuals()
	_refresh_hand()

func _dismiss_drag_visuals() -> void:
	_dragging = false
	_drop_hint.visible = false
	_discard_hint.visible = false
	for v in _drag_visuals:
		if is_instance_valid(v):
			v.queue_free()
	_drag_visuals.clear()
	_drag_indices.clear()
	_drag_origin.clear()

func _drag_cancel() -> void:
	_dragging = false
	_drop_hint.visible = false
	_discard_hint.visible = false
	for i in _drag_visuals.size():
		var v := _drag_visuals[i]
		if not is_instance_valid(v):
			continue
		var o: Dictionary = _drag_origin[i]
		var cur_global := v.global_position
		if is_instance_valid(o["parent"]):
			v.reparent(o["parent"])
		v.global_position = cur_global
		v.z_index = o["z"]
		var tw := v.create_tween()
		v._layout_tween = tw
		tw.set_ease(Tween.EASE_OUT).set_trans(Tween.TRANS_BACK)
		tw.set_parallel(true)
		tw.tween_property(v, "position", o["pos"], DRAG_RETURN_DUR)
		tw.tween_property(v, "rotation", o["rot"], DRAG_RETURN_DUR)
		tw.tween_property(v, "scale", o["scale"], DRAG_RETURN_DUR)
	_drag_visuals.clear()
	_drag_indices.clear()
	_drag_origin.clear()
