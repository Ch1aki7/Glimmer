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

// 单方向排线：沿 angle 角度画等间距线条
float hatchLine(vec2 uv, float angle, float density)
{
	float s = sin(angle), c = cos(angle);
	float d = uv.x * c - uv.y * s;        // 旋转后的坐标
	return smoothstep(0.0, density, abs(fract(d * 12.0) - 0.5) * 2.0);
}

void main()
{
	vec3 norm = normalize(v_Normal);
	vec3 lightDir = normalize(u_LightPos - v_WorldPos);
	vec3 viewDir = normalize(u_ViewPos - v_WorldPos);

	// 漫反射强度决定排线密度
	float diff = max(dot(norm, lightDir), 0.0);

	// 菲涅尔边缘描边
	float rim = 1.0 - max(dot(viewDir, norm), 0.0);
	float outline = smoothstep(10.35, 10.55, rim);

	// 三层排线：越暗区域排线越密、角度越交叉
	float hatch1 = hatchLine(v_TexCoord, 0.785, 0.12);           // 45度 稀疏
	float hatch2 = hatchLine(v_TexCoord, -0.785, 0.12);          // -45度 中等
	float hatch3 = hatchLine(v_TexCoord, 0.0, 0.08);             // 水平 密集

	float hatching = 1.0;
	if (diff < 0.7) hatching = min(hatching, hatch1);
	if (diff < 0.45) hatching = min(hatching, hatch2);
	if (diff < 0.2)  hatching = min(hatching, hatch3);

	vec4 texColor = texture(u_Texture, v_TexCoord);

	// 纸张底色 + 排线叠加 + 边缘描边
	vec3 paper = vec3(0.95, 0.93, 0.88);
	vec3 ink = vec3(0.08, 0.06, 0.04);

	vec3 result = mix(paper * texColor.rgb, ink, 1.0 - hatching);
	result = mix(result, ink, outline);

	color = vec4(result, texColor.a);
}
