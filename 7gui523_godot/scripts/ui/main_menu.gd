class_name MainMenu
extends RefCounted

## 主菜单覆盖层组件。
## 由 play.gd 在 _ready / 回到主菜单时调用 show()，通过回调接口驱动游戏流程。

const RULES_TEXT := """【目标】逐关击败 BOSS：让对方筹码 ≤ 0 即过关；你的筹码 ≤ 0 则挑战失败。初始 500 筹码。

【卡牌战力】7 > 大鬼 > 小鬼 > 5 > 2 > 3 > A > K > Q > J > 10 > 9 > 8 > 6 > 4

【牌型·合法出牌】单张 / 对子 / 三条 / 三带一 / 三带二 / 顺子（≥5 张） / 连对（≥3 连） / 飞机带翅膀 / 炸弹（4 张同点） / 王炸（大小鬼） / 五同点 / 7gui523

【7gui523】凑齐 7 + 任意鬼 + 5 + 2 + 3 共 5 张一次打出，直接赢下当前小回合（第 9 关禁用）。

【出牌】拖拽手牌到桌面区域出牌，牌型必须合法且压过上家。双击桌面 / 单击「不要」= 跳过（首出不能跳过）。

【下注 · 筹码】
• 底注：每小回合双方各押 10；上回合赢家加押 5（共 15）。
• 出牌压注：每张牌的基础注 = 5/10/K → 5/10/20，点数注见下。
• 同牌型同主点压牌时，额外加注（每张）：2/3/5/7=15, 4=4, 6=6, 8=8, 9=9, 10=10, J=11, Q=12, K=13, A=14, 鬼=20。炸弹/王炸/五同点/7gui523 同理。
• 立刻区：出 1 张 +1，一次出 2 张 +3。

【负债 · 破产】筹码不足时扣成负数（负债）；小回合结束桌面归赢家（第 9 关归 BOSS），双方双倍偿还负债。付不起双倍 → 出局；结算后仍为负 → 出局。

【补牌】每小回合赢家从牌堆补满 5 张（普通区）+ 2 张（立刻区）。牌堆抽干即停。

【门票 · 关末】首次进入赌场需购买门票（金币）。过关赢得奖励筹码。关末商店可购买卡牌和附魔。

【关卡】共 9 关：
Lv1 小六 · 无特殊 | Lv2 抹牌虫 · 摸牌概率隐藏
Lv3 急眼雀 · BOSS 每轮先手 | Lv4 疯狗 · 每张牌 ×3 额外负债
Lv5 赖账鬼 · 同花强压+禁同点 | Lv6 不识数 · 50% 蒙混过关
Lv7 黑老大 · 弃你最大牌 | Lv8 换牌鬼 · 替换手牌
Lv9 赌场老板 · 禁 7gui523，不可战胜的最终挑战

【训练模式】主界面可进入训练模式：自选 AI 对手（规则引擎 / GA训练 / 默认权重 / 备选权重），任意组合关卡限制，可选展示 AI 手牌正面。对局不消耗金币筹码，可反复训练。"""

const OVERLAY_NAME := "MainMenuOverlay"

var _play: Node
var _dialogs: PlayDialogs


func _init(play: Node, dialogs: PlayDialogs) -> void:
	_play = play
	_dialogs = dialogs


func show() -> void:
	var overlay := _build_root()
	var box := _build_box(overlay)
	_build_titles(box)
	_build_buttons(box, overlay)


func dismiss() -> void:
	var old_menu := _play.get_node_or_null(OVERLAY_NAME)
	if old_menu:
		old_menu.queue_free()


## 主菜单根节点：全屏覆盖层 + 同色系压暗背景
func _build_root() -> Control:
	var overlay := Control.new()
	overlay.name = OVERLAY_NAME
	overlay.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	overlay.z_index = 80
	overlay.mouse_filter = Control.MOUSE_FILTER_STOP
	_play.add_child(overlay)

	var dim := ColorRect.new()
	dim.color = Color(0.08, 0.08, 0.10, 0.97)
	dim.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	dim.mouse_filter = Control.MOUSE_FILTER_STOP
	overlay.add_child(dim)
	return overlay


## 主菜单纵向容器：居中 320×320
func _build_box(overlay: Control) -> VBoxContainer:
	var box := VBoxContainer.new()
	box.set_anchors_and_offsets_preset(Control.PRESET_CENTER)
	box.offset_left = -160
	box.offset_top = -160
	box.offset_right = 160
	box.offset_bottom = 160
	box.alignment = BoxContainer.ALIGNMENT_CENTER
	box.add_theme_constant_override("separation", 16)
	overlay.add_child(box)
	return box


## 主菜单标题、副标题与留白
func _build_titles(box: VBoxContainer) -> void:
	var title := Label.new()
	title.text = "七鬼 523"
	title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	title.add_theme_font_size_override("font_size", 44)
	title.add_theme_color_override("font_color", Color(1.0, 0.84, 0.42))
	title.add_theme_color_override("font_outline_color", Color(0.3, 0.2, 0.08, 1.0))
	title.add_theme_constant_override("outline_size", 8)
	box.add_child(title)

	var sub := Label.new()
	sub.text = "鸿运赌场 · 逐关挑战"
	sub.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	sub.add_theme_font_size_override("font_size", 14)
	sub.add_theme_color_override("font_color", Color(0.62, 0.60, 0.57))
	box.add_child(sub)

	var spacer := Control.new()
	spacer.custom_minimum_size = Vector2(0, 18)
	box.add_child(spacer)


## 主菜单四个入口按钮及其行为
func _build_buttons(box: VBoxContainer, overlay: Control) -> void:
	var start_btn := Button.new()
	start_btn.text = "开始游戏"
	start_btn.custom_minimum_size = Vector2(220, 52)
	_play.style_button_balatro(start_btn, Color(0.22, 0.55, 0.32), Color(0.95, 0.95, 0.93))
	box.add_child(start_btn)

	var settings_btn := Button.new()
	settings_btn.text = "⚙ 设置"
	settings_btn.custom_minimum_size = Vector2(220, 52)
	_play.style_button_balatro(settings_btn, Color(0.18, 0.17, 0.19), Color(0.88, 0.86, 0.83))
	box.add_child(settings_btn)

	var rules_btn := Button.new()
	rules_btn.text = "阅读规则"
	rules_btn.custom_minimum_size = Vector2(220, 52)
	_play.style_button_balatro(rules_btn, Color(0.20, 0.38, 0.58), Color(0.95, 0.95, 0.93))
	box.add_child(rules_btn)

	var training_btn := Button.new()
	training_btn.text = "⚔ 训练模式"
	training_btn.custom_minimum_size = Vector2(220, 52)
	_play.style_button_balatro(training_btn, Color(0.6, 0.15, 0.08), Color(0.95, 0.95, 0.93))
	box.add_child(training_btn)

	start_btn.pressed.connect(_on_start_game.bind(overlay))
	settings_btn.pressed.connect(func(): SettingsDialog.show(overlay, _play))
	rules_btn.pressed.connect(func(): _dialogs.show_rules(overlay, RULES_TEXT))
	training_btn.pressed.connect(func(): _dialogs.show_training_setup(overlay, _play._on_training_start))


func _on_start_game(overlay: Control) -> void:
	overlay.queue_free()
	_play._entrance.enter_casino_then_start(_play, _play._state.campaign.current_level)