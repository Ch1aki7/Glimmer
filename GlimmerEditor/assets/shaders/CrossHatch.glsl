#type vertex
#version 330 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec3 a_Tangent;
layout(location = 3) in vec2 a_TexCoord;

uniform mat4 u_ViewProjection;
uniform mat4 u_Transform;

out vec2 v_TexCoord;
out vec3 v_WorldPos;
out mat3 v_TBN;

void main()
{
	v_TexCoord = a_TexCoord;
	v_WorldPos = vec3(u_Transform * vec4(a_Position, 1.0));

	vec3 N = normalize(mat3(transpose(inverse(u_Transform))) * a_Normal);
	vec3 T = normalize(mat3(u_Transform) * a_Tangent);
	T = normalize(T - N * dot(N, T));
	vec3 B = cross(N, T);
	v_TBN = mat3(T, B, N);

	gl_Position = u_ViewProjection * vec4(v_WorldPos, 1.0);
}

#type fragment
#version 330 core

layout(location = 0) out vec4 color;

in vec2 v_TexCoord;
in vec3 v_WorldPos;
in mat3 v_TBN;

uniform sampler2D u_Texture;
uniform sampler2D u_NormalMap;
uniform bool    u_UseNormalMap;
uniform vec3 u_LightPos;
uniform vec3 u_LightColor;
uniform vec3 u_ViewPos;

float hatchLine(vec2 uv, float angle, float density)
{
	float s = sin(angle), c = cos(angle);
	float d = uv.x * c - uv.y * s;
	return smoothstep(0.0, density, abs(fract(d * 12.0) - 0.5) * 2.0);
}

void main()
{
	vec3 norm;
	if (u_UseNormalMap)
		norm = normalize(v_TBN * (texture(u_NormalMap, v_TexCoord).rgb * 2.0 - 1.0));
	else
		norm = normalize(v_TBN[2]);

	vec3 lightDir = normalize(u_LightPos - v_WorldPos);
	vec3 viewDir = normalize(u_ViewPos - v_WorldPos);

	float diff = max(dot(norm, lightDir), 0.0);

	float rim = 1.0 - max(dot(viewDir, norm), 0.0);
	float outline = smoothstep(10.35, 10.55, rim);

	float hatch1 = hatchLine(v_TexCoord, 0.785, 0.12);
	float hatch2 = hatchLine(v_TexCoord, -0.785, 0.12);
	float hatch3 = hatchLine(v_TexCoord, 0.0, 0.08);

	float hatching = 1.0;
	if (diff < 0.7) hatching = min(hatching, hatch1);
	if (diff < 0.45) hatching = min(hatching, hatch2);
	if (diff < 0.2)  hatching = min(hatching, hatch3);

	vec4 texColor = texture(u_Texture, v_TexCoord);

	vec3 paper = vec3(0.95, 0.93, 0.88);
	vec3 ink = vec3(0.08, 0.06, 0.04);

	vec3 result = mix(paper * texColor.rgb, ink, 1.0 - hatching);
	result = mix(result, ink, outline);

	color = vec4(result, texColor.a);
}
