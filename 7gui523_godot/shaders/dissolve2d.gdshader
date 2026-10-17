// 2D 溶解消失 shader（燃烧边框式）
// 移植自 Godot4-Fake3D-Card-Game-UI-Demo (MIT, by Hei)
// dissolve_value: 1 = 完整显示，0 = 完全消失（动画即 1 → 0）
// 噪声图红色通道决定每个像素的消失先后；燃烧边框在消失前沿染上 burn_color
shader_type canvas_item;

uniform sampler2D dissolve_texture : source_color;
uniform float dissolve_value : hint_range(0.0, 1.0);
// 燃烧边框尺寸
uniform float burn_border_size : hint_range(0.0, 1.0, 0.01);
// 燃烧颜色
uniform vec4 burn_color : source_color = vec4(1.0, 1.0, 1.0, 1.0);

void fragment() {
	vec4 main_texture = texture(TEXTURE, UV);
	vec4 noise_texture = texture(dissolve_texture, UV);

	// 燃烧边框（通过红色通道判断）
	float burn_size_step = burn_border_size * step(0.001, dissolve_value) * step(dissolve_value, 0.999);
	float threshold = smoothstep(noise_texture.x - burn_size_step, noise_texture.x, dissolve_value);
	float border = smoothstep(noise_texture.x, noise_texture.x + burn_size_step, dissolve_value);

	COLOR.a = threshold;
	COLOR.rgb = mix(burn_color.rgb, main_texture.rgb, border);
}
