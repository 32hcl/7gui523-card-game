// Balatro 原版 LÖVE 旋涡 · 逐行数学直译 · Godot canvas_item
// 像素画版：粗像素块 → 模拟 → 颜色量化

shader_type canvas_item;

uniform float time_val : hint_range(0.0, 10000.0) = 0.0;
uniform float spin_time : hint_range(0.0, 10000.0) = 0.0;
uniform vec4 colour_1 : source_color = vec4(0.871, 0.267, 0.231, 1.0);
uniform vec4 colour_2 : source_color = vec4(0.0, 0.42, 0.71, 1.0);
uniform vec4 colour_3 : source_color = vec4(0.086, 0.137, 0.145, 1.0);
uniform float contrast : hint_range(0.0, 10.0) = 3.5;
uniform float spin_amount : hint_range(0.0, 2.0) = 0.25;

uniform float pixel_size_fac : hint_range(40.0, 1500.0) = 180.0;
uniform float color_levels : hint_range(2.0, 32.0) = 5.0;
uniform float quantize_mix : hint_range(0.0, 1.0) = 0.7;

const float SPIN_EASE = 0.5;

void fragment() {
	vec2 screen_size = 1.0 / SCREEN_PIXEL_SIZE;
	float pixel_size = length(screen_size) / pixel_size_fac;

	// 像素块坐标：同一 block 内所有像素 uv 完全相同 → 无毛刺
	vec2 block_coord = floor(FRAGCOORD.xy / pixel_size) * pixel_size + pixel_size * 0.5;

	vec2 uv = (block_coord - 0.5 * screen_size) / length(screen_size)
	          - vec2(0.12, 0.0);

	float uv_len = length(uv);

	float speed = (spin_time * SPIN_EASE * 0.2) + 302.2;
	float new_pixel_angle = atan(uv.y, uv.x) + speed
	    - SPIN_EASE * 20.0 * (spin_amount * uv_len + (1.0 - spin_amount));

	vec2 mid = (screen_size / length(screen_size)) / 2.0;
	uv = vec2(uv_len * cos(new_pixel_angle) + mid.x,
	          uv_len * sin(new_pixel_angle) + mid.y) - mid;

	uv *= 30.0;
	float spd = time_val * 2.0;
	vec2 uv2 = vec2(uv.x + uv.y);
	for (int i = 0; i < 5; i++) {
		uv2 += sin(max(uv.x, uv.y)) + uv;
		uv += 0.5 * vec2(
			cos(5.1123314 + 0.353 * uv2.y + spd * 0.131121),
			sin(uv2.x - 0.113 * spd)
		);
		uv -= cos(uv.x + uv.y) - sin(uv.x * 0.711 - uv.y);
	}

	float contrast_mod = 0.25 * contrast + 0.5 * spin_amount + 1.2;
	float paint_res = min(2.0, max(0.0, length(uv) * 0.035 * contrast_mod));
	float c1p = max(0.0, 1.0 - contrast_mod * abs(1.0 - paint_res));
	float c2p = max(0.0, 1.0 - contrast_mod * abs(paint_res));
	float c3p = 1.0 - min(1.0, c1p + c2p);

	vec4 col = (0.3 / contrast) * colour_1
	         + (1.0 - 0.3 / contrast)
	           * (colour_1 * c1p + colour_2 * c2p
	              + vec4(c3p * colour_3.rgb, c3p * colour_1.a));

	// 颜色量化：阶梯色带
	vec3 quantized = floor(col.rgb * color_levels + 0.5) / color_levels;
	col.rgb = mix(col.rgb, quantized, quantize_mix);

	COLOR = col;
}