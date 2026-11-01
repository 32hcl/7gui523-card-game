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
#  补牌:  active_count++            把隐藏牌头部并入游戏牌堆（普通牌堆仅新关开始调用；立刻补区共享牌堆 loop_active=true 时关内自动循环）
#  出牌:  进入不可见回收缓冲区，满 15 张后批量打乱并追加到隐藏牌堆尾部
#  买卡:  insert(随机位)            原生插入，自动挨紧，天然可打断 LIFO ✅
# ═══════════════════════════════════════════════════════════════

class DeckManager extends RefCounted:
	const DECK_CAP := 32          ## 游戏牌堆常态上限（补牌目标）
	const DECK_MAX := 96          ## 牌堆上限（防止商店买牌无限膨胀）
	const RECYCLE_BATCH_SIZE := 15 ## 出牌回收批量大小

	var cards: Array[int] = []    ## 所有卡牌 ID。前 active_count 张为游戏牌堆
	var active_count := 0         ## 游戏牌堆张数（分界线）
	var _recycle_buffer: Array[int] = [] ## 不可见出牌回收缓冲区
	var loop_active := false      ## 局内循环：抽空时自动把隐藏区/回收区打乱回灌游戏牌堆

	## 初始化：54 个 ID 洗牌 → 游戏 32 张 + 隐藏 22 张
	func init_standard() -> void:
		_init_cards(54)

	## 初始化立刻补区牌堆：52 张（无大小鬼 52/53）
	func init_instant() -> void:
		_init_cards(52)

	func _init_cards(count: int) -> void:
		cards.clear()
		_recycle_buffer.clear()
		for i in count:
			cards.append(i)
		cards.shuffle()
		active_count = DECK_CAP

	## 当前总牌数
	func total() -> int:
		return cards.size()

	## 从游戏牌堆引用抽取 n 张（GE 左端抽出，真实移出）
	## 牌流方向: [GE←游戏区|空隙|隐藏区→HE]，抽口在左端
	func draw_from_active(n: int) -> Array[Card]:
		## 共享立刻补区牌堆：先做一次局内循环，同一关内不会真的抽干
		if loop_active:
			_cycle_active()
		var actual := mini(n, active_count)
		var drawn: Array[Card] = []
		for i in actual:
			drawn.append(Card.from_id(cards[0]))
			cards.remove_at(0)
			active_count -= 1
		return drawn

	## 从隐藏牌堆补牌至游戏牌堆（分界线右移，隐藏牌头部并入游戏）
	## 仅新关卡开始时调用；关内抽干后不补牌（设计约定，见 play.gd::_start_level）。
	func refill_active() -> void:
		while active_count < DECK_CAP and active_count < cards.size():
			active_count += 1

	## 局内循环：回收区并回隐藏区 → 隐藏区补进游戏牌堆 → 仍为空则整体打乱回灌
	func _cycle_active() -> void:
		if active_count >= DECK_CAP:
			return
		flush_recycle_buffer()
		refill_active()
		if active_count == 0 and not cards.is_empty():
			cards.shuffle()
			active_count = mini(DECK_CAP, cards.size())

	## ── 牌堆构建与使用：向玩家补牌（补牌动作归牌堆，玩家仅数据）──

	func _fill_player_hand(player: Player.PlayerData, hand: Array, cap: int) -> void:
		var need := cap - hand.size()
		if need <= 0:
			return
		player.insert_cards(hand, draw_from_active(need))

	func fill_player_normal(player: Player.PlayerData) -> void:
		_fill_player_hand(player, player.normal_hand, GameConfig.NORMAL_MAX)

	func fill_player_instant(player: Player.PlayerData) -> void:
		_fill_player_hand(player, player.instant_hand, GameConfig.INSTANT_MAX)

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
		_safe_append_array(_recycle_buffer)
		_recycle_buffer.clear()

	func _flush_recycle_batches() -> void:
		while _recycle_buffer.size() >= RECYCLE_BATCH_SIZE:
			var batch: Array[int] = []
			for i in RECYCLE_BATCH_SIZE:
				batch.append(_recycle_buffer[i])
			batch.shuffle()
			_safe_append_array(batch)
			_recycle_buffer = _recycle_buffer.slice(RECYCLE_BATCH_SIZE)

	## 安全追加：超过 DECK_MAX 时丢弃超量牌
	func _safe_append_array(arr: Array[int]) -> void:
		var room := DECK_MAX - cards.size()
		if room > 0:
			cards.append_array(arr.slice(0, mini(arr.size(), room)))

	## 商店买卡直接追加到隐藏牌堆尾部，不经过出牌回收缓冲区。
	## 达到 DECK_MAX 上限时拒绝追加。
	func add_to_hidden(card: Card) -> bool:
		if card.id < 0:
			return false
		if cards.size() >= DECK_MAX:
			return false
		cards.push_back(card.id)
		return true

	func _to_string() -> String:
		return "游戏牌堆:%d张  隐藏牌堆:%d张  合计:%d张" % \
			[active_count, cards.size() - active_count, cards.size()]