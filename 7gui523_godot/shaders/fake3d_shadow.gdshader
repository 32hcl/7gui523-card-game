// Fake3D shadow — 透视阴影
// 移植自 Godot4-Fake3D-Card-Game-UI-Demo (MIT, by Hei)
// 在卡牌下方渲染一个与卡牌同形的黑色半透明阴影,
// 随 x_rot / y_rot 同步做伪3D 透视变形,营造悬浮感。
//
// 与卡牌本体 fake3d.gdshader 区别：
//   - fragment 只输出纯色（黑 / 选中高亮色）
//   - 移除了 delete_color 绿幕抠像（项目卡牌无绿幕）
//   - 通过 alpha 控制阴影浓度
shader_type canvas_item;

uniform float fov : hint_range(1, 179) = 90;
uniform bool cull_back = true;

uniform bool selected = false;
uniform vec4 selected_color : source_color = vec4(1.0, 0.9, 0.5, 1.0);

uniform float y_rot : hint_range(-180, 180) = 0.0;
uniform float x_rot : hint_range(-180, 180) = 0.0;
uniform float alpha : hint_range(0.0, 1.0) = 0.45;
uniform float inset : hint_range(0, 1) = 0.0;

varying flat vec2 o;
varying vec3 p;

void vertex() {
	float sin_b = sin(y_rot / 180.0 * PI);
	float cos_b = cos(y_rot / 180.0 * PI);
	float sin_c = sin(x_rot / 180.0 * PI);
	float cos_c = cos(x_rot / 180.0 * PI);

	mat3 inv_rot_mat;
	inv_rot_mat[0][0] = cos_b;
	inv_rot_mat[0][1] = 0.0;
	inv_rot_mat[0][2] = -sin_b;
	inv_rot_mat[1][0] = sin_b * sin_c;
	inv_rot_mat[1][1] = cos_c;
	inv_rot_mat[1][2] = cos_b * sin_c;
	inv_rot_mat[2][0] = sin_b * cos_c;
	inv_rot_mat[2][1] = -sin_c;
	inv_rot_mat[2][2] = cos_b * cos_c;

	float t = tan(fov / 360.0 * PI);
	p = inv_rot_mat * vec3((UV - 0.5), 0.5 / t);
	float v = (0.5 / t) + 0.5;
	p.xy *= v * inv_rot_mat[2].z;
	o = v * inv_rot_mat[2].xy;

	VERTEX += (UV - 0.5) / TEXTURE_PIXEL_SIZE * t * (1.0 - inset);
}

void fragment() {
	if (cull_back && p.z <= 0.0) discard;
	vec2 uv = (p.xy / p.z).xy - o;
	float shape = step(max(abs(uv.x), abs(uv.y)), 0.5);
	if (selected) {
		COLOR = vec4(selected_color.rgb, selected_color.a * alpha * shape);
	} else {
		COLOR = vec4(0.0, 0.0, 0.0, alpha * shape);
	}
}
