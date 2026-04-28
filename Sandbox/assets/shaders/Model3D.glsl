#type vertex
#version 330 core
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;

uniform mat4 u_ViewProjection;
uniform mat4 u_Transform; // 3D模型需要单独的模型矩阵

out vec2 v_TexCoord;

void main() {
	v_TexCoord = a_TexCoord;
	// 标准 3D 变换公式
	gl_Position = u_ViewProjection * u_Transform * vec4(a_Position, 1.0);
}

#type fragment
#version 330 core
layout(location = 0) out vec4 color;
in vec2 v_TexCoord;
uniform sampler2D u_Texture;

void main() {
	// 暂时只显示纹理，如果没有纹理就显示个纯色用于调试
	vec4 texColor = texture(u_Texture, v_TexCoord);
	color = texColor.a > 0.0 ? texColor : vec4(0.8, 0.8, 0.8, 1.0);
}
