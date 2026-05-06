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

void main() {
	v_TexCoord = a_TexCoord;

	// 计算法线矩阵，保证旋转后法线方向正确
	v_Normal = mat3(transpose(inverse(u_Transform))) * a_Normal;

	// 计算世界空间坐标
	v_WorldPos = vec3(u_Transform * vec4(a_Position, 1.0));

	// 最终投影位置
	gl_Position = u_ViewProjection * vec4(v_WorldPos, 1.0);
}

#type fragment
#version 330 core

layout(location = 0) out vec4 color;

in vec2 v_TexCoord;
in vec3 v_Normal;
in vec3 v_WorldPos;

// 在这里声明摄像机位置 Uniform
uniform vec3 u_ViewPos;
uniform vec3 u_LightColor;
uniform float u_Time;

void main() {
	vec3 norm = normalize(v_Normal);

	// 计算视线方向：从物体表面指向摄像机
	vec3 viewDir = normalize(u_ViewPos - v_WorldPos);

	// 1. 菲涅尔公式 (Fresnel)
	// dot(norm, viewDir) 越大说明面正对着你，1.0 - dot 越大说明是边缘
	float fresnel = 1.0 - max(dot(norm, viewDir), 0.0);
	fresnel = pow(fresnel, 3.0); // 3.0 决定了发光边的厚度，数值越大边越细

	// 2. 动态扫描线 (扫描线随时间上下移动)
	// 使用 sin 函数配合世界坐标 Y 轴
	float scanline = sin(v_WorldPos.y * 800.0 + u_Time * 5.0) * 0.1 + 0.9;

	// 3. 颜色组合
	// 使用传入的灯光颜色作为全息投影的底色
	vec3 baseColor = u_LightColor;
	vec3 finalColor = baseColor * fresnel * scanline;

	// 透明度处理：边缘实，中心透
	// 注意：要看到透明效果，C++ 端必须开启 glEnable(GL_BLEND)
	float alpha = fresnel * 0.8;

	color = vec4(finalColor, alpha);
}
