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

enum CardSortMode { SAME_RANK = 0, FULL_SORT = 1, NO_SORT = 2 }

class PlayerData extends RefCounted:
	var normal_hand:  Array[Card] = []  ## 普通区手牌（上限见 GameConfig.NORMAL_MAX）
	var instant_hand: Array[Card] = []  ## 立刻补区手牌（上限见 GameConfig.INSTANT_MAX）
	var card_sort_mode: int = CardSortMode.SAME_RANK  ## 补牌排序模式

	## 身份标识
	var name: String = "32%Hcl"
	var is_human := false
	var ai_level := 0

	## ── 筹码（HP / 得分 / 筹码已统一，1:1） ─────────────────
	## 允许为负：负数就是负债，不单独标记 debt / bankrupt。
	var chips: int = GameConfig.INITIAL_CHIPS

	## 新关卡：用战役里的筹码重置本局筹码
	func reset_battle_chips(amount: int) -> void:
		chips = amount

	## 往桌面下注：直接扣筹码，允许扣成负数（负数即负债）
	func pay_stake(amount: int) -> void:
		if amount > 0:
			chips -= amount

	## 当前负债额（筹码为负时取绝对值，否则 0）
	func debt_amount() -> int:
		return maxi(0, -chips)

	## 双倍偿还指定负债；筹码不够时返回 false，且不继续扣成更多负债。
	## “不可以借钱还贷”：付不起就停在这里，由 Round 记录结算失败。
	func pay_debt(debt: int) -> bool:
		if debt <= 0:
			return true
		var need := debt * GameConfig.DEBT_REPAY_MULTIPLIER
		if chips >= need:
			chips -= need
			return true
		# 付不起：不借新债补旧债，余额保持，直接记结算失败
		return false

	## 赢家收走桌面筹码
	func receive_chips(amount: int) -> void:
		if amount > 0:
			chips += amount

	## 合并手牌填入提供的数组（避免每次访问分配新数组）
	func collect_hand_into(out: Array[Card]) -> void:
		out.clear()
		out.append_array(normal_hand)
		out.append_array(instant_hand)

	func hand_size() -> int:
		return normal_hand.size() + instant_hand.size()

	func is_hand_empty() -> bool:
		return normal_hand.is_empty() and instant_hand.is_empty()


	## 同 id 卡牌的下一个副本标记（初始牌0，商店第一张1，以此类推）
	func next_copy_mark(card_id: int) -> int:
		var max_mark := 0
		for c in normal_hand:
			if c.id == card_id and c.copy_mark > max_mark:
				max_mark = c.copy_mark
		for c in instant_hand:
			if c.id == card_id and c.copy_mark > max_mark:
				max_mark = c.copy_mark
		return max_mark + 1


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


	## 统计本手牌中有多少张取自立刻补区
	## 与 remove_cards_from_hand 一致：普通区优先匹配，匹配不到才算立刻补区
	func count_instant_in_play(cards_to_play: Array[Card]) -> int:
		var normal_keys: Array[String] = []
		for c in normal_hand:
			normal_keys.append(c.point + "|" + c.suit)
		var used_instant := 0
		for card in cards_to_play:
			var key: String = card.point + "|" + card.suit
			var idx: int = normal_keys.find(key)
			if idx >= 0:
				normal_keys.remove_at(idx)
			else:
				used_instant += 1
		return used_instant


	## 手牌排序：等级降序，同级花色升序
	func sort_hand() -> void:
		normal_hand.sort_custom(func(a: Card, b: Card):
			var ra := Card.get_rank(a.point)
			var rb := Card.get_rank(b.point)
			if ra != rb:
				return ra > rb
			return a.suit < b.suit)

	## 抽牌归位：新牌插入同点牌的右侧；无同点则追加到末尾（相同大小的牌聚在一起）
	func insert_same_rank_together(hand: Array[Card], new_cards: Array[Card]) -> void:
		for card in new_cards:
			var inserted := false
			for i in range(hand.size() - 1, -1, -1):
				if hand[i].point == card.point:
					hand.insert(i + 1, card)
					inserted = true
					break
			if not inserted:
				hand.append(card)

	## 统一补牌入口：根据 card_sort_mode 分发到不同的排序策略
	func insert_cards(hand: Array[Card], new_cards: Array[Card]) -> void:
		match card_sort_mode:
			CardSortMode.FULL_SORT:
				hand.append_array(new_cards)
				sort_hand()
			CardSortMode.NO_SORT:
				hand.append_array(new_cards)
			_:
				insert_same_rank_together(hand, new_cards)


	func _to_string() -> String:
		return "普通区:%d/%d张  立刻补区:%d/%d张" % \
			[normal_hand.size(), GameConfig.NORMAL_MAX, instant_hand.size(), GameConfig.INSTANT_MAX]