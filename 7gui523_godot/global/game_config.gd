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


# ── 玩家 / Boss 血量 ──────────────────────────────────────────

const PLAYER_INIT_HP := 140
const PLAYER_MAX_HP  := 210
const BOSS_MAX_HP    := 110


# ── 关卡配置 ──────────────────────────────────────────────────

const TOTAL_LEVELS  := 9
const HEAL_PER_CARD := 2


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


# ── 工具方法 ──────────────────────────────────────────────────

static func get_entry_chips(level: int) -> int:
	if level < 1 or level > TOTAL_LEVELS:
		return 0
	return ENTRY_CHIPS[level]


static func get_reward_chips(level: int) -> int:
	if level < 1 or level > TOTAL_LEVELS:
		return 0
	return REWARD_CHIPS[level]


static func get_boss_name(level: int) -> String:
	if level < 1 or level > TOTAL_LEVELS:
		return "???"
	return BOSS_NAMES[level]