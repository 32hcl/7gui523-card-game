extends "res://scripts/ui/play_ui.gd"
class_name PlayInput

## 第 4 层 play_input：输入入口与拖拽过程更新。

const DOUBLE_CLICK_MSEC := 300
const DOUBLE_CLICK_PX := 44.0   ## 双击判定距离阈值
var _last_table_click_msec := 0
var _last_table_click_pos := Vector2.ZERO

## 由 play.gd 覆盖的回合操作钩子。基类提供安全默认值。
func _on_pass_pressed() -> void:
	push_warning("_on_pass_pressed() should be overridden by subclass")

## 由 play.gd 覆盖的出牌钩子。基类默认拒绝出牌。
func _do_play(_indices: Array[int]) -> bool:
	push_warning("_do_play() should be overridden by subclass")
	return false

## 非法出牌：扣 2 筹码（回合内允许扣成负数，不写入桌面）→
## 牌正面朝上飞桌、激烈晃动 → 飞回手牌 → 仍留在自己回合，可重新选牌再出
func _on_invalid_play(reason: String) -> void:
	_state.human.pay_stake(INVALID_PLAY_PENALTY)
	_state.campaign.player_chips = _state.human.chips
	_refresh_top()
	_spawn_floating_chips(INVALID_PLAY_PENALTY, "违规", true)
	_refresh_status("%s，扣 %d 筹码（当前 %d）" % [reason, INVALID_PLAY_PENALTY, _state.human.chips])
	await _play_invalid_shake_and_return()
	if _state.finished:
		return
	_refresh_hand()
	_refresh_turn()


## 弃牌（两种入口，统一 3 筹码 / 张）
## immediate_draw=true  → 方式一：拖到牌堆，扣筹码并立即从牌堆补等量
## immediate_draw=false → 方式二：选中+双击桌面，扣筹码，下回合正常补牌
func _discard_cards(indices: Array[int], immediate_draw: bool) -> bool:
	if _state.finished or _invalid_anim_running or not is_instance_valid(_state.round) or _state.round.whose_turn() != _state.human:
		return false
	if not _state.player_can_discard:
		_refresh_status("当前不可弃牌")
		return false
	if _state.player_discarded_this_round:
		_refresh_status("本小回合已经弃过牌")
		return false
	if _state.player_discard_count >= PLAYER_DISCARD_LIMIT:
		_refresh_status("本关弃牌额度已用完（%d 张）" % PLAYER_DISCARD_LIMIT)
		return false
	if indices.is_empty() or indices.size() > PLAYER_DISCARD_LIMIT - _state.player_discard_count:
		_refresh_status("弃牌数量超出本关剩余额度")
		return false
	var discard_cards: Array[Card] = []
	for index in indices:
		if index < 0 or index >= _state.human.normal_hand.size():
			_refresh_status("只能弃普通手牌")
			return false
		discard_cards.append(_state.human.normal_hand[index])
	if discard_cards.is_empty():
		return false

	var cost: int = discard_cards.size() * DISCARD_CHIP_COST
	if _state.human.chips < cost:
		_refresh_status("筹码不足（弃 %d 张需要 %d 筹码）" % [discard_cards.size(), cost])
		return false
	_state.human.chips -= cost
	_state.campaign.player_chips = _state.human.chips

	_state.human.remove_cards_from_hand(discard_cards)
	_state.human_deck.recycle_played(discard_cards)
	_state.player_discard_count += discard_cards.size()
	_state.player_discarded_this_round = true

	if immediate_draw:
		_state.human_deck.fill_player_normal(_state.human)
		_refresh("你弃掉 %d 张，花费 %d 筹码，已补牌" % [discard_cards.size(), cost])
	else:
		_refresh("你弃掉 %d 张，花费 %d 筹码（下回合补牌）" % [discard_cards.size(), cost])
	return true

func _input(event: InputEvent) -> void:
	if event is InputEventMouseButton and event.pressed and event.button_index == MOUSE_BUTTON_LEFT \
			and not _dragging and _drag_first_index < 0:
		var hand_rect: Rect2 = _hand_area.get_global_rect()
		if not hand_rect.has_point(event.global_position):
			## 点击不在手牌区 → 清除残留拖拽索引
			_drag_first_index = -1
		var pass_rect: Rect2 = _pass_rect()
		if pass_rect.has_point(event.global_position) and not hand_rect.has_point(event.global_position):
			var now: int = Time.get_ticks_msec()
			var dist: float = event.global_position.distance_to(_last_table_click_pos)
			if (now - _last_table_click_msec) < DOUBLE_CLICK_MSEC and dist < DOUBLE_CLICK_PX:
				get_viewport().set_input_as_handled()
				_last_table_click_msec = 0
				_last_table_click_pos = Vector2.ZERO
				_on_pass_pressed()
				return
			_last_table_click_msec = now
			_last_table_click_pos = event.global_position
		return
	if _drag_first_index < 0 and not _dragging:
		return
	if event is InputEventMouseMotion:
		if _drag_first_index >= 0 and not _dragging and get_global_mouse_position().distance_to(_drag_press_pos) > DRAG_THRESHOLD:
			_drag_start(_drag_first_index)
	elif event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_LEFT and not event.pressed:
		if _dragging:
			get_viewport().set_input_as_handled()
			_drag_drop()
		elif _drag_first_index >= 0:
			var idx := _drag_first_index
			_drag_first_index = -1
			_on_card_clicked(idx)
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
	if _state.finished or not is_instance_valid(_state.round) or _state.round.whose_turn() != _state.human:
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
	if _state.finished or _invalid_anim_running or not is_instance_valid(_state.round) or _state.round.whose_turn() != _state.human:
		return
	var total := _state.human.normal_hand.size() + _state.human.instant_hand.size()
	if total <= index:
		return
	if _state.selected.has(index):
		_state.selected.erase(index)
	else:
		_state.selected.append(index)
	_refresh_hand()

