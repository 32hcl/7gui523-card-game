class_name CardEvent
extends Node

##键值到数字的映射
const KEY_TO_NUM = {
	KEY_0: 0, KEY_1: 1, KEY_2: 2, KEY_3: 3, KEY_4: 4,
	KEY_5: 5, KEY_6: 6, KEY_7: 7, KEY_8: 8, KEY_9: 9,
	KEY_KP_0: 0, KEY_KP_1: 1, KEY_KP_2: 2, KEY_KP_3: 3, KEY_KP_4: 4,
	KEY_KP_5: 5, KEY_KP_6: 6, KEY_KP_7: 7, KEY_KP_8: 8, KEY_KP_9: 9
}

signal dragging_right_click(event: InputEvent)
signal dragging_left_click(event: InputEvent)
signal dragging_middle_click(event: InputEvent)
signal dragging_wheel_up(event: InputEvent)
signal dragging_wheel_down(event: InputEvent)

var dragging_count: int = 0
var cards_on_mouse: int = 0

##卡牌相关信息
var cards_dragging: Array = []
var cards_displaying: Array = []

##牌桌相关信息
var hand  ## HandTable


func _input(event: InputEvent) -> void:
	##鼠标点击事件
	_cards_dragging_input(event)
	_cards_selected_input(event)
	##键盘点击事件
	_cards_hand_input(event)


func _cards_dragging_input(event: InputEvent) -> void:
	if cards_dragging.size() <= 0: return
	if event is InputEventMouseButton:
		match event.button_index:
			MOUSE_BUTTON_RIGHT: dragging_right_click.emit(event)
			MOUSE_BUTTON_LEFT: dragging_left_click.emit(event)
			MOUSE_BUTTON_MIDDLE: dragging_middle_click.emit(event)
			MOUSE_BUTTON_WHEEL_UP: dragging_wheel_up.emit(event)
			MOUSE_BUTTON_WHEEL_DOWN: dragging_wheel_down.emit(event)


func _cards_selected_input(event: InputEvent) -> void:
	##当有拖拽和展示牌、没有牌被选择时，不处理该事件
	if get_tree().get_nodes_in_group("selected").size() <= 0: return
	if (cards_displaying.size() > 0 || cards_dragging.size() > 0): return
	if event is InputEventMouseButton:
		match event.button_index:
			MOUSE_BUTTON_LEFT: 
				##只有鼠标在被选择的卡牌上时，才会处理点击事件
				var mouse_on_card := false
				for card in get_tree().get_nodes_in_group("selected"): 
					if card.mouse_on_card: mouse_on_card = true; break
				
				if not mouse_on_card: return
				get_tree().get_first_node_in_group("dragger").dirty_left_released = true
				for card in get_tree().get_nodes_in_group("selected"): 
					card.selected_left_click.emit(event)
			_:
				return


func _cards_hand_input(event: InputEvent) -> void:
	if not is_instance_valid(hand): return
	if event is InputEventKey:
		if not event.is_pressed() or event.echo: return
		##处理数字按键 
		if event.keycode in KEY_TO_NUM:
			var number = KEY_TO_NUM[event.keycode]
			hand.num_key_input(number)
			get_viewport().set_input_as_handled()
			return
		##处理空格按键
		if event.keycode == KEY_SPACE:
			hand.space_input(event)
			get_viewport().set_input_as_handled()
			return
