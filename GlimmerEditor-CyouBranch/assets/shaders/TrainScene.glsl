#type vertex
#version 330 core
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoord;
uniform mat4 u_ViewProjection;
uniform mat4 u_Transform;
out vec2 v_TexCoord;
void main() {
	v_TexCoord = a_TexCoord;
	gl_Position = u_ViewProjection * u_Transform * vec4(a_Position, 1.0);
}

#type fragment
#version 330 core

layout(location = 0) out vec4 color;
in vec2 v_TexCoord;

uniform float u_Time;
uniform vec2 u_Resolution;
uniform sampler2D u_NoiseTexture;

// --- 基础数学与工具函数 ---
#define dot2(v) dot(v, v)

float hash(vec2 p) {
	return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

// 程序化替代 iChannel0 的采样
float noise(vec2 x) {
	return texture(u_NoiseTexture, x / 256.0).r;
}

float fbm(vec2 x, int detail) {
	float a = 0.0;
	float b = 1.0;
	float t = 0.0;
	for (int i = 0; i < detail; i++) {
		float n = noise(x);
		a += b * n;
		t += b;
		b *= 0.7;
		x *= 2.0;
	}
	return a / t;
}

float fbm2(vec2 x, int detail) {
	float a = 0.0;
	float b = 1.0;
	float t = 0.0;
	for (int i = 0; i < detail; i++) {
		float n = noise(x);
		a += b * n;
		t += b;
		b *= 0.9;
		x *= 2.0;
	}
	return a / t;
}

float box(vec2 uv, float x1, float x2, float y1, float y2) {
	return (uv.x > x1 && uv.x < x2 && uv.y > y1 && uv.y < y2) ? 1.0 : 0.0;
}

// 模拟层渲染逻辑
vec3 applyLayer(vec2 uv, float h, float midlevel, float dh, vec3 layerCol, vec3 lastCol) {
	if (uv.y < h + midlevel - dh) return layerCol;
	return lastCol;
}

// --- 场景组件：背景云 ---
vec4 getBackground(vec2 uv, float t) {
	vec3 col = vec3(0.58, 0.7, 1.0); // 初始天空色
	vec2 uv2; float h;

	// 简化后的云层渲染 (c1 - c12 的逻辑抽象)
	// 这里为了性能抽取几个代表性层级
	struct CloudLayer { float mid; float disp; float dist; float off; vec3 c; };
	CloudLayer layers[3] = CloudLayer[](
		CloudLayer(0.3, 0.9, 10.0, 32.5, vec3(0.95, 0.45, 0.30)),
		CloudLayer(0.5, 2.3, 30.0, 20.5, vec3(0.99, 0.29, 0.20)),
		CloudLayer(0.8, 2.7, 60.0, 9.5, vec3(1.0, 0.76, 0.60))
		);

	for (int i = 0; i < 3; i++) {
		uv2 = uv + vec2(t / layers[i].dist + layers[i].off, 0.0);
		h = (fbm(uv2, 5) - 0.5) * layers[i].disp;
		if (uv.y < h + layers[i].mid) col = layers[i].c;
	}
	return vec4(col, 1.0);
}

// --- 场景组件：前景云 ---
vec4 getForeground(vec2 uv, float t) {
	uv.y -= 0.2;
	float dist = 1.0;
	vec2 uv2 = uv + vec2(t / dist + 40.0, 0.0);
	float h = (fbm(uv2, 6) - 0.5) * 1.7;
	if (uv.y < h - 0.1) return vec4(0.77, 0.48, 0.46, 1.0);
	return vec4(0.0);
}

void main() {
	// 对齐坐标系：Shadertoy 使用 y 轴归一化
	vec2 fragCoord = gl_FragCoord.xy;
	vec2 uv = fragCoord / u_Resolution.y;
	float t = u_Time * 4.0;

	// 1. 渲染背景
	vec4 bg = getBackground(uv, t);
	vec3 finalCol = bg.rgb;

	// 2. 渲染火车 (利用 step 和 box 函数手动绘制几何体)
	vec2 trainUV = uv;
	trainUV.y -= 0.2;
	float loco = box(trainUV, 0.45, 0.5, 0.103, 0.112);
	finalCol = mix(finalCol, vec3(0.38, 0.19, 0.20), loco);

	// 3. 渲染烟雾
	vec2 smokeUV = trainUV + vec2(t / 5.0 + 3.5, 0.0);
	smokeUV.x -= t / 5.0 * 0.2;
	float h_smoke = fbm2(smokeUV, 5) - 0.55;
	if (trainUV.x < 0.49) {
		float x = -trainUV.x + 0.49;
		float y = abs(trainUV.y + h_smoke * 0.4 - 0.16 * sqrt(x) - 0.12) - 0.8 * x * exp(-x * 10.0);
		if (y < 0.0) finalCol = vec3(1.0, 0.94, 0.91);
	}

	// 4. 叠加前景
	vec4 fg = getForeground(uv, t);
	finalCol = mix(finalCol, fg.rgb, fg.a);

	// 5. 后期：Vignette 暗角
	vec2 screenUV = fragCoord / u_Resolution.xy;
	float vig = 16.0 * screenUV.x * screenUV.y * (1.0 - screenUV.x) * (1.0 - screenUV.y);
	finalCol *= 0.5 + 0.5 * pow(vig, 0.2);

	color = vec4(finalCol, 1.0);
}
