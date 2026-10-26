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
#  出牌:  进入不可见回收缓冲区，满 15 张后批量打乱并追加到隐藏牌堆尾部
#  买卡:  insert(随机位)            原生插入，自动挨紧，天然可打断 LIFO ✅
# ═══════════════════════════════════════════════════════════════

class DeckManager extends RefCounted:
	const DECK_CAP := 32          ## 游戏牌堆常态上限（补牌目标）
	const RECYCLE_BATCH_SIZE := 15 ## 出牌回收批量大小

	var cards: Array[int] = []    ## 所有卡牌 ID。前 active_count 张为游戏牌堆
	var active_count := 0         ## 游戏牌堆张数（分界线）
	var _recycle_buffer: Array[int] = [] ## 不可见出牌回收缓冲区

	## 初始化：54 个 ID 洗牌 → 游戏 32 张 + 隐藏 22 张
	func init_standard() -> void:
		cards = []
		_recycle_buffer = []
		for i in 54:
			cards.append(i)
		cards.shuffle()
		active_count = DECK_CAP

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

	## 向玩家普通区补到上限
	func fill_player_normal(player: Player.PlayerData) -> void:
		var need := GameConfig.NORMAL_MAX - player.normal_hand.size()
		if need <= 0:
			return
		player.insert_cards(player.normal_hand, draw_from_active(need))

	## 向玩家立刻补区补到上限
	func fill_player_instant(player: Player.PlayerData) -> void:
		var need := GameConfig.INSTANT_MAX - player.instant_hand.size()
		if need <= 0:
			return
		player.insert_cards(player.instant_hand, draw_from_active(need))

	## 出牌进入不可见缓冲区；满一批后整体打乱并追加到隐藏牌堆尾部。
	func recycle_played(cards_to_recycle: Array[Card]) -> void:
		for card in cards_to_recycle:
			if card.id >= 0:
				_recycle_buffer.append(card.id)
		_flush_recycle_batches()

	## 关末调用：残余缓冲区整体打乱后进入隐藏牌堆，不跨关保留。
	func flush_recycle_buffer() -> void:
		if _recycle_buffer.is_empty():
			return
		_recycle_buffer.shuffle()
		cards.append_array(_recycle_buffer)
		_recycle_buffer.clear()

	func _flush_recycle_batches() -> void:
		while _recycle_buffer.size() >= RECYCLE_BATCH_SIZE:
			var batch: Array[int] = []
			for i in RECYCLE_BATCH_SIZE:
				batch.append(_recycle_buffer[i])
			batch.shuffle()
			cards.append_array(batch)
			_recycle_buffer = _recycle_buffer.slice(RECYCLE_BATCH_SIZE)

	## 商店买卡直接追加到隐藏牌堆尾部，不经过出牌回收缓冲区。
	func add_to_hidden(card: Card) -> bool:
		if card.id < 0:
			return false
		cards.push_back(card.id)
		return true

	func _to_string() -> String:
		return "游戏牌堆:%d张  隐藏牌堆:%d张  合计:%d张" % \
			[active_count, cards.size() - active_count, cards.size()]