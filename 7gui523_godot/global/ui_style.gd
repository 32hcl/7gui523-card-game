class_name UIStyle
extends RefCounted

## 全局 UI 样式工具：面板圆角边框与 Balatro 风格按钮的唯一实现。
## play_base 与 shop_panel 均委托到这里，避免同一套样式逻辑存在多份拷贝。

## 生成统一的面板/按钮 StyleBoxFlat：圆角 + 边框 + 可选阴影与顶部内边距
static func flat_border(bg_c: Color, border_c: Color, bw: int = 2, radius: int = 6,
		shadow_c: Color = Color(0, 0, 0, 0), shadow_size: int = 0,
		shadow_off: int = 0, content_top: int = 0) -> StyleBoxFlat:
	var sb := StyleBoxFlat.new()
	sb.bg_color = bg_c
	sb.border_color = border_c
	sb.set_border_width_all(bw)
	sb.set_corner_radius_all(radius)
	if shadow_size > 0:
		sb.shadow_color = shadow_c
		sb.shadow_size = shadow_size
		sb.shadow_offset = Vector2(0, shadow_off)
	if content_top != 0:
		sb.content_margin_top = content_top
	return sb


## Balatro 风格按钮：常态/悬停/按下/禁用/焦点五套样式 + 字体颜色
static func style_button(btn: Button, bg: Color,
		text_col: Color = Color(0.95, 0.95, 0.93)) -> void:
	var shadow_c := Color(0.0, 0.0, 0.0, 0.55)
	var border_c := bg.darkened(0.18)
	btn.add_theme_stylebox_override("normal", flat_border(bg, border_c, 1, 6, shadow_c, 4, 4, 2))
	btn.add_theme_stylebox_override("hover",
		flat_border(bg.lightened(0.08), border_c.lightened(0.05), 1, 6, shadow_c, 5, 4, 2))
	btn.add_theme_stylebox_override("pressed",
		flat_border(bg.darkened(0.12), border_c.darkened(0.08), 1, 6, shadow_c, 1, 1, 6))
	btn.add_theme_stylebox_override("disabled",
		flat_border(Color(0.18, 0.18, 0.19), Color(0.22, 0.22, 0.23), 1, 6))
	btn.add_theme_stylebox_override("focus",
		flat_border(Color(0, 0, 0, 0), Color(0.35, 0.35, 0.38), 1, 6))
	btn.add_theme_color_override("font_color", text_col)
	btn.add_theme_color_override("font_hover_color", Color(1.0, 1.0, 0.97))
	btn.add_theme_color_override("font_pressed_color", text_col.darkened(0.12))
	btn.add_theme_color_override("font_disabled_color", Color(0.45, 0.44, 0.43))
