#type vertex
#version 330 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoord;

uniform mat4 u_ViewProjection;
uniform mat4 u_Transform;
uniform float u_Time;

out vec3 v_Position;
out vec2 v_TexCoord;

void main()
{
	v_TexCoord = a_TexCoord;
	vec3 pos = a_Position;
	pos.y += sin(pos.x * 5.0 + u_Time) * 0.1;
	v_Position = pos;
	gl_Position = u_ViewProjection * u_Transform * vec4(pos, 1.0);
}

#type fragment
#version 330 core

layout(location = 0) out vec4 color;

in vec3 v_Position;
in vec2 v_TexCoord;

uniform sampler2D u_Texture;
uniform float u_Time;

void main()
{
	vec3 col;
	// 使用三角函数让 R, G, B 三个通道随位置和时间发生不同的相位偏移
	col.r = sin(v_Position.x * 3.0 + u_Time) * 0.5 + 0.5;
	col.g = sin(v_Position.y * 3.0 + u_Time + 2.0) * 0.5 + 0.5;
	col.b = sin((v_Position.x + v_Position.y) * 3.0 + u_Time + 4.0) * 0.5 + 0.5;
	color = vec4(col, 1.0);
	color *= texture(u_Texture, v_TexCoord);
}
