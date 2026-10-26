class_name GameState
extends RefCounted

# ═══════════════════════════════════════════════════════════════
# GameState — 游戏运行时状态的单一容器
#
# 将分散在 play_base / play 中的游戏数据集中管理，
# 使 game/ ↔ ui/ 松耦合，并为多场景和多存档铺路。
#
# play_base.gd 通过 setget 属性转发访问，
# 所有旧代码中的 _campaign / _round / _ai 等引用无需改动。
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


# ── HP ───────────────────────────────────────────────────────

var human_hp: int = 100
var ai_hp: int = 100


# ── 关卡流程状态 ────────────────────────────────────────────

var phase: String = "battle"
var finished: bool = false
var flow_generation: int = 0


# ── 选牌 & 弃牌 ─────────────────────────────────────────────

var selected: Array[int] = []
var player_discard_count: int = 0
var player_discarded_this_round: bool = false
var discard_unlocked: bool = false

## BOSS 最近一次实际出牌张数（Round 结算时会清空 last_play）
var last_boss_play_count: int = 0


# ── 便捷方法 ────────────────────────────────────────────────

func is_player_turn() -> bool:
	return round != null and round.whose_turn() == human


func is_ai_turn() -> bool:
	return round != null and round.whose_turn() == ai


func reset_round_state() -> void:
	selected.clear()
	player_discarded_this_round = false
	last_boss_play_count = 0