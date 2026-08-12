#type vertex
#version 450 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TerrainTexCoord;
layout(location = 2) in float a_TerrainSkirt;
layout(location = 3) in vec2 a_ModelTexCoord;
layout(location = 4) in mat4 a_InstanceTransform;

uniform mat4 u_LightViewProjection;
uniform mat4 u_Transform;
uniform sampler2D u_HeightMap;
uniform float u_MaxHeight;
uniform int u_IsTerrain;
uniform int u_UseInstancing;
uniform vec2 u_ChunkUVOffset;
uniform vec2 u_ChunkUVScale;
uniform vec2 u_ChunkLocalOffset;
uniform float u_ChunkLocalScale;
uniform float u_SkirtDepth;

layout(location = 0) out vec2 v_TexCoord;

void main()
{
	vec3 localPosition = a_Position;
	if (u_IsTerrain != 0)
	{
		vec2 terrainUV = u_ChunkUVOffset
			+ a_TerrainTexCoord * u_ChunkUVScale;
		localPosition.xz = a_Position.xz * u_ChunkLocalScale
			+ u_ChunkLocalOffset;
		localPosition.y = texture(u_HeightMap, terrainUV).r * u_MaxHeight;
		localPosition.y -= a_TerrainSkirt * u_SkirtDepth;
		v_TexCoord = terrainUV;
	}
	else
		v_TexCoord = a_ModelTexCoord;
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
