class_name EventBus
extends RefCounted

# ═══════════════════════════════════════════════════════════════
# EventBus — 全局事件总线
#
# 解耦游戏逻辑层 (game/) 与表现层 (ui/) 的直接信号依赖。
# 逻辑层只管 emit，表现层（或音效、日志、成就等新模块）
# 只管 connect 监听，彼此不需要知道对方存在。
#
# 注册方式：project.godot → [autoload] → EventBus
# 用法：
#   EventBus.round_started.connect(my_handler)
#   EventBus.round_started.emit()
# ═══════════════════════════════════════════════════════════════


# ── 对局事件 ──────────────────────────────────────────────────

signal round_started
## who: 当前回合玩家（Player 或 Boss）
signal turn_changed(who: RefCounted)
## winner: 获胜方, damage: 造成的伤害
signal round_won(winner: RefCounted, damage: int)
## who: 出牌方, cards: 出的牌 Array[Card]
signal card_played(who: RefCounted, cards: Array)
## who: 放弃出牌方
signal card_passed(who: RefCounted)

# ── 压迫 / 克制 ──────────────────────────────────────────────

## who: 获得压迫加成方, bonus: 加成值
signal pressure_bonus(who: RefCounted, bonus: int)
## who: 出大牌方, by: 克制方
signal big_card_countered(who: RefCounted, by: RefCounted)
## who: 达成特殊胜利方
signal special_victory(who: RefCounted)

# ── 牌库事件 ──────────────────────────────────────────────────

## who: 补牌方, count: 补充数量
signal deck_refill(who: RefCounted, count: int)
## which: "player" 或 "boss"
signal deck_emptied(which: String)

# ── 错误事件 ──────────────────────────────────────────────────

## reason: 错误原因描述
signal error_invalid_play(reason: String)
signal error_cannot_beat(reason: String)
signal error_hand_full
signal error_not_your_turn

# ── 游戏结束 ──────────────────────────────────────────────────

signal game_over