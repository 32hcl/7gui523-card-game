class_name AudioManager
extends RefCounted

# ═══════════════════════════════════════════════════════════════
# AudioManager — 音频管理单例
#
# 当前为空壳骨架，预留了 BGM / SFX / 音量控制的标准接口。
#
# 接入方式（未来）：
#   1. 将 AudioStreamPlayer 节点挂在 autoload 场景上
#   2. 音频资源放入 res://art/sfx/ 和 res://art/bgm/
#   3. 在 project.godot [autoload] 注册 AudioManager
#   4. 调用 AudioManager.play_sfx("play_card")
#     或订阅事件：EventBus.card_played.connect(AudioManager._on_card_played)
# ═══════════════════════════════════════════════════════════════


# ── 资源路径约定 ──────────────────────────────────────────────

const SFX_DIR := "res://art/sfx/"
const BGM_DIR := "res://art/bgm/"


# ── 音量（0.0 ~ 1.0，未来接入设置面板持久化） ──────────────

var master_volume: float = 1.0:
	set(v):
		master_volume = clampf(v, 0.0, 1.0)
		_apply_volume()

var bgm_volume: float = 0.8:
	set(v):
		bgm_volume = clampf(v, 0.0, 1.0)
		_apply_volume()

var sfx_volume: float = 1.0:
	set(v):
		sfx_volume = clampf(v, 0.0, 1.0)
		_apply_volume()


# ── 静音 ─────────────────────────────────────────────────────

var muted: bool = false:
	set(v):
		muted = v
		_apply_volume()


# ── BGM ──────────────────────────────────────────────────────

static func play_bgm(resource_name: String, fade_in: float = 0.0) -> void:
	pass


static func stop_bgm(fade_out: float = 0.0) -> void:
	pass


# ── SFX ──────────────────────────────────────────────────────

static func play_sfx(resource_name: String, pitch_variation: float = 0.0) -> void:
	pass


# ── 事件挂钩（未来接入 EventBus） ────────────────────────────

## 出牌音效
static func _on_card_played(who: RefCounted, cards: Array) -> void:
	pass


## 胜利音效
static func _on_round_won(winner: RefCounted, damage: int) -> void:
	pass


# ── 内部 ─────────────────────────────────────────────────────

static func _apply_volume() -> void:
	## TODO: 遍历所有 AudioStreamPlayer 节点，应用 master/bgm/sfx/muted
	pass