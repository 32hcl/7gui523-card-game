// Fake3D + 选中闪光 canvas_item shader
// 移植自 Godot4-Fake3D-Card-Game-UI-Demo (MIT, by Hei)
// vertex 与 fake3d.gdshader 完全一致（伪3D 透视）；
// fragment 额外支持流光闪光（use_flash），默认关闭时与 fake3d.gdshader 等价。
// 已删除原版绿幕抠像段（delete_color / delete_tolerance，本项目无绿幕）。
shader_type canvas_item;

uniform float fov : hint_range(1, 179) = 90;
uniform bool cull_back = true;
uniform float y_rot : hint_range(-180, 180) = 0.0;
uniform float x_rot : hint_range(-180, 180) = 0.0;
// inset=0 时图片保持原始大小；越大旋转越不容易超出自身矩形被裁剪
uniform float inset : hint_range(0, 1) = 0.0;

// =====================
// 闪光效果参数
// =====================
uniform bool use_flash = false;

// 局部闪光筛选：0 = 不筛选；1 = 屏蔽所选颜色；2 = 只处理所选颜色
uniform int flash_filter_type : hint_range(0, 2) = 0;
uniform vec3 filter_color : source_color = vec3(1.0, 1.0, 1.0);
uniform float filter_tolerance : hint_range(0, 1) = 0.2;

// 闪光类型：0 = 纯色；1 = 彩虹；2 = 自定义渐变色
uniform int flash_type : hint_range(0, 2) = 0;
uniform vec4 pure_flash_color : source_color = vec4(1.0, 1.0, 1.0, 1.0);
uniform sampler2D gradient_texture : filter_linear, repeat_enable;

// 闪光遮罩贴图（不设 = 全区域有效）
uniform sampler2D mask_texture : filter_linear_mipmap;
uniform float mask_scale : hint_range(0.1, 5.0) = 1.0;
// 闪光随旋转变化的灵敏度
uniform float speed : hint_range(0.1, 5.0) = 0.1;
// 条纹间距，值越大条纹越密集
uniform float stripe_spacing : hint_range(0.1, 50.0) = 6.5;
// 条纹宽度，值越大条纹越窄
uniform float stripe_width : hint_range(0.1, 20.0) = 1.5;
// 条纹偏移量，范围为一个周期
uniform float stripe_offset : hint_range(0.0, 1.0) = 0.55;
// 闪光类型 2 时渐变的位置和大小
uniform float gradient_offset : hint_range(0.0, 1.0) = 0.5;
uniform float gradient_scale : hint_range(0.1, 3.0) = 1.414;
// 闪光随时间移动的速度（周期/秒）
uniform float move_speed : hint_range(0.0, 5.0) = 5.0;
// 闪光强度
uniform float intensity : hint_range(0.0, 2.0) = 0.5;
// 闪光角度（0-360度）
uniform float angle : hint_range(0.0, 360.0) = 45.0;

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
	COLOR = texture(TEXTURE, uv + 0.5);
	COLOR.a *= step(max(abs(uv.x), abs(uv.y)), 0.5);

	// 闪光范围筛选
	float ddis = distance(COLOR.rgb, filter_color);
	bool fil_cons = true;
	if (flash_filter_type == 0) {
		fil_cons = true;
	} else if (flash_filter_type == 1) {
		if (ddis <= filter_tolerance) fil_cons = false;
	} else if (flash_filter_type == 2) {
		if (ddis > filter_tolerance) fil_cons = false;
	}

	// 闪光实现：基于旋转角度 + 时间的条纹流光
	if (use_flash && COLOR.a > 0.0 && fil_cons) {
		float rad = radians(angle);
		vec2 dir = vec2(cos(rad), sin(rad));

		// x_rot/y_rot 组合成旋转因子，闪光随 3D 旋转流动
		float rotation_factor = (x_rot * sin(rad) - y_rot * cos(rad)) * speed;
		rotation_factor += TIME * 2.0 * move_speed * speed;

		// 遮罩 UV 与采样
		vec2 mask_uv = (uv + 0.5);
		mask_uv = (mask_uv - 0.5) * mask_scale + 0.5;
		float mask_alpha = texture(mask_texture, mask_uv).a;

		// 闪光条纹
		float stripe = sin(dot(uv + 0.5, dir) * stripe_spacing + (rotation_factor + 2.0 * stripe_offset) * PI) * 0.5 + 0.5;
		float flash = pow(stripe, stripe_width) * intensity * mask_alpha;

		if (flash_type == 0) {
			// 纯色闪光
			COLOR.rgb = mix(COLOR.rgb, pure_flash_color.rgb, flash * pure_flash_color.a);
		} else if (flash_type == 1) {
			// 彩虹闪光
			float hue = fract(dot(uv + 1.0, dir));
			vec3 rainbow = vec3(
				0.5 + 0.5 * sin(6.2831 * hue),
				0.5 + 0.5 * sin(6.2831 * (hue + 0.33)),
				0.5 + 0.5 * sin(6.2831 * (hue + 0.66))
			);
			COLOR.rgb = mix(COLOR.rgb, rainbow, flash);
		} else {
			// 自定义渐变色闪光
			float gradient_uv_x = fract(dot(uv, dir) / gradient_scale + gradient_offset);
			vec4 gradient_color = texture(gradient_texture, vec2(gradient_uv_x, 0.0));
			COLOR.rgb = mix(COLOR.rgb, gradient_color.rgb, flash * gradient_color.a);
		}
	}
}
