extends Control

## 阶段 B — 牌堆循环 & 手牌区
##
## 用"完全顺序有序"牌堆(0-53)直观验证单数组牌堆的循环逻辑：
##   抽牌  remove_at(0)              GE 左端真实移出游戏牌堆
##   出牌  push_back                 追加到隐藏牌堆尾部
##   补牌  active_count++            隐藏牌堆头部并入游戏牌堆
##   买卡  insert(随机位)            打断 LIFO
## 牌流方向: [GE←游戏区|空隙|隐藏区→HE]，抽口在左端
## 分界线两侧的牌序都是"看得到"的，任何错误一眼可辨。

const CARD_SIZE := Vector2(56, 79)   ## 牌堆缩略卡尺寸
const WIN_W := 88                    ## 正面缩略卡显示宽度
const MAX_LOG := 200                 ## 日志最多保留条数

@onready var _deck := Deck.DeckManager.new()
@onready var _player := Player.PlayerData.new()

var _log_lines: Array[String] = []   ## 日志行缓冲（限 MAX_LOG 条）

@onready var _active_row: HBoxContainer = $Main/DeckPane/VSplit/ActiveScroll/Box/ActiveRow
@onready var _hidden_row: HBoxContainer = $Main/DeckPane/VSplit/HiddenScroll/Box/HiddenRow
@onready var _active_count_lbl: Label = $Main/DeckPane/ActiveCount
@onready var _hand_row: HBoxContainer = $Main/HandPane/HandRow
@onready var _log_lbl: RichTextLabel = $Main/LogPane/LogText
@onready var _status_lbl: Label = $Main/Status


func _ready() -> void:
	_reset()


## 重置：回到完全顺序牌堆 + 空手牌 + 干净日志
func _reset() -> void:
	_deck.init_ordered()
	_player.normal_hand.clear()
	_deck.fill_player_normal(_player)
	_log_lines.clear()
	if is_instance_valid(_log_lbl):
		_log_lbl.clear()
	_log("重置: 完全顺序牌堆 0-53，手牌补满 5 张")
	_refresh()


## 重置按钮
func _on_reset_pressed() -> void:
	_reset()


# ═══════════════════════════════════════════════════════════════
# 控制按钮
# ═══════════════════════════════════════════════════════════════

## 抽 1 张 → 游戏牌堆  补牌+1 → 手牌
func _on_draw_pressed() -> void:
	if _player.normal_hand.size() >= GameConfig.NORMAL_MAX:
		_log("⚠ 手牌已满 (%d/5)，请先出牌再抽" % _player.normal_hand.size())
		return
	if _deck.active_size() <= 0:
		_log("⚠ 游戏牌堆已空，无法抽牌")
		return
	var drawn := _deck.draw_from_active(1)
	if drawn.is_empty():
		return
	_player.normal_hand.append(drawn[0])
	_log("抽得: %s （真实移出游戏牌堆顶部）" % drawn[0])
	_refresh()


## 顺手牌首张 → 追加隐藏牌堆尾部（出牌回收）
func _on_play_pressed() -> void:
	if _player.normal_hand.is_empty():
		_log("⚠ 无手牌可出")
		return
	var top: Card = _player.normal_hand.pop_front()
	_deck.add_to_hidden(top)
	_log("出牌: %s （追加隐藏牌堆尾部）" % top)
	_refresh()


## 补牌：预设目标 32，分界线右移，隐藏头部并入游戏区
func _on_refill_pressed() -> void:
	var old: int = _deck.active_size()
	_deck.refill_active()
	var moved: int = _deck.active_size() - old
	_log("补牌: 隐藏牌堆头部 %d 张并入游戏牌堆" % moved)
	_refresh()


## 商店：抽 7 → 销毁中间那张 → 其余随机回隐藏 → 补牌
func _on_shop_pressed() -> void:
	var res: Variant = _deck.shop_destroy_flow(3)
	if res.shown.is_empty():
		_log("⚠ 游戏牌堆不足 7 张，无法商店")
		return
	_log("商店: 展示7张，销毁 %s；其余 %d 张随机回隐藏牌堆" % [res.destroyed, res.remaining.size()])
	_refresh()


# ═══════════════════════════════════════════════════════════════
# 刷新视图
# ═══════════════════════════════════════════════════════════════

func _refresh() -> void:
	_clear_children(_active_row)
	_clear_children(_hidden_row)
	_clear_children(_hand_row)

	var cards := _deck.cards
	var active: int = _deck.active_size()

	# 游戏牌堆（0..active-1）
	for i in active:
		_active_row.add_child(_mini_card(cards[i], true))
	# 隐藏牌堆（active..末）
	for i in range(active, cards.size()):
		_hidden_row.add_child(_mini_card(cards[i], false))

	# 手牌
	for card in _player.normal_hand:
		_hand_row.add_child(_face_card(card))

	_active_count_lbl.text = "游戏牌堆 %d 张  /  隐藏牌堆 %d 张  /  合计 %d 张" % \
		[active, cards.size() - active, cards.size()]

	_status_lbl.text = "手牌 %d/5 张 — 牌堆循环正常" % _player.normal_hand.size()


## 一个牌堆缩略卡：正面牌面 + 下方 ID 数字
func _mini_card(id: int, is_active: bool) -> Control:
	var box := VBoxContainer.new()
	box.custom_minimum_size = Vector2(WIN_W, 79 + 16)

	var visual := CardVisual.new(CARD_SIZE)
	visual.show_id(id)
	visual.mouse_filter = Control.MOUSE_FILTER_IGNORE
	box.add_child(visual)

	var lbl := Label.new()
	lbl.text = str(id)
	lbl.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	lbl.add_theme_font_size_override("font_size", 11)
	lbl.modulate = Color.WHITE if is_active else Color(0.7, 0.8, 1.0)
	lbl.custom_minimum_size = Vector2(WIN_W, 16)
	box.add_child(lbl)

	return box


## 一个纯正面手牌卡
func _face_card(card: Card) -> Control:
	var visual := CardVisual.new()
	visual.show_texture(CardVisual.texture_for_id(Card.id_for(card.point, card.suit)))
	return visual


## 清空一个容器的所有子节点
func _clear_children(node: Node) -> void:
	for child in node.get_children():
		child.queue_free()


func _log(msg: String) -> void:
	_log_lines.append(msg)
	while _log_lines.size() > MAX_LOG:
		_log_lines.pop_front()
	_log_lbl.clear()
	for line in _log_lines:
		_log_lbl.append_text("> " + line + "\n")
	_scroll_log_to_bottom.call_deferred()


func _scroll_log_to_bottom() -> void:
	if not is_instance_valid(_log_lbl):
		return
	var scroll := _log_lbl.get_parent()
	if scroll is ScrollContainer:
		scroll.scroll_vertical = int(scroll.get_v_scroll_bar().max_value)