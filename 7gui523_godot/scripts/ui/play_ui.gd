extends PlayAnim
class_name PlayUI

## — 第 3 层 play_ui：关末商店
##
## 商店面板由 ShopPanel 场景（所见即所得）渲染，购买/刷新委托给 play_ui。

const SHOP_SCENE := preload("res://scenes/shop/shop_panel.tscn")
const SHOP_SCRIPT := preload("res://scripts/ui/shop_panel.gd")

var _shop_panel: Control = null


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


func _on_next_level() -> void:
	_hide_shop_panel()


func _refresh_shop_panel() -> void:
	if not _shop_panel or not _shop_panel.visible:
		return
	_shop_panel.populate(
		_shop.get_fixed_items(),
		_shop.get_refreshable_items(),
		_shop.enchant_items,
		_campaign.player_chips,
		_shop.can_enter_enchant_shop(),
		_shop.level_spent_chips,
	)


func _on_buy_item(item_type: int) -> void:
	var res := _shop.buy(item_type, _campaign.player_chips, _campaign.player_hp, _campaign.config.player_max_hp)
	if not res.success:
		_log("购买失败：%s" % res.message)
		_refresh_shop_panel()
		return

	var price: int = Shop.BASE_PRICE.get(item_type, 0)
	_campaign.player_chips -= price

	if res.hp_healed > 0:
		_campaign.player_hp = mini(_campaign.player_hp + res.hp_healed, _campaign.config.player_max_hp)
		_human_hp = _campaign.player_hp
	elif res.card != null:
		match res.dest:
			Shop.CardDest.TO_HAND:
				if _human.normal_hand.size() < Player.PlayerData.NORMAL_MAX:
					_human.normal_hand.append(res.card)
				else:
					_human_deck.add_to_hidden(res.card)
			Shop.CardDest.TO_PLAY_DECK:
				_human_deck.add_to_hidden(res.card)
			Shop.CardDest.TO_HIDDEN:
				_human_deck.add_to_hidden(res.card)
			_:
				_human_deck.add_to_hidden(res.card)

	_log("✅ %s" % res.message)
	_refresh_shop_panel()
	_refresh_top()


func _on_buy_enchant(index: int) -> void:
	var target: Card = null
	for c in _human.normal_hand:
		if c.is_joker():
			target = c
			break
	if target == null:
		for id in _human_deck.cards:
			var c := Card.from_id(id)
			if c.is_joker():
				target = c
				break
	if target == null:
		_log("附魔失败：你没有小丑牌（大鬼/小鬼）")
		return

	var res := _shop.enchant_buy(index, _campaign.player_chips, target)
	if not res.success:
		_log("附魔失败：%s" % res.message)
		_refresh_shop_panel()
		return

	_campaign.player_chips -= res.enchant.cost_chips
	_log("✨ %s" % res.message)
	_refresh_shop_panel()
	_refresh_top()


func _on_shop_refresh() -> void:
	if _campaign.player_chips < Shop.REFRESH_COST:
		_log("筹码不足，无法刷新")
		return
	_campaign.player_chips -= Shop.REFRESH_COST
	_shop.refresh(_campaign.player_chips)
	_log("已刷新可刷新区（-%d 筹码）" % Shop.REFRESH_COST)
	_refresh_shop_panel()
	_refresh_top()