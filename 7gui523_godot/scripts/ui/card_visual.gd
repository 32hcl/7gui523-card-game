class_name CardVisual
extends Control

## 单卡视觉组件：逻辑层(Card id) ↔ 视觉层(贴图)的唯一契约
## 自包含脚本，代码内自建 TextureRect + 阴影层，不依赖额外 .tscn
##
## 节点结构(z 序从下到上):
##   CardVisual (Control)
##     _shadow_tex  (TextureRect) — 伪3D 透视阴影（可选）
##     _tex        (TextureRect) — 牌面贴图（带/不带 fake3d）

const SUIT_DIR := {
	0: "Spades",    ## 黑桃  id 0-12
	1: "Hearts",    ## 红桃  id 13-25
	2: "Clubs",     ## 梅花  id 26-38
	3: "Diamonds",  ## 方块  id 39-51
}
const CARDS_DIR := "res://art/Top-Down/Cards"
const BACK_TEX := "res://art/Top-Down/Cards/Card_Back-Red-88x124.png"
const JOKER_BLACK := "res://art/Top-Down/Cards/joker_black.png"
const JOKER_RED := "res://art/Top-Down/Cards/joker_red.png"

const SHADOW_OFFSET := Vector2(6, 10)   ## 阴影偏移（右下，模拟左上光源）
const SHADOW_ALPHA := 0.42              ## 阴影浓度
const SHADOW_ALPHA_SEL := 0.85          ## 选中时阴影浓度（边框冷光更浓更醒目）
const SHADOW_INSET := 0.62              ## 阴影 inset（比牌面稍大，避免被矩形裁剪）
const FLASH_GRADIENT := "res://art/effects/gloden_glow.tres"  ## 闪光金色渐变（源自 Demo）

## 预加载着色器（避免运行时 load 开销）
const SHD_DISSOLVE := preload("res://shaders/dissolve2d.gdshader")
const SHD_FAKE3D := preload("res://shaders/fake3d_flash.gdshader")
const SHD_SHADOW := preload("res://shaders/fake3d_shadow.gdshader")

## 选中边框（垫在卡牌最底层的发光描边；非金色，用深靛蓝+冷光）
const SEL_FRAME_COLOR := Color(0.32, 0.52, 0.88, 0.92)
const SEL_FRAME_GLOW := Color(0.20, 0.38, 0.72, 0.25)

## 当前展示的 Card id（-1 表示非牌面图）
var card_id: int = -1

var _tex: TextureRect
var _shadow_tex: TextureRect = null
var _fake3d := false
var _shadow_enabled := false
## 假 3D 当前旋转角（供平滑动画读写插值）
var _rot_x := 0.0
var _rot_y := 0.0
var _rot_tween: Tween = null
var _layout_tween: Tween = null  ## 布局/进出场动画槽位（由上层的重排清理）
var base_z := 0                 ## 覆盖层级基准（由上层布局写入，hover 退出时恢复，保证重叠命中稳定）


## 杀掉上层重排遗留的布局动画（不依赖 Node.kill_tweens/get_tweens，全版本兼容）
func kill_layout_tween() -> void:
	if _layout_tween and _layout_tween.is_valid():
		_layout_tween.kill()
		_layout_tween = null


## 构造函数：固定卡面基准尺寸，自建 TextureRect
func _init(p_size: Vector2 = Vector2(88, 124)) -> void:
	custom_minimum_size = p_size
	size = p_size

	_tex = TextureRect.new()
	_tex.name = "Texture"
	_tex.position = Vector2.ZERO
	_tex.size = p_size
	_tex.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	_tex.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	_tex.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(_tex)


func _notification(what: int) -> void:
	if what == NOTIFICATION_RESIZED and _tex:
		_tex.position = Vector2.ZERO
		_tex.size = size
		if _shadow_tex:
			_shadow_tex.size = size
		if _sel_frame:
			_sel_frame.size = size + Vector2(10, 10)


## 逻辑→视觉唯一入口：按 Card id 显示牌面
func show_id(id: int) -> void:
	card_id = id
	var tex := texture_for_id(id)
	_tex.texture = tex
	_sync_shadow_texture(tex)


## 显示任意资源图（牌背 / 牌堆 / 筹码 / 画廊用）
func show_texture(tex: Texture2D) -> void:
	card_id = -1
	_tex.texture = tex
	_sync_shadow_texture(tex)


func _sync_shadow_texture(tex: Texture2D) -> void:
	if _shadow_enabled and _shadow_tex:
		_shadow_tex.texture = tex


## 交换当前展示为牌背
func show_back() -> void:
	show_texture(load(BACK_TEX))


# ═══════════════════════════════════════════════════════════════
# Fake3D 透视 + 阴影
# ═══════════════════════════════════════════════════════════════

## 启用 Fake3D 悬浮旋转（给贴图挂假透视 shader，小丑牌式立牌效果）
## 同时启用阴影层（带偏移的同形黑色半透明）
## 牌面用 fake3d_flash.gdshader（含闪光能力，use_flash 默认关，关时与纯 fake3d 等价）
func enable_fake3d(with_shadow: bool = true) -> void:
	if _fake3d:
		return
	_fake3d = true

	var mat := ShaderMaterial.new()
	mat.shader = SHD_FAKE3D
	mat.set_shader_parameter("inset", 0.6)
	## 闪光默认参数（对齐 Demo 牌面配置：金色渐变流光、纯白区不闪）
	mat.set_shader_parameter("use_flash", false)
	mat.set_shader_parameter("flash_filter_type", 1)
	mat.set_shader_parameter("filter_color", Color(1, 1, 1, 1))
	mat.set_shader_parameter("filter_tolerance", 1.0)
	mat.set_shader_parameter("flash_type", 2)
	mat.set_shader_parameter("gradient_texture", load(FLASH_GRADIENT))
	mat.set_shader_parameter("speed", 0.1)
	mat.set_shader_parameter("stripe_spacing", 6.5)
	mat.set_shader_parameter("stripe_width", 1.5)
	mat.set_shader_parameter("stripe_offset", 0.55)
	mat.set_shader_parameter("gradient_offset", 0.5)
	mat.set_shader_parameter("gradient_scale", 1.414)
	mat.set_shader_parameter("move_speed", 3.0)
	mat.set_shader_parameter("intensity", 1.0)
	mat.set_shader_parameter("angle", 45.0)
	_tex.material = mat

	if with_shadow:
		_enable_shadow()


## 切换选中态（上浮/放大由布局处理；这里保留阴影金色高亮 + 牌面流光闪光）
## 不再绘制金框描边
func set_selected(selected: bool) -> void:
	set_flash(selected)
	set_shadow_alpha(SHADOW_ALPHA_SEL if selected else SHADOW_ALPHA)
	if _sel_frame:
		_sel_frame.visible = false


## 开关牌面流光闪光（需先 enable_fake3d）
func set_flash(enabled: bool) -> void:
	if not _fake3d or _tex.material == null:
		return
	_tex.material.set_shader_parameter("use_flash", enabled)


# ═══════════════════════════════════════════════════════════════
# 溶解消失
# ═══════════════════════════════════════════════════════════════

## 溶解消失：燃烧边框式（dissolve_value 1→0），结束时自动 queue_free
## 返回 Tween，调用方可 await 等待溶解完成
func dissolve(duration: float = 0.5) -> Tween:
	## 杀旧 tween，避免 _rot_tween/_layout_tween 持有已替换的材质引用
	if _rot_tween and _rot_tween.is_valid():
		_rot_tween.kill()
		_rot_tween = null
	if _layout_tween and _layout_tween.is_valid():
		_layout_tween.kill()
		_layout_tween = null

	var mat := ShaderMaterial.new()
	mat.shader = SHD_DISSOLVE
	mat.set_shader_parameter("dissolve_texture", _get_dissolve_noise())
	mat.set_shader_parameter("dissolve_value", 1.0)
	mat.set_shader_parameter("burn_border_size", 0.05)
	mat.set_shader_parameter("burn_color", Color(1.0, 0.3, 0.0, 0.78))
	_tex.material = mat
	## 材质已替换，伪3D/选中态一并收尾
	_fake3d = false
	if _shadow_tex:
		var stw := create_tween()
		stw.tween_property(_shadow_tex, "modulate:a", 0.0, duration * 0.5)
	if _sel_frame:
		_sel_frame.visible = false

	var tw := create_tween()
	tw.tween_property(mat, "shader_parameter/dissolve_value", 0.0, duration)
	tw.tween_callback(queue_free)
	return tw


## 溶解噪声图（代码同步生成，避免 NoiseTexture2D 异步首帧采样为空的坑）
## 参数对齐 Demo：seed=-54, frequency=0.0019
static var _dissolve_noise: ImageTexture = null

static func _get_dissolve_noise() -> ImageTexture:
	if _dissolve_noise != null:
		return _dissolve_noise
	var noise := FastNoiseLite.new()
	noise.seed = -54
	noise.frequency = 0.0019
	var img := Image.create(256, 256, false, Image.FORMAT_RGBA8)
	for y in 256:
		for x in 256:
			var v: float = clampf((noise.get_noise_2d(x, y) + 1.0) * 0.5, 0.0, 1.0)
			img.set_pixel(x, y, Color(v, v, v, 1.0))
	_dissolve_noise = ImageTexture.create_from_image(img)
	return _dissolve_noise


## 内部：确保选中金框存在（垫在最底层，比卡大一圈的圆角描边）
var _sel_frame: Panel = null

func _ensure_sel_frame() -> void:
	if _sel_frame:
		return
	_sel_frame = Panel.new()
	_sel_frame.name = "SelFrame"
	_sel_frame.position = Vector2(-5, -5)
	_sel_frame.size = size + Vector2(10, 10)
	_sel_frame.mouse_filter = Control.MOUSE_FILTER_IGNORE
	var st := StyleBoxFlat.new()
	st.bg_color = SEL_FRAME_GLOW
	st.border_color = SEL_FRAME_COLOR
	st.set_border_width_all(3)
	st.set_corner_radius_all(10)
	_sel_frame.add_theme_stylebox_override("panel", st)
	add_child(_sel_frame)
	move_child(_sel_frame, 0)


## 设置假 3D 旋转（x_rot 上下翻仰、y_rot 左右转），0,0 即平铺原样
## 同步应用到阴影层，保持透视一致
## 即时跳变；需要平滑/弹性过渡请用 to_rot()
func set_fake3d_rot(x: float, y: float) -> void:
	_rot_x = x
	_rot_y = y
	_apply_rot()


## 平滑旋转到目标角（对应 Demo 的 to_rot，可弹性缓动的假 3D 立牌旋转）
## 取消进行中的旋转动画并新开一条，避免抖动；返回该 Tween 供 await
func to_rot(x: float, y: float, duration: float = 0.18,
		trans: Tween.TransitionType = Tween.TRANS_BACK,
		ease: Tween.EaseType = Tween.EASE_OUT) -> Tween:
	if not _fake3d or _tex.material == null:
		return null
	if _rot_tween and _rot_tween.is_valid():
		_rot_tween.kill()
	var tw := create_tween()
	tw.set_trans(trans).set_ease(ease)
	tw.tween_method(_apply_tween_rot, Vector2(_rot_x, _rot_y), Vector2(x, y), duration)
	_rot_tween = tw
	return tw


## 旋转动画每帧回调：写入当前角度并应用到材质
func _apply_tween_rot(cur: Vector2) -> void:
	_rot_x = cur.x
	_rot_y = cur.y
	_apply_rot()


## 写当前角度到牌面与阴影材质（set_fake3d_rot 与 to_rot 的公共落点）
func _apply_rot() -> void:
	if not _fake3d or _tex.material == null:
		return
	_tex.material.set_shader_parameter("x_rot", _rot_x)
	_tex.material.set_shader_parameter("y_rot", _rot_y)
	if _shadow_enabled and _shadow_tex and _shadow_tex.material:
		_shadow_tex.material.set_shader_parameter("x_rot", _rot_x)
		_shadow_tex.material.set_shader_parameter("y_rot", _rot_y)


## 切换选中高亮阴影（选中时阴影变为金色高亮色）
func set_shadow_selected(selected: bool) -> void:
	if not _shadow_enabled or not _shadow_tex or not _shadow_tex.material:
		return
	_shadow_tex.material.set_shader_parameter("selected", selected)


## 调整阴影偏移量（默认右下 6,10）
func set_shadow_offset(offset: Vector2) -> void:
	if not _shadow_enabled or not _shadow_tex:
		return
	_shadow_tex.position = offset


## 调整阴影浓度（0~1，默认 0.42）
func set_shadow_alpha(alpha: float) -> void:
	if not _shadow_enabled or not _shadow_tex or not _shadow_tex.material:
		return
	_shadow_tex.material.set_shader_parameter("alpha", alpha)


# ═══════════════════════════════════════════════════════════════
# 内部
# ═══════════════════════════════════════════════════════════════


## 内部：创建阴影层
func _enable_shadow() -> void:
	if _shadow_enabled:
		return
	_shadow_enabled = true

	_shadow_tex = TextureRect.new()
	_shadow_tex.name = "Shadow"
	_shadow_tex.position = SHADOW_OFFSET
	_shadow_tex.size = size
	_shadow_tex.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	_shadow_tex.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	_shadow_tex.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_shadow_tex.z_index = -1

	var mat := ShaderMaterial.new()
	mat.shader = SHD_SHADOW
	mat.set_shader_parameter("inset", SHADOW_INSET)
	mat.set_shader_parameter("alpha", SHADOW_ALPHA)
	_shadow_tex.material = mat

	## 同步当前牌面纹理到阴影层（解耦 show_texture / enable_fake3d 调用顺序）
	if _tex.texture:
		_shadow_tex.texture = _tex.texture

	## 阴影必须在卡牌之下（作为第一个子节点）
	add_child(_shadow_tex)
	move_child(_shadow_tex, 0)


# ═══════════════════════════════════════════════════════════════
# 静态工具
# ═══════════════════════════════════════════════════════════════

## 按 id 返回牌面贴图（id 0-51 花色牌，52 小鬼/黑Joker / 53 大鬼/红Joker）
static func texture_for_id(id: int) -> Texture2D:
	if id == 52:
		return load(JOKER_BLACK)
	if id == 53:
		return load(JOKER_RED)
	var suit_i := id / 13
	var rank_i := id % 13
	var path := "%s/%s/%02d.png" % [CARDS_DIR, SUIT_DIR[suit_i], rank_i]
	return load(path)