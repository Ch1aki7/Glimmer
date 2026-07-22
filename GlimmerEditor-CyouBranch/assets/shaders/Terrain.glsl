#type vertex
#version 330 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoord;

uniform mat4 u_ViewProjection;
uniform sampler2D u_HeightMap;
uniform float u_MaxHeight;
uniform float u_UVScale;
uniform vec2 u_TexelSize;  // 1.0 / heightmap width, 1.0 / heightmap height

out vec3 v_WorldPos;
out vec3 v_Normal;
out float v_Height;

void main()
{
	vec2 uv = a_TexCoord * u_UVScale;
	float h = texture(u_HeightMap, uv).r;

	// 采样邻居高度 → 计算梯度 → 推导法线
	float hL = texture(u_HeightMap, uv - vec2(u_TexelSize.x, 0.0)).r;
	float hR = texture(u_HeightMap, uv + vec2(u_TexelSize.x, 0.0)).r;
	float hD = texture(u_HeightMap, uv - vec2(0.0, u_TexelSize.y)).r;
	float hU = texture(u_HeightMap, uv + vec2(0.0, u_TexelSize.y)).r;

	vec3 worldPos = a_Position;
	worldPos.y = h * u_MaxHeight;

	// 梯度（世界空间）：dx = 两邻居间距 1 格，dy = 高度差 * MaxHeight
	float gridSpacing = 1.0;  // 顶点间距
	vec3 normal = normalize(vec3(
		(hL - hR) * u_MaxHeight / (2.0 * gridSpacing),
		1.0,
		(hD - hU) * u_MaxHeight / (2.0 * gridSpacing)
	));

	v_WorldPos = worldPos;
	v_Normal   = normal;
	v_Height   = h;

	gl_Position = u_ViewProjection * vec4(worldPos, 1.0);
}

#type fragment
#version 330 core

layout(location = 0) out vec4 color;

in vec3 v_WorldPos;
in vec3 v_Normal;
in float v_Height;

uniform vec3 u_LightDir = vec3(0.5, 1.0, 0.3);
uniform vec3 u_CameraPos = vec3(0.0, 30.0, 0.0);

void main()
{
	// 高度混合材质
	vec3 grass  = vec3(0.15, 0.55, 0.15);
	vec3 rock   = vec3(0.45, 0.40, 0.35);
	vec3 snow   = vec3(0.92, 0.92, 0.96);

	float t1 = smoothstep(0.05, 0.35, v_Height);
	float t2 = smoothstep(0.55, 0.80, v_Height);
	vec3 baseColor = mix(grass, rock, t1);
	baseColor = mix(baseColor, snow, t2);

	// Phong 光照
	vec3 N = normalize(v_Normal);
	vec3 L = normalize(u_LightDir);
	vec3 V = normalize(u_CameraPos - v_WorldPos);
	vec3 H = normalize(L + V);

	float ambient  = 0.25;
	float diffuse  = max(dot(N, L), 0.0) * 0.6;
	float specular = pow(max(dot(N, H), 0.0), 16.0) * 0.15;

	vec3 lit = baseColor * (ambient + diffuse) + specular * vec3(1.0);
	color = vec4(lit, 1.0);
}
