class_name GameConfig
extends RefCounted

# ═══════════════════════════════════════════════════════════════
# GameConfig — 游戏平衡数值的单一真相来源
#
# 所有模块（core/game/ui）统一引用此文件，消除魔数散落。
# 纯 const 数据，零依赖，不会形成类型环依赖。
# ═══════════════════════════════════════════════════════════════


# ── 手牌容量 ──────────────────────────────────────────────────

const NORMAL_MAX  := 5    ## 普通区手牌上限
const INSTANT_MAX := 2    ## 立刻补区手牌上限


# ── 筹码（HP / 得分 / 筹码 已统一为同一货币，1:1） ────────────

const INITIAL_CHIPS := 500        ## 玩家初始筹码
const BOSS_INITIAL_CHIPS := 500   ## 每关 BOSS 初始筹码
const DEBT_REPAY_MULTIPLIER := 2  ## 小回合结束：双倍偿还本局负债
const BITE_CHIPS_PER_CARD := 3    ## 疯狗：每张出牌额外扣玩家筹码
const BOSS_FINISH_CHIPS_PER_CARD := 3  ## BOSS 先出完：玩家每张剩余牌额外扣 3 筹码


# ── 关卡配置 ──────────────────────────────────────────────────

const TOTAL_LEVELS  := 9


# ── 筹码 & 金币 ───────────────────────────────────────────────

const ENTRY_CHIPS  := [0, 1, 5, 10, 20, 50, 100, 200, 400, 800]
const REWARD_CHIPS := [0, 3, 12, 25, 50, 120, 250, 500, 1000, 2000]

const ENTRY_TICKET_DOLLARS := 5_000_000
const INITIAL_DOLLARS      := 50_000_000_000


# ── Boss 名称表（下标 = 关卡号） ─────────────────────────────

const BOSS_NAMES := [
	"",
	"新手",
	"抹牌虫",
	"急眼雀",
	"疯狗",
	"赖账鬼",
	"不识数",
	"黑老大",
	"换牌鬼",
	"赌场老板",
]