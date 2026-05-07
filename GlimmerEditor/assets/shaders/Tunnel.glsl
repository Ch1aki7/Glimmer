#type vertex
#version 330 core
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoord;
out vec2 v_TexCoord;
void main() {
	v_TexCoord = a_TexCoord;
	gl_Position = vec4(a_Position, 1.0);
}

#type fragment
#version 330 core

layout(location = 0) out vec4 color;
in vec2 v_TexCoord;

uniform float u_Time;
uniform vec2 u_Resolution;

// 适配宏定义
#define iTime u_Time
#define iResolution u_Resolution
#define T (sin(iTime*.6)*16.+iTime*1e2)
#define P(z) (vec3(cos((z)*.011)*16.+cos((z) * .012)  *24., \
                   cos((z)*.01)*4., (z)))
#define R(a) mat2(cos(a+vec4(0,33,11,0)))
#define N normalize

vec4 lights; // 全局光照累加器

float boxen(vec3 p) {
	p = abs(fract(p / 2e1) * 2e1 - 1e1) - 1.;
	return min(p.x, min(p.y, p.z));
}

float map(vec3 p) {
	vec3 q = P(p.z);
	float m, g = q.y - p.y + 6.;

	m = boxen(p);
	p.xy -= q.xy;

	float red, blue;
	float e = min(red = length(p.xy - sin(p.z / 12. + vec2(0, 1.3)) * 12.) - 1.,
		blue = length(p.xy - sin(p.z / 16. + vec2(0, .7)) * 16.) - 2.);

	// 发光累加
	lights += vec4(1e1, 2, 1, 0) / (.1 + abs(red));
	lights += vec4(1, 2, 1e1, 0) / (.1 + abs(blue) / 1e1);

	p = abs(p);
	float tex = abs(length(sin(p * cos(p.yzx / 3e1) * 4.) / (p * 4.)));
	float tun = min(32. - p.x - p.y, 24. - p.y);

	float d = max(min(m, g), tun) - tex;
	return min(e, d);
}

void main() {
	// 1. 初始化变量
	float i = 0.0, s = 0.0, d = 0.0;
	vec2 fragCoord = gl_FragCoord.xy;
	vec2 u = (fragCoord - iResolution.xy / 2.) / iResolution.y;
	u.y -= .2;

	vec4 o = vec4(0);
	lights = vec4(0); // 每一像素开始前重置光照

	vec3 p_pos = P(T), ro = p_pos,
		Z = N(P(T + 2.) - p_pos),
		X = N(vec3(Z.z, 0, -Z.x)),
		D = N(vec3(R(sin(T * .005) * .4) * u, 1)
			* mat3(-X, cross(X, Z), Z));

	// 2. 主步进循环
	for (; i < 100.0; i++) {
		p_pos = ro + D * d;
		s = map(p_pos) * 0.8;
		d += s;
		o += lights + 1.0 / max(s, 0.01);
		if (d > 1000.0) break; // 性能优化：距离太远跳出
	}

	// 3. 法线计算 (Tetrahedron technique)
	const float h = 0.005;
	const vec2 k = vec2(1, -1);
	vec3 n = N(k.xyy * map(p_pos + k.xyy * h) +
		k.yyx * map(p_pos + k.yyx * h) +
		k.yxy * map(p_pos + k.yxy * h) +
		k.xxx * map(p_pos + k.xxx * h));

	// 4. 漫反射
	o *= (.1 + max(dot(n, -D), 0.));

	// 5. 反射步进 (Reflection march)
	vec4 ref = vec4(0);
	lights = vec4(0);
	float i2 = 0.0;
	for (p_pos += n * .05, D = reflect(D, n), s = 0.0; i2 < 50.0; i2++) {
		p_pos += D * s;
		s = map(p_pos) * 0.8;
		ref += lights + 1.0 / max(s, 0.01);
		if (length(p_pos - ro) > 500.0) break;
	}

	// 6. 最终颜色合并
	o += o * ref;
	// 使用 tanh 进行色调映射 (Tone Mapping)
	o = vec4(tanh(o.rgb / 1e9 * exp(vec3(1e1, 2, 1) * d / 5e2)), 1.0);

	color = o;
}
