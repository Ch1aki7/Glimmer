#type vertex
#version 330 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoord;

uniform mat4 u_ViewProjection;
uniform mat4 u_Transform;

out vec2 v_TexCoord;

void main()
{
	v_TexCoord = a_TexCoord;
	gl_Position = u_ViewProjection * u_Transform * vec4(a_Position, 1.0);
}

#type fragment
#version 330 core

layout(location = 0) out vec4 color;

in vec2 v_TexCoord;

uniform float u_Time;
uniform vec2 u_Resolution;
uniform vec2 u_Mouse; // 假设你的引擎传入 vec2 格式的鼠标像素坐标

// --- Star Nest 参数配置 ---
#define iterations 17
#define formuparam 0.53

#define volsteps 20
#define stepsize 0.1

#define zoom   3.800
#define tile   0.850
#define speed  0.005

#define brightness 0.0015
#define darkmatter 0.300
#define distfading 0.730
#define saturation 0.850

void main()
{
	// 1. 坐标与方向计算
	// 直接使用 v_TexCoord (0-1) 转换为中心对齐的坐标 (-0.5 到 0.5)
	vec2 uv = v_TexCoord - 0.5;

	// 修正纵横比，防止画面拉伸
	uv.y *= u_Resolution.y / u_Resolution.x;

	vec3 dir = vec3(uv * zoom, 1.0);
	float time = u_Time * speed + 0.25;

	// 2. 鼠标旋转逻辑适配
	// 将鼠标坐标映射到 0-2 的范围进行旋转控制
	float a1 = 0.5 + u_Mouse.x / u_Resolution.x * 2.0;
	float a2 = 0.8 + u_Mouse.y / u_Resolution.y * 2.0;
	mat2 rot1 = mat2(cos(a1), sin(a1), -sin(a1), cos(a1));
	mat2 rot2 = mat2(cos(a2), sin(a2), -sin(a2), cos(a2));

	dir.xz *= rot1;
	dir.xy *= rot2;

	vec3 from = vec3(1.0, 0.5, 0.5);
	from += vec3(time * 2.0, time, -2.0);
	from.xz *= rot1;
	from.xy *= rot2;

	// 3. 体积渲染 (Volumetric Rendering)
	float s = 0.1, fade = 1.0;
	vec3 v = vec3(0.0);
	for (int r = 0; r < volsteps; r++) {
		vec3 p = from + s * dir * 0.5;
		p = abs(vec3(tile) - mod(p, vec3(tile * 2.0))); // 空间重复平铺
		float pa, a = pa = 0.0;
		for (int i = 0; i < iterations; i++) {
			p = abs(p) / dot(p, p) - formuparam; // 核心分形公式
			a += abs(length(p) - pa);
			pa = length(p);
		}
		float dm = max(0.0, darkmatter - a * a * 0.001); // 暗物质
		a *= a * a; // 增加对比度
		if (r > 6) fade *= 1.0 - dm; // 距离衰减

		v += fade;
		v += vec3(s, s * s, s * s * s * s) * a * brightness * fade; // 基于距离上色
		fade *= distfading;
		s += stepsize;
	}

	// 4. 颜色调整与最终输出
	v = mix(vec3(length(v)), v, saturation);
	color = vec4(v * 0.01, 1.0);
}
