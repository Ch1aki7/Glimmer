#type vertex
#version 450 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoord;

uniform mat4 u_LightViewProjection;
uniform mat4 u_Transform;
uniform sampler2D u_HeightMap;
uniform float u_MaxHeight;
uniform int u_IsTerrain;

void main()
{
	vec3 localPosition = a_Position;
	if (u_IsTerrain != 0)
		localPosition.y = texture(u_HeightMap, a_TexCoord).r * u_MaxHeight;
	gl_Position = u_LightViewProjection * u_Transform * vec4(localPosition, 1.0);
}

#type fragment
#version 450 core

void main()
{
}
