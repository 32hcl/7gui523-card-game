// Balatro 风格 · 紧凑旋涡 · 强像素风 · 全屏背景
// 从 Shadertoy 移植为 Godot canvas_item shader
// 调参：旋涡拧得更紧(d*8)、像素块更粗(grid=96)、黏糊感加强(0.45 × 4轮)

shader_type canvas_item;

// 由 GDScript 传入该 ColorRect 实际渲染尺寸（像素）
uniform vec2 u_resolution = vec2(1920.0, 1080.0);

// 调色板函数
vec3 palette(float t) {
	vec3 a = vec3(0.40, 0.15, 0.55);
	vec3 b = vec3(0.95, 0.20, 0.60);
	vec3 c = vec3(0.15, 0.70, 0.95);
	vec3 d = vec3(1.00, 0.85, 0.30);
	return a + b * cos(6.28318 * (c * t + d));
}

// 背景底色（深藏蓝，与 COL_BG 一致）
const vec3 BG_COLOR = vec3(0.08, 0.08, 0.10);

void fragment() {
	// UV(0..1) 映射到 Shadertoy 居中坐标系: x ∈ [-aspect, +aspect], y ∈ [-1, +1]
	// u_resolution 由脚本每帧传入节点像素尺寸，避免依赖不可用的 SCREEN_PIXEL_SIZE
	float aspect = u_resolution.x / u_resolution.y;
	vec2 uv = vec2((UV.x - 0.5) * 2.0 * aspect, (UV.y - 0.5) * 2.0);

	float t = TIME;
	float d = length(uv);

	// 旋涡拧紧：距离扭曲系数 8，外圈绕更多圈才消隐
	float angle = d * 8.0 + t * 0.5;
	float c = cos(angle), s = sin(angle);
	uv = mat2(c, -s, s, c) * uv;

	// 黏糊感：4 轮迭代，振幅 0.45
	for (int i = 0; i < 4; i++) {
		uv += sin(uv.yx * 6.0 + t * 0.7) * 0.45;
	}

	// 像素感：grid=96，方块大一倍，边缘更硬
	float grid = 96.0;
	uv = floor(uv * grid) / grid;

	// 调色 + 时间微移
	vec3 col = palette(uv.x * 0.25 + uv.y * 0.25 + t * 0.08);

	// 暗角淡出：外圈过渡到深底色
	float vignette = smoothstep(1.2, 0.1, d);
	col = mix(BG_COLOR, col, vignette);

	COLOR = vec4(col, 1.0);
}