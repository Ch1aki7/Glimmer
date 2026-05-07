#type vertex
#version 330 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;

uniform mat4 u_ViewProjection;
uniform mat4 u_Transform;

out vec2 v_TexCoord;
out vec3 v_Normal;
out vec3 v_WorldPos;

void main()
{
	v_TexCoord = a_TexCoord;
	// 修正法线矩阵：处理缩放导致的法线不垂直问题
	v_Normal = mat3(transpose(inverse(u_Transform))) * a_Normal;
	v_WorldPos = vec3(u_Transform * vec4(a_Position, 1.0));
	gl_Position = u_ViewProjection * vec4(v_WorldPos, 1.0);
}

#type fragment
#version 330 core

layout(location = 0) out vec4 color;

in vec2 v_TexCoord;
in vec3 v_Normal;
in vec3 v_WorldPos;

uniform sampler2D u_Texture;
uniform vec3 u_LightPos;
uniform vec3 u_LightColor;
uniform vec3 u_ViewPos;

void main()
{
	// ✨ 核心修正：重新归一化
	vec3 norm = normalize(v_Normal);
	vec3 lightDir = normalize(u_LightPos - v_WorldPos);
	vec3 viewDir = normalize(u_ViewPos - v_WorldPos);

	float NdotL = dot(norm, lightDir);
	float NdotV = dot(norm, viewDir);

	// --- 1. 描边逻辑 (Outline) ---
	// [参数1: 0.35, 0.4] 菲涅尔宽度控制
	float fresnel = 1.0 - max(NdotV, 0.0);
	float edgeWidth = 0.05;       // 描边宽度
	float edgeStart = 10.35;       // 从哪里开始亮
	float edgeOutline = smoothstep(edgeStart, edgeStart + edgeWidth, fresnel);

	// [参数2: -0.1, 0.0] 背光面阴影线
	// 模拟漫画中背光一侧加粗的笔触
	float shadowEdge = smoothstep(-0.1, 0.05, -NdotL);

	// 融合两种描边
	float outline = max(edgeOutline, shadowEdge * 0.5);

	// --- 2. 色阶平涂 (Posterization) ---
	// 使用 smoothstep 代替 if 产生微小的过度，防止像素锯齿
	float diff = NdotL * 0.5 + 0.5; // 半兰伯特，让色阶分布更均匀
	float shade;

	// [参数3: 0.2, 0.5, 0.8] 决定明暗交界线的位置
	if (diff > 0.8)      shade = 1.1;  // 极亮
	else if (diff > 0.5)  shade = 0.9;  // 亮
	else if (diff > 0.25) shade = 0.6;  // 灰
	else                  shade = 0.4;  // 暗

	// --- 3. 颜色合成 ---
	vec4 texColor = texture(u_Texture, v_TexCoord);

	// [参数4: ink颜色] 建议深紫色或深蓝色，比纯黑更有高级感
	vec3 inkColor = vec3(0.05, 0.04, 0.08);

	// 计算基础光照颜色
	vec3 baseColor = texColor.rgb * shade * u_LightColor;

	// 最终混合：在基础色和墨色之间切换
	vec3 finalRGB = mix(baseColor, inkColor, outline);

	// 饱和度微调 (可选)
	finalRGB = mix(vec3(dot(finalRGB, vec3(0.299, 0.587, 0.114))), finalRGB, 1.2);

	color = vec4(finalRGB, texColor.a);
}
