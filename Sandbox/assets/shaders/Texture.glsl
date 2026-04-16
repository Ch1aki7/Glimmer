#type vertex
#version 330 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoord;

uniform mat4 u_ViewProjection;
uniform mat4 u_Transform;

out vec2 v_TexCoord;

void main()
{
	v_TexCoord = a_TexCoord;
	gl_Position = u_ViewProjection * u_Transform * vec4(a_Position, 1.0);
}

#type fragment
#version 330 core

layout(location = 0) out vec4 color;

in vec2 v_TexCoord;

uniform vec4 u_Color;
uniform sampler2D u_Texture;
uniform float u_TilingFactor;

void main()
{
	// UV 坐标乘以平铺系数，实现贴图重复（需贴图设为 GL_REPEAT）
	vec4 texColor = texture(u_Texture, v_TexCoord * u_TilingFactor) * u_Color;

	// 如果透明度低于一个很小的阈值，直接扔掉这个像素
	// 这样它就不会去更新深度缓冲区了
	if (texColor.a < 0.1)
		discard;

	color = texColor;
}
