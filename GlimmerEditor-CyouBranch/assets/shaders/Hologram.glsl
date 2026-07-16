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

void main() {
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

uniform sampler2D u_NormalMap;
uniform bool    u_UseNormalMap;
uniform vec3 u_ViewPos;
uniform vec3 u_LightColor;
uniform float u_Time;

void main() {
	vec3 norm;
	if (u_UseNormalMap)
		norm = normalize(v_TBN * (texture(u_NormalMap, v_TexCoord).rgb * 2.0 - 1.0));
	else
		norm = normalize(v_TBN[2]);

	vec3 viewDir = normalize(u_ViewPos - v_WorldPos);

	float fresnel = 1.0 - max(dot(norm, viewDir), 0.0);
	fresnel = pow(fresnel, 3.0);

	float scanline = sin(v_WorldPos.y * 800.0 + u_Time * 5.0) * 0.1 + 0.9;

	vec3 baseColor = u_LightColor;
	vec3 finalColor = baseColor * fresnel * scanline;

	float alpha = fresnel * 0.8;

	color = vec4(finalColor, alpha);
}
