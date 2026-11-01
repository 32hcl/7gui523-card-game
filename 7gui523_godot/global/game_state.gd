class_name GameState
extends RefCounted

# ═══════════════════════════════════════════════════════════════
# GameState — 游戏运行时状态的单一容器
#
# 将分散在 play_base / play 中的游戏数据集中管理，
# 使 game/ ↔ ui/ 松耦合，为多场景和多存档铺路。
#
# play_base.gd 通过 setget 属性转发访问。
#
# 数值统一：HP / 得分 / 筹码 已合并为 PlayerData.chips（1:1）。
# ═══════════════════════════════════════════════════════════════


# ── 战役 & 商店（跨关卡持久） ───────────────────────────────

var campaign: Campaign
var shop: Shop
var tracker: AICore.CardTracker


# ── 当前对局（每关重建） ────────────────────────────────────

var round: Round
var human: Player.PlayerData
var ai: Player.PlayerData
var human_deck: Deck.DeckManager
var ai_deck: Deck.DeckManager
var instant_deck: Deck.DeckManager          ## 立刻补区共享牌堆（双方共用，每关重建，局内循环）


# ── 关卡流程状态 ────────────────────────────────────────────

var phase: String = "battle"
var finished: bool = false
var flow_generation: int = 0


# ── 选牌 & 弃牌 ─────────────────────────────────────────────

var selected: Array[int] = []
var player_discard_count: int = 0
var player_discarded_this_round: bool = false


# ── 流程状态 ────────────────────────────────────────────────

## 弃牌窗口：玩家上一回合"不要"过 → 本回合可弃牌
var player_can_discard: bool = false
## AI 流程锁：防止同一回合重复调度 AI
var ai_busy: bool = false
## 关卡规则：玩家摸牌是否隐藏（BOSS 特殊规则）
var mask_player_draws: bool = false
## 已被隐藏的玩家手牌（以 Card 实例为键）
##
## 设计解释：为什么用 Card 实例而非 card.id 作为 Dictionary 的键？
##
## card.id 是静态定义 id，商店可买到同花色同点数的重复牌。
## 若两张牌 id 相同（例如两张黑桃A来自牌库和商店），用 id 做键
## 会互相覆盖状态，一张应正面、一张应背面的渲染会错乱。
##
## 代码中 _render_hand_row 已经用 card.get_instance_id() 区分槽位，
## 正是出于同样理由。此外，"新实例不继承旧状态"也是合理的：
## 抹牌虫的规则是摸牌时随机决定隐藏，旧牌回收后新摸到的牌
## 应重新判定，用实例引用天然实现这个语义。
##
## 潜在陷阱：切勿 duplicate() Card 实例，新实例引用不同会查不到状态。
## 代码中 Card 的流动均为引用传递（append/remove_at），无此风险。
var masked_player_cards: Dictionary = {}
## 首次发牌是否已完成（决定发牌延迟/时长与隐藏判定）
var initial_deal_finished: bool = false
## 拖拽重排后按数组顺序显示手牌，新一关发牌时复位
var manual_hand_order: bool = false


# ── 便捷方法 ────────────────────────────────────────────────

func is_player_turn() -> bool:
	return round != null and round.whose_turn() == human


func is_ai_turn() -> bool:
	return round != null and round.whose_turn() == ai


func reset_round_state() -> void:
	selected.clear()
	player_discarded_this_round = false
	player_can_discard = false
	ai_busy = false
	manual_hand_order = false