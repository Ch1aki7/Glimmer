#type vertex
#version 330 core

layout(location = 0) in vec3 a_Position;
uniform mat4 u_ViewProjection;
uniform mat4 u_Transform;

out vec2 v_Position;

void main()
{
	v_Position = a_Position.xy;
	// 顶点位置保持原始，不进行物理位移，确保背景铺满
	gl_Position = u_ViewProjection * u_Transform * vec4(a_Position, 1.0);
}

#type fragment
#version 330 core

layout(location = 0) out vec4 color;
in vec2 v_Position;

uniform float u_Time;
uniform float u_VortexAmt; // 对应第二段代码的 vortex_amt (建议范围 -5.0 到 5.0)

// --- 基础噪声函数 ---
float hash(vec2 p) {
	p = fract(p * vec2(123.34, 456.21));
	p += dot(p, p + 45.32);
	return fract(p.x * p.y);
}

float noise(vec2 p) {
	vec2 i = floor(p);
	vec2 f = fract(p);
	float a = hash(i);
	float b = hash(i + vec2(1.0, 0.0));
	float c = hash(i + vec2(0.0, 1.0));
	float d = hash(i + vec2(1.0, 1.0));
	vec2 u = f * f * (3.0 - 2.0 * f);
	return mix(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}

float fbm(vec2 p) {
	float v = 0.0;
	float a = 0.5;
	mat2 rot = mat2(1.6, 1.2, -1.2, 1.6);
	for (int i = 0; i < 5; i++) {
		v += a * noise(p);
		p = rot * p * 2.0;
		a *= 0.5;
	}
	return v;
}

void main()
{
	// 1. 获取基础坐标 (假设 v_Position 是相对于中心的)
	vec2 uv = v_Position;
	float r = length(uv);
	float angle = atan(uv.y, uv.x);

	// 2. 融合：第二段代码的 Smoothstep 扭曲逻辑
	// 控制旋转半径和角度
	float effectRadius = 2.0;
	float twist = u_VortexAmt * smoothstep(effectRadius, 0.0, r);

	// 3. 加入“不规则正弦”效果 (波浪抖动)
	// 利用 sin 让漩涡边缘产生不规则的起伏
	float wobble = sin(r * 10.0 - u_Time * 2.0) * 0.05;

	// 最终角度 = 原始角度 + 强度扭曲 + 动态旋转 + 波浪抖动
	float finalAngle = angle + twist + (u_Time * 0.2) + wobble;

	// 4. 将扭曲后的极坐标转回平面坐标，作为颜料噪声的输入
	vec2 twistedUV = vec2(cos(finalAngle), sin(finalAngle)) * r;

	// 5. 领域扭曲 (Balatro 核心颜料算法)
	float n = fbm(twistedUV * 3.0 + u_Time * 0.1);
	float m = fbm(twistedUV * 2.0 + n + u_Time * 0.05);

	// 6. 颜色混合 (红蓝艺术配色)
	vec3 colorRed = vec3(0.85, 0.15, 0.2);   // 鲜亮红
	vec3 colorBlue = vec3(0.1, 0.25, 0.75);  // 宝石蓝
	vec3 darkColor = vec3(0.1, 0.05, 0.15);
	vec3 darkRed = vec3(0.2, 0.0, 0.0);   // 偏黑红
	vec3 darkBlue = vec3(0.0, 0.0, 0.2);  // 偏黑蓝
	vec3 darkGreen = vec3(0.0, 0.2, 0.0); // 偏黑绿
	vec3 deepBlue = vec3(0.05, 0.05, 0.3); // 深蓝，略带一点暗

	// 用最终噪声值 m 混合，并加入高光亮边
	vec3 finalCol = mix(colorRed, colorBlue, m);
	finalCol += smoothstep(0.75, 1.0, m) * 0.25; // 增加白色颜料反光

	// 7. 边缘压暗 (Vignette)
	finalCol *= smoothstep(1.8, 0.5, r);

	color = vec4(finalCol, 1.0);
}
