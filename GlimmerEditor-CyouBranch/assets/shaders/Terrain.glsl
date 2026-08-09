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
	return texture(u_HeightMap,
		clamp(uv, u_TexelSize * 0.5, vec2(1.0) - u_TexelSize * 0.5)).r;
}

void main()
{
	vec2 uv = a_TexCoord * u_UVScale;
	float height = SampleHeight(uv);
	float leftHeight = SampleHeight(uv - vec2(u_TexelSize.x, 0.0));
	float rightHeight = SampleHeight(uv + vec2(u_TexelSize.x, 0.0));
	float downHeight = SampleHeight(uv - vec2(0.0, u_TexelSize.y));
	float upHeight = SampleHeight(uv + vec2(0.0, u_TexelSize.y));
	vec3 localPosition = a_Position;
	localPosition.y = height * u_MaxHeight;
	float spacing = max(u_SampleSpacing, 0.0001);
	vec3 localNormal = u_HasDerivedMaps != 0
		? normalize(texture(u_NormalSlopeMap, uv).xyz * 2.0 - 1.0)
		: normalize(vec3((leftHeight - rightHeight) * u_MaxHeight / (2.0 * spacing),
			1.0, (downHeight - upHeight) * u_MaxHeight / (2.0 * spacing)));
	vec4 worldPosition = u_Transform * vec4(localPosition, 1.0);
	v_WorldPos = worldPosition.xyz;
	v_Normal = normalize(transpose(inverse(mat3(u_Transform))) * localNormal);
	v_Height = height;
	v_TerrainAnalysis = u_HasDerivedMaps != 0
		? texture(u_TerrainAnalysisMap, uv).rg : vec2(0.5, 0.0);
	v_MaterialWeights = u_HasDerivedMaps != 0
		? texture(u_MaterialWeightMap, uv) : vec4(1.0, 0.0, 0.0, 0.0);
	v_EntityID = u_EntityID;
	gl_Position = u_ViewProjection * worldPosition;
}

#type fragment
#version 450 core

layout(location = 0) out vec4 o_Color;
layout(location = 1) out int o_EntityID;

in vec3 v_WorldPos;
in vec3 v_Normal;
in float v_Height;
in vec2 v_TerrainAnalysis;
in vec4 v_MaterialWeights;
flat in int v_EntityID;

struct PointLightData { vec4 PositionRange; vec4 ColorIntensity; };
layout(std140, binding = 1) uniform LightEnvironment
{
	vec4 u_DirectionalDirectionIntensity;
	vec4 u_DirectionalColor;
	vec4 u_AmbientColorIntensity;
	uvec4 u_LightCounts;
	PointLightData u_PointLights[16];
};

struct TerrainLayer
{
	vec3 BaseColor;
	float Tiling;
	float Metallic;
	float Roughness;
	float NormalScale;
	float AOStrength;
	int HasAlbedo;
	int HasNormal;
	int HasAO;
};

uniform vec3 u_CameraPos;
uniform TerrainLayer u_Layers[4];
uniform sampler2D u_AlbedoTextures[4];
uniform sampler2D u_NormalTextures[4];
uniform sampler2D u_AOTextures[4];
uniform float u_TriplanarSharpness;
uniform float u_WeightContrast;
uniform float u_HeightInfluence;
uniform float u_SlopeInfluence;
uniform float u_CurvatureInfluence;
uniform float u_MoistureInfluence;
uniform sampler2D u_ShadowMap;
uniform mat4 u_LightViewProjection;
uniform int u_ShadowEnabled;
uniform float u_ShadowBias;
uniform float u_ShadowTexelSize;

const float PI = 3.14159265359;

vec3 ProjectionWeights(vec3 normal)
{
	vec3 weights = pow(abs(normal), vec3(max(u_TriplanarSharpness, 1.0)));
	return weights / max(weights.x + weights.y + weights.z, 0.0001);
}

vec4 SampleTriplanar(sampler2D source, vec3 position, vec3 weights, float tiling)
{
	vec4 x = texture(source, position.zy * tiling);
	vec4 y = texture(source, position.xz * tiling);
	vec4 z = texture(source, position.xy * tiling);
	return x * weights.x + y * weights.y + z * weights.z;
}

vec3 SampleTriplanarNormal(sampler2D source, vec3 position,
	vec3 geometricNormal, vec3 weights, float tiling, float scale)
{
	vec3 x = texture(source, position.zy * tiling).xyz * 2.0 - 1.0;
	vec3 y = texture(source, position.xz * tiling).xyz * 2.0 - 1.0;
	vec3 z = texture(source, position.xy * tiling).xyz * 2.0 - 1.0;
	x.xy *= scale; y.xy *= scale; z.xy *= scale;
	x = vec3(x.z * sign(geometricNormal.x), x.y, x.x);
	y = vec3(y.x, y.z * sign(geometricNormal.y), y.y);
	z = vec3(z.x, z.y, z.z * sign(geometricNormal.z));
	return normalize(x * weights.x + y * weights.y + z * weights.z);
}

vec4 ResolveMaterialWeights(vec3 normal)
{
	vec4 weights = max(v_MaterialWeights, vec4(0.0001));
	float slope = 1.0 - clamp(normal.y, 0.0, 1.0);
	float curvature = v_TerrainAnalysis.x * 2.0 - 1.0;
	float flow = clamp(v_TerrainAnalysis.y, 0.0, 1.0);
	float moisture = clamp((1.0 - v_Height) * 0.45 + flow * 0.70
		+ max(-curvature, 0.0) * 0.25, 0.0, 1.0);
	weights.x *= 1.0 + moisture * u_MoistureInfluence * (1.0 - slope);
	weights.y *= 1.0 + moisture * u_MoistureInfluence
		+ max(-curvature, 0.0) * u_CurvatureInfluence;
	weights.z *= 1.0 + slope * 3.0 * u_SlopeInfluence
		+ max(curvature, 0.0) * u_CurvatureInfluence;
	weights.w *= 1.0 + smoothstep(0.55, 0.9, v_Height)
		* (1.0 - slope) * 3.0 * u_HeightInfluence;
	weights = pow(max(weights, vec4(0.0001)), vec4(max(u_WeightContrast, 0.25)));
	return weights / max(dot(weights, vec4(1.0)), 0.0001);
}

float DistributionGGX(vec3 n, vec3 h, float roughness)
{
	float a = roughness * roughness;
	float a2 = a * a;
	float nh = max(dot(n, h), 0.0);
	float d = nh * nh * (a2 - 1.0) + 1.0;
	return a2 / max(PI * d * d, 0.000001);
}

float GeometrySchlickGGX(float nd, float roughness)
{
	float r = roughness + 1.0;
	float k = r * r / 8.0;
	return nd / max(nd * (1.0 - k) + k, 0.000001);
}

vec3 EvaluateBRDF(vec3 n, vec3 v, vec3 l, vec3 radiance,
	vec3 albedo, float metallic, float roughness)
{
	vec3 h = normalize(v + l);
	vec3 f0 = mix(vec3(0.04), albedo, metallic);
	vec3 f = f0 + (1.0 - f0) * pow(clamp(1.0 - dot(h, v), 0.0, 1.0), 5.0);
	float g = GeometrySchlickGGX(max(dot(n, v), 0.0), roughness)
		* GeometrySchlickGGX(max(dot(n, l), 0.0), roughness);
	vec3 specular = DistributionGGX(n, h, roughness) * g * f
		/ max(4.0 * max(dot(n, v), 0.0) * max(dot(n, l), 0.0), 0.0001);
	vec3 diffuse = (1.0 - f) * (1.0 - metallic) * albedo / PI;
	return (diffuse + specular) * radiance * max(dot(n, l), 0.0);
}

float DirectionalShadowVisibility(
	vec3 worldPosition, vec3 normal, vec3 lightDirection)
{
	if (u_ShadowEnabled == 0)
		return 1.0;
	vec4 lightClip = u_LightViewProjection * vec4(worldPosition, 1.0);
	vec3 projected = lightClip.xyz / max(lightClip.w, 0.0001);
	projected = projected * 0.5 + 0.5;
	if (projected.z <= 0.0 || projected.z >= 1.0
		|| any(lessThan(projected.xy, vec2(0.0)))
		|| any(greaterThan(projected.xy, vec2(1.0))))
		return 1.0;
	float slopeBias = max(u_ShadowBias
		* (1.0 - max(dot(normal, lightDirection), 0.0)), u_ShadowBias * 0.25);
	float shadow = 0.0;
	for (int y = -1; y <= 1; ++y)
		for (int x = -1; x <= 1; ++x)
		{
			float closest = texture(u_ShadowMap,
				projected.xy + vec2(x, y) * u_ShadowTexelSize).r;
			shadow += projected.z - slopeBias > closest ? 1.0 : 0.0;
		}
	return 1.0 - shadow / 9.0;
}

void main()
{
	vec3 geometricNormal = normalize(v_Normal);
	vec3 projection = ProjectionWeights(geometricNormal);
	vec4 layerWeights = ResolveMaterialWeights(geometricNormal);
	vec3 albedo = vec3(0.0);
	vec3 detailNormal = vec3(0.0);
	float metallic = 0.0;
	float roughness = 0.0;
	float ao = 0.0;
	for (int index = 0; index < 4; ++index)
	{
		TerrainLayer layer = u_Layers[index];
		vec3 layerAlbedo = pow(max(layer.BaseColor, vec3(0.0)), vec3(2.2));
		if (layer.HasAlbedo != 0)
			layerAlbedo *= SampleTriplanar(u_AlbedoTextures[index],
				v_WorldPos, projection, layer.Tiling).rgb;
		vec3 layerNormal = geometricNormal;
		if (layer.HasNormal != 0)
			layerNormal = SampleTriplanarNormal(u_NormalTextures[index],
				v_WorldPos, geometricNormal, projection, layer.Tiling, layer.NormalScale);
		float layerAO = 1.0;
		if (layer.HasAO != 0)
			layerAO = mix(1.0, SampleTriplanar(u_AOTextures[index],
				v_WorldPos, projection, layer.Tiling).r, clamp(layer.AOStrength, 0.0, 1.0));
		float weight = layerWeights[index];
		albedo += layerAlbedo * weight;
		detailNormal += layerNormal * weight;
		metallic += clamp(layer.Metallic, 0.0, 1.0) * weight;
		roughness += clamp(layer.Roughness, 0.04, 1.0) * weight;
		ao += layerAO * weight;
	}
	vec3 normal = normalize(detailNormal);
	vec3 viewDirection = normalize(u_CameraPos - v_WorldPos);
	vec3 result = albedo * u_AmbientColorIntensity.rgb
		* u_AmbientColorIntensity.a * ao;
	if (u_DirectionalDirectionIntensity.w > 0.0)
	{
		vec3 direction = normalize(-u_DirectionalDirectionIntensity.xyz);
		vec3 radiance = u_DirectionalColor.rgb * u_DirectionalDirectionIntensity.w;
		float visibility = DirectionalShadowVisibility(
			v_WorldPos, normal, direction);
		result += EvaluateBRDF(normal, viewDirection, direction,
			radiance, albedo, metallic, roughness) * visibility;
	}
	uint count = min(u_LightCounts.x, 16u);
	for (uint index = 0u; index < count; ++index)
	{
		vec3 toLight = u_PointLights[index].PositionRange.xyz - v_WorldPos;
		float distanceToLight = length(toLight);
		float range = max(u_PointLights[index].PositionRange.w, 0.01);
		if (distanceToLight >= range) continue;
		float normalizedDistance = distanceToLight / range;
		float falloff = max(1.0 - normalizedDistance * normalizedDistance, 0.0);
		vec3 radiance = u_PointLights[index].ColorIntensity.rgb
			* u_PointLights[index].ColorIntensity.w * falloff * falloff
			/ max(distanceToLight * distanceToLight, 1.0);
		result += EvaluateBRDF(normal, viewDirection, toLight / max(distanceToLight, 0.0001),
			radiance, albedo, metallic, roughness);
	}
	o_Color = vec4(max(result, vec3(0.0)), 1.0);
	o_EntityID = v_EntityID;
}
