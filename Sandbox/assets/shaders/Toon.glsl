#type vertex
#version 330 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal; // 之前在 Mesh 里存好的法线
layout(location = 2) in vec2 a_TexCoord;

uniform mat4 u_ViewProjection;
uniform mat4 u_Transform;

out vec2 v_TexCoord;
out vec3 v_Normal;
out vec3 v_WorldPos; // 传出世界坐标，用于计算光线方向

void main()
{
	v_TexCoord = a_TexCoord;

	// 核心：法线也需要旋转。使用“法线矩阵”防止非等比缩放导致法线畸变
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
uniform vec3 u_LightPos;    // 光源位置
uniform vec3 u_LightColor;  // 灯光颜色
uniform vec3 u_ViewPos;     // 摄像机位置（用于高光）

void main()
{
	vec3 norm = normalize(v_Normal);
	vec3 lightDir = normalize(u_LightPos - v_WorldPos);
	vec3 viewDir = normalize(u_ViewPos - v_WorldPos);

	// 1. 核心：将漫反射强度“阶梯化”
	float diff = dot(norm, lightDir);
	float intensity = smoothstep(0.0, 0.05, diff) * 0.5 +
		smoothstep(0.4, 0.45, diff) * 0.5; // 只有两层亮度

	vec3 diffuse = intensity * u_LightColor;

	// 2. 边缘光 (Rim Light)：在物体轮廓处产生发光感
	float rim = 1.0 - max(dot(viewDir, norm), 0.0);
	rim = pow(rim, 4.0); // 调整边缘光的细度
	vec3 rimColor = u_LightColor * rim * 0.5;

	vec4 texColor = texture(u_Texture, v_TexCoord);
	// 卡通色块 + 基础环境光 + 边缘光
	vec3 result = (vec3(0.3) + diffuse + rimColor) * texColor.rgb;

	color = vec4(result, texColor.a);
}
