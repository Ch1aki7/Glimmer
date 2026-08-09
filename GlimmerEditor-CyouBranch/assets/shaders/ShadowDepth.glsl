#type vertex
#version 450 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TerrainTexCoord;
layout(location = 3) in vec2 a_ModelTexCoord;
layout(location = 4) in mat4 a_InstanceTransform;

uniform mat4 u_LightViewProjection;
uniform mat4 u_Transform;
uniform sampler2D u_HeightMap;
uniform float u_MaxHeight;
uniform int u_IsTerrain;
uniform int u_UseInstancing;

layout(location = 0) out vec2 v_TexCoord;

void main()
{
	vec3 localPosition = a_Position;
	if (u_IsTerrain != 0)
		localPosition.y = texture(u_HeightMap, a_TerrainTexCoord).r * u_MaxHeight;
	v_TexCoord = u_IsTerrain != 0 ? a_TerrainTexCoord : a_ModelTexCoord;
	mat4 transform = u_UseInstancing != 0 ? a_InstanceTransform : u_Transform;
	gl_Position = u_LightViewProjection * transform * vec4(localPosition, 1.0);
}

#type fragment
#version 450 core

layout(location = 0) in vec2 v_TexCoord;

uniform sampler2D u_BaseColorTexture;
uniform int u_AlphaMaskEnabled;
uniform int u_HasBaseColorTexture;
uniform float u_BaseColorAlpha;
uniform float u_AlphaCutoff;
uniform float u_TilingFactor;

void main()
{
	if (u_AlphaMaskEnabled != 0)
	{
		float textureAlpha = u_HasBaseColorTexture != 0
			? texture(u_BaseColorTexture, v_TexCoord * u_TilingFactor).a
			: 1.0;
		if (clamp(u_BaseColorAlpha * textureAlpha, 0.0, 1.0) < u_AlphaCutoff)
			discard;
	}
}
