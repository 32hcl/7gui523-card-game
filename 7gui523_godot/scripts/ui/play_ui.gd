extends "res://scripts/ui/play_anim.gd"
class_name PlayUI

## — 第 3 层 play_ui：关末商店
##
## 商店面板由 ShopPanel 场景（所见即所得）渲染，购买/刷新委托给 play_ui。

const SHOP_SCENE := preload("res://scenes/shop/shop_panel.tscn")
const SHOP_SCRIPT := preload("res://scripts/ui/shop_panel.gd")

var _shop_panel: Control = null

## 由 play.gd 实现的关卡推进钩子。
func _on_next_level() -> void:
	push_error("_on_next_level() must be overridden by subclass")


func _show_shop_panel() -> void:
	if _shop_panel == null:
		var root := SHOP_SCENE.instantiate()
		root.set_script(SHOP_SCRIPT)
		_shop_panel = root
		_shop_panel.item_buy_requested.connect(_on_buy_item)
		_shop_panel.enchant_buy_requested.connect(_on_buy_enchant)
		_shop_panel.close_requested.connect(_hide_shop_panel)
		_shop_panel.next_level_requested.connect(_on_next_level)
		_shop_panel.refresh_requested.connect(_on_shop_refresh)
		add_child(_shop_panel)
	_shop_panel.visible = true
	_refresh_shop_panel()


func _hide_shop_panel() -> void:
	if _shop_panel:
		_shop_panel.visible = false


func _refresh_shop_panel() -> void:
	if not _shop_panel or not _shop_panel.visible:
		return
	_shop_panel.populate(
		_state.shop.get_fixed_items(),
		_state.shop.get_refreshable_items(),
		_state.shop.enchant_items,
		_state.campaign.player_chips,
		_state.shop.can_enter_enchant_shop(),
		_state.shop.level_spent_chips,
		_state.shop.get_enchant_unlock_threshold(),
	)


func _on_buy_item(item: Shop.ShopItem) -> void:
	var res: Shop.PurchaseResult = _state.shop.buy(item, _state.campaign.player_chips)
	if not res.success:
		_refresh_shop_panel()
		return

	_state.campaign.player_chips -= item.price

	if res.card != null:
		res.card.copy_mark = _state.human.next_copy_mark(res.card.id)
		match res.dest:
			Shop.CardDest.TO_HAND:
				## 留到下关开牌时进入手牌；当前关的手牌会在下关重建
				_state.campaign.pending_hand_cards.append(res.card)
			Shop.CardDest.TO_PLAY_DECK:
				_state.human_deck.add_to_hidden(res.card)
			Shop.CardDest.TO_HIDDEN:
				_state.human_deck.add_to_hidden(res.card)
			_:
				_state.human_deck.add_to_hidden(res.card)

	_refresh_shop_panel()
	_refresh_top()


func _on_buy_enchant(index: int) -> void:
	var target: Card = null
	for c in _state.human.normal_hand:
		if c.is_joker():
			target = c
			break
	if target == null:
		for id in _state.human_deck.cards:
			var c := Card.from_id(id)
			if c.is_joker():
				target = c
				break
	if target == null:
		return

	var res: Enchant.EnchantPurchaseResult = _state.shop.enchant_buy(index, _state.campaign.player_chips, target)
	if not res.success:
		_refresh_shop_panel()
		return

	_state.campaign.player_chips -= res.enchant.cost_chips
	_refresh_shop_panel()
	_refresh_top()


func _on_shop_refresh() -> void:
	if _state.campaign.player_chips < Shop.REFRESH_COST:
		return
	_state.campaign.player_chips -= Shop.REFRESH_COST
	_state.shop.refresh()
	_refresh_shop_panel()
	_refresh_top()