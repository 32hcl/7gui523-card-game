class_name Deck
extends RefCounted

# ═══════════════════════════════════════════════════════════════
# Deck — 命名空间宿主
# 单数组循环牌堆统一走 Deck.DeckManager（见下方嵌套类）
# ═══════════════════════════════════════════════════════════════


# ═══════════════════════════════════════════════════════════════
# DeckManager — 单数组牌堆管理器
#
#   cards: 一张卡片列表（ID 0-53）。原生数组，抽/插自动伸缩、零残留。
#   active_count: 前 active_count 张是游戏牌堆，其余是隐藏牌堆。
#
#     cards[0 ........ active_count ........ 末尾]
#          ◄── 游戏牌堆 ──►◄───── 隐藏牌堆 ─────►
#
#  抽牌:  remove_at(0)              真实删除，数组自动变短 ✅（GE 左端抽）
#  补牌:  active_count++            把隐藏牌头部并入游戏牌堆 ✅
#  出牌:  push_back                 追加到隐藏牌堆尾部 ✅
#  买卡:  insert(随机位)            原生插入，自动挨紧，天然可打断 LIFO ✅
# ═══════════════════════════════════════════════════════════════

class DeckManager extends RefCounted:
	const DECK_CAP := 32          ## 游戏牌堆常态上限（补牌目标）

	var cards: Array[int] = []    ## 所有卡牌 ID。前 active_count 张为游戏牌堆
	var active_count := 0         ## 游戏牌堆张数（分界线）

	## 初始化：54 个 ID 洗牌 → 游戏 32 张 + 隐藏 22 张
	func init_standard() -> void:
		cards = []
		for i in 54:
			cards.append(i)
		cards.shuffle()
		active_count = DECK_CAP

	## 初始化：完全顺序的 0-53（不洗牌），游戏 32 张 + 隐藏 22 张
	## 用于阶段 B 循环测试，肉眼即可验证抽/出/补的顺序流转
	func init_ordered() -> void:
		cards = []
		for i in 54:
			cards.append(i)
		active_count = DECK_CAP

	## 当前游戏牌堆张数
	func active_size() -> int:
		return active_count

	## 当前总牌数
	func total() -> int:
		return cards.size()

	## 从游戏牌堆引用抽取 n 张（GE 左端抽出，真实移出）
	## 牌流方向: [GE←游戏区|空隙|隐藏区→HE]，抽口在左端
	func draw_from_active(n: int) -> Array[Card]:
		var actual := mini(n, active_count)
		var drawn: Array[Card] = []
		for i in actual:
			drawn.append(Card.from_id(cards[0]))
			cards.remove_at(0)
			active_count -= 1
		return drawn

	## 从隐藏牌堆补牌至游戏牌堆（分界线右移，隐藏牌头部并入游戏）
	func refill_active() -> void:
		while active_count < DECK_CAP and active_count < cards.size():
			active_count += 1

	## ── 牌堆构建与使用：向玩家补牌（补牌动作归牌堆，玩家仅数据）──

	## 向玩家普通区补到上限（默认 5 张）
	func fill_player_normal(player: Player.PlayerData) -> void:
		var need := Player.PlayerData.NORMAL_MAX - player.normal_hand.size()
		if need <= 0:
			return
		player.normal_hand.append_array(draw_from_active(need))

	## 向玩家立刻补区补到上限（默认 2 张）
	func fill_player_instant(player: Player.PlayerData) -> void:
		var need := Player.PlayerData.INSTANT_MAX - player.instant_hand.size()
		if need <= 0:
			return
		player.instant_hand.append_array(draw_from_active(need))

	## 兼容旧 round 调用：普通区补满
	func fill_to_five(player: Player.PlayerData) -> void:
		fill_player_normal(player)

	## 追加卡牌到隐藏牌堆末尾（出牌回收 / 保底 LIFO 顺序）
	func add_to_hidden(card: Card) -> bool:
		var id := Card.id_for(card.point, card.suit)
		if id < 0:
			return false
		cards.push_back(id)
		return true

	## 商店销毁流程：
	## 抽 7 张展示 → 销毁选中那张 → 其余随机放回隐藏 → 补齐游戏牌堆
	func shop_destroy_flow(selected_index: int) -> Dictionary:
		var shown := draw_from_active(7)

		if shown.is_empty():
			return {"shown": [], "destroyed": null, "remaining": []}

		var idx := clampi(selected_index, 0, shown.size() - 1)
		var destroyed := shown[idx]

		var remaining: Array[Card] = []
		for i in shown.size():
			if i != idx:
				remaining.append(shown[i])

		# 剩余 6 张随机放回隐藏牌堆
		for card in remaining:
			add_to_hidden(card)

		refill_active()

		return {"shown": shown, "destroyed": destroyed, "remaining": remaining}


	func _to_string() -> String:
		return "游戏牌堆:%d张  隐藏牌堆:%d张  合计:%d张" % \
			[active_count, cards.size() - active_count, cards.size()]