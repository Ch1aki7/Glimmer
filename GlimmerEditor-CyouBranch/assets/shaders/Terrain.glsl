#type vertex
#version 450 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoord;

uniform mat4 u_ViewProjection;
uniform mat4 u_Transform;
uniform int u_EntityID;
uniform sampler2D u_HeightMap;
uniform sampler2D u_NormalSlopeMap;
uniform sampler2D u_TerrainAnalysisMap;
uniform sampler2D u_MaterialWeightMap;
uniform int u_HasDerivedMaps;
uniform float u_MaxHeight;
uniform float u_UVScale;
uniform vec2 u_TexelSize;
uniform float u_SampleSpacing;

out vec3 v_WorldPos;
out vec3 v_Normal;
out float v_Height;
out vec2 v_TerrainAnalysis;
out vec4 v_MaterialWeights;
flat out int v_EntityID;

float SampleHeight(vec2 uv)
{
	vec2 sampleUV = clamp(uv, u_TexelSize * 0.5, vec2(1.0) - u_TexelSize * 0.5);
	return texture(u_HeightMap, sampleUV).r;
}

void main()
{
	vec2 uv = a_TexCoord * u_UVScale;
	float height = SampleHeight(uv);
	float heightLeft = SampleHeight(uv - vec2(u_TexelSize.x, 0.0));
	float heightRight = SampleHeight(uv + vec2(u_TexelSize.x, 0.0));
	float heightDown = SampleHeight(uv - vec2(0.0, u_TexelSize.y));
	float heightUp = SampleHeight(uv + vec2(0.0, u_TexelSize.y));

	vec3 worldPosition = a_Position;
	worldPosition.y = height * u_MaxHeight;

	float sampleSpacing = max(u_SampleSpacing, 0.0001);
	vec3 normal = u_HasDerivedMaps != 0
		? normalize(texture(u_NormalSlopeMap, uv).xyz * 2.0 - 1.0)
		: normalize(vec3(
			(heightLeft - heightRight) * u_MaxHeight / (2.0 * sampleSpacing),
			1.0,
			(heightDown - heightUp) * u_MaxHeight / (2.0 * sampleSpacing)));

	vec4 transformedPosition = u_Transform * vec4(worldPosition, 1.0);
	v_WorldPos = transformedPosition.xyz;
	v_Normal = normalize(transpose(inverse(mat3(u_Transform))) * normal);
	v_Height = height;
	v_TerrainAnalysis = u_HasDerivedMaps != 0
		? texture(u_TerrainAnalysisMap, uv).rg : vec2(0.5, 0.0);
	v_MaterialWeights = u_HasDerivedMaps != 0
		? texture(u_MaterialWeightMap, uv) : vec4(0.0);
	v_EntityID = u_EntityID;
	gl_Position = u_ViewProjection * transformedPosition;
}

#type fragment
#version 450 core

layout(location = 0) out vec4 color;
layout(location = 1) out int entityID;

in vec3 v_WorldPos;
in vec3 v_Normal;
in float v_Height;
in vec2 v_TerrainAnalysis;
in vec4 v_MaterialWeights;
flat in int v_EntityID;

uniform vec3 u_CameraPos;
uniform int u_HasDerivedMaps;

struct PointLightData
{
	vec4 PositionRange;
	vec4 ColorIntensity;
};

layout(std140, binding = 1) uniform LightEnvironment
{
	vec4 u_DirectionalDirectionIntensity;
	vec4 u_DirectionalColor;
	vec4 u_AmbientColorIntensity;
	uvec4 u_LightCounts;
	PointLightData u_PointLights[16];
};

vec3 EvaluateLight(vec3 normal, vec3 viewDirection, vec3 lightDirection,
	vec3 radiance, vec3 baseColor)
{
	float diffuse = max(dot(normal, lightDirection), 0.0);
	vec3 halfwayDirection = normalize(lightDirection + viewDirection);
	float specular = pow(max(dot(normal, halfwayDirection), 0.0), 32.0) * 0.15;
	return baseColor * diffuse * radiance + specular * radiance;
}

void main()
{
	vec3 grass = vec3(0.15, 0.55, 0.15);
	vec3 soil = vec3(0.30, 0.20, 0.10);
	vec3 rock = vec3(0.45, 0.40, 0.35);
	vec3 snow = vec3(0.92, 0.92, 0.96);

	vec3 baseColor;
	if (u_HasDerivedMaps != 0)
	{
		vec4 weights = max(v_MaterialWeights, vec4(0.0));
		weights /= max(dot(weights, vec4(1.0)), 0.0001);
		baseColor = grass * weights.x + soil * weights.y
			+ rock * weights.z + snow * weights.w;
		baseColor *= mix(1.0, 0.82, clamp(v_TerrainAnalysis.y, 0.0, 1.0) * 0.35);
	}
	else
	{
		float rockBlend = smoothstep(0.05, 0.35, v_Height);
		float snowBlend = smoothstep(0.55, 0.80, v_Height);
		baseColor = mix(grass, rock, rockBlend);
		baseColor = mix(baseColor, snow, snowBlend);
	}
	baseColor = pow(max(baseColor, vec3(0.0)), vec3(2.2));

	vec3 normal = normalize(v_Normal);
	vec3 viewDirection = normalize(u_CameraPos - v_WorldPos);
	vec3 result = baseColor * u_AmbientColorIntensity.rgb * u_AmbientColorIntensity.a;

	float directionalIntensity = u_DirectionalDirectionIntensity.w;
	if (directionalIntensity > 0.0)
	{
		vec3 lightDirection = normalize(-u_DirectionalDirectionIntensity.xyz);
		vec3 radiance = u_DirectionalColor.rgb * directionalIntensity;
		result += EvaluateLight(normal, viewDirection, lightDirection, radiance, baseColor);
	}

	uint pointLightCount = min(u_LightCounts.x, 16u);
	for (uint index = 0u; index < pointLightCount; ++index)
	{
		vec3 toLight = u_PointLights[index].PositionRange.xyz - v_WorldPos;
		float distanceToLight = length(toLight);
		float range = max(u_PointLights[index].PositionRange.w, 0.01);
		if (distanceToLight >= range)
			continue;

		vec3 lightDirection = toLight / max(distanceToLight, 0.0001);
		float normalizedDistance = distanceToLight / range;
		float rangeFalloff = max(1.0 - normalizedDistance * normalizedDistance, 0.0);
		float attenuation = rangeFalloff * rangeFalloff
			/ max(distanceToLight * distanceToLight, 1.0);
		vec3 radiance = u_PointLights[index].ColorIntensity.rgb
			* u_PointLights[index].ColorIntensity.w * attenuation;
		result += EvaluateLight(normal, viewDirection, lightDirection, radiance, baseColor);
	}

	color = vec4(result, 1.0);
	entityID = v_EntityID;
}