## 桌面交互区（拖放出牌）
func _table_interact_rect() -> Rect2:
	return $MainVBox/TableArea.get_global_rect()

## 双击不要判定区（整块绿毡：Boss区到桌面到手牌底）
func _pass_rect() -> Rect2:
	var mbv := $MainVBox as Control
	var hand: Rect2 = _hand_area.get_global_rect()
	if is_instance_valid(mbv):
		return Rect2(mbv.global_position, Vector2(mbv.size.x, hand.end.y - mbv.global_position.y))
	return _table_interact_rect()

func _drag_start(index: int) -> void:
	if index < 0:
		return
	if _state.finished or _invalid_anim_running or not is_instance_valid(_state.round) or _state.round.whose_turn() != _state.human:
		return
	var total := _state.human.normal_hand.size() + _state.human.instant_hand.size()
	if index < 0 or total <= index:
		return
	_dragging = true
	_drag_indices.clear()
	_drag_visuals.clear()
	_drag_origin.clear()
	if _state.selected.has(index) and _state.selected.size() > 1:
		_drag_indices = _state.selected.duplicate()
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
		_drag_origin.append(DragOrigin.new())
		var o: DragOrigin = _drag_origin[-1]
		o.parent = v.get_parent()
		o.pos = v.position
		o.rot = v.rotation
		o.scale = v.scale
		o.z = v.z_index
		v.kill_layout_tween()
		v.reparent(_anim_overlay)
		v.z_index = 60
		v.set_fake3d_rot(0.0, 0.0)
		## 第二关：背面牌一拖出就翻面（调用翻面动画），拖拽与桌面出牌始终正面朝上
		var card := _hand_card_at(idx)
		if card != null and _state.masked_player_cards.get(card, false):
			## 置 false 而不是 erase：保留“已判定不隐藏”的记录，避免下次刷新重新抽到牌背
			_state.masked_player_cards[card] = false
			_flip_visual_to_front(v, card)
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
	var saved := _state.selected.duplicate()
	_state.selected.clear()
	if not _discard_cards(indices, true):   ## 方式一：拖到牌堆 → 扣筹码 + 立即补牌
		_state.selected = saved
		_drag_cancel()
		return
	_dismiss_drag_visuals()

func _drag_drop_play() -> void:
	var indices := _drag_indices.duplicate()
	var saved := _state.selected.duplicate()
	_state.selected.clear()
	if not _do_play(indices):
		## 非法出牌：扣 2 筹码，牌留在桌上晃动后飞回手牌（不换手、不消耗）
		if not _invalid_reason.is_empty():
			var reason := _invalid_reason
			_invalid_reason = ""
			_on_invalid_play(reason)
			return
		_state.selected = saved
		_drag_cancel()
		return
	_dismiss_drag_visuals()

func _drag_sort(mouse_x: float) -> void:
	var idx: int = _drag_indices[0]
	var hand: Array[Card] = _state.human.normal_hand
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
	_state.manual_hand_order = true
	_state.selected.clear()
	_dismiss_drag_visuals()
	_refresh_hand()

func _dismiss_drag_visuals() -> void:
	_dragging = false
	_drag_first_index = -1
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
	_drag_first_index = -1
	_drop_hint.visible = false
	_discard_hint.visible = false
	for i in _drag_visuals.size():
		var v := _drag_visuals[i]
		if not is_instance_valid(v):
			continue
		var o: DragOrigin = _drag_origin[i]
		var cur_global := v.global_position
		if is_instance_valid(o.parent):
			v.reparent(o.parent)
		v.global_position = cur_global
		v.z_index = o.z
		var tw := v.create_tween()
		v._layout_tween = tw
		tw.set_ease(Tween.EASE_OUT).set_trans(Tween.TRANS_BACK)
		tw.set_parallel(true)
		tw.tween_property(v, "position", o.pos, DRAG_RETURN_DUR)
		tw.tween_property(v, "rotation", o.rot, DRAG_RETURN_DUR)
		tw.tween_property(v, "scale", o.scale, DRAG_RETURN_DUR)
	_drag_visuals.clear()
	_drag_indices.clear()
	_drag_origin.clear()