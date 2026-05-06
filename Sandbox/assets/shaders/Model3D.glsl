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
	// 1. 环境光 (Ambient) - 保证没光的地方不是全黑
	float ambientStrength = 0.6;
	vec3 ambient = ambientStrength * u_LightColor;

	// 2. 漫反射 (Diffuse) - 根据物体朝向光的角度决定亮度
	vec3 norm = normalize(v_Normal);
	vec3 lightDir = normalize(u_LightPos - v_WorldPos);
	float diff = max(dot(norm, lightDir), 0.0);
	vec3 diffuse = diff * u_LightColor;

	// 3. 高光 (Specular) - 金属或光滑表面的反光
	float specularStrength = 0.5;
	vec3 viewDir = normalize(u_ViewPos - v_WorldPos);
	vec3 reflectDir = reflect(-lightDir, norm);
	float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32); // 32 是发光反光度
	vec3 specular = specularStrength * spec * u_LightColor;

	// 结合贴图颜色
	vec4 texColor = texture(u_Texture, v_TexCoord);
	vec3 result = (ambient + diffuse + specular) * texColor.rgb;

	color = vec4(result, texColor.a);
}
