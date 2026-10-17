class_name Player
extends RefCounted

# ═══════════════════════════════════════════════════════════════
# Player — 命名空间宿主
# 游戏逻辑统一走 Player.PlayerData（见下方嵌套类）
# ═══════════════════════════════════════════════════════════════


# ═══════════════════════════════════════════════════════════════
# PlayerData — 双区手牌玩家数据
# 普通区 上限5张 | 立刻补区 上限2张 | 两区补牌互不干扰
# ═══════════════════════════════════════════════════════════════

class PlayerData extends RefCounted:
	const NORMAL_MAX  := 5        ## 普通区手牌上限
	const INSTANT_MAX := 2        ## 立刻补区手牌上限

	var normal_hand:  Array[Card] = []  ## 普通区手牌
	var instant_hand: Array[Card] = []  ## 立刻补区手牌

	## 兼容旧规则引擎：name / is_human / ai_level
	var name: String = "32%Hcl"
	var is_human := false
	var ai_level := 0

	## 兼容接口：hand 合并 normal_hand + instant_hand，AI/规则引擎统一读取
	var hand: Array[Card]:
		get:
			return normal_hand + instant_hand
		set(value):
			normal_hand = value


	## 普通区补牌：从牌堆抽至 5 张（上限）
	## 已下沉 → Deck.DeckManager.fill_player_normal(player)

	## 立刻补区补牌：从牌堆抽至 2 张（上限）
	## 已下沉 → Deck.DeckManager.fill_player_instant(player)

	## 按 point + suit 精确匹配移除出牌（普通区优先，缺了再去立刻补区删）
	## 规则层 process_play 出牌可能来自两个区，需双向删除
	func remove_cards_from_hand(cards_to_remove: Array[Card]) -> void:
		for card in cards_to_remove:
			var removed := false
			for i in normal_hand.size():
				if normal_hand[i].point == card.point and normal_hand[i].suit == card.suit:
					normal_hand.remove_at(i)
					removed = true
					break
			if removed:
				continue
			for i in instant_hand.size():
				if instant_hand[i].point == card.point and instant_hand[i].suit == card.suit:
					instant_hand.remove_at(i)
					break


	## 手牌排序：等级降序，同级花色升序
	func sort_hand() -> void:
		normal_hand.sort_custom(func(a: Card, b: Card):
			var ra := Card.get_rank(a.point)
			var rb := Card.get_rank(b.point)
			if ra != rb:
				return ra > rb
			return a.suit < b.suit)


	func _to_string() -> String:
		return "普通区:%d/%d张  立刻补区:%d/%d张" % \
			[normal_hand.size(), NORMAL_MAX, instant_hand.size(), INSTANT_MAX]