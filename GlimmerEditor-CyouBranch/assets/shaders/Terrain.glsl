#type vertex
#version 450 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoord;
layout(location = 2) in float a_Skirt;

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
uniform vec2 u_ChunkUVOffset;
uniform vec2 u_ChunkUVScale;
uniform vec2 u_ChunkLocalOffset;
uniform float u_ChunkLocalScale;
uniform float u_SkirtDepth;

out vec3 v_WorldPos;
out vec3 v_Normal;
out float v_Height;
out vec2 v_TerrainAnalysis;
out vec4 v_MaterialWeights;
out vec2 v_TerrainUV;
flat out int v_EntityID;

float SampleHeight(vec2 uv)
{
	return texture(u_HeightMap,
		clamp(uv, u_TexelSize * 0.5, vec2(1.0) - u_TexelSize * 0.5)).r;
}

void main()
{
	vec2 uv = u_ChunkUVOffset
		+ a_TexCoord * u_ChunkUVScale * u_UVScale;
	float height = SampleHeight(uv);
	float leftHeight = SampleHeight(uv - vec2(u_TexelSize.x, 0.0));
	float rightHeight = SampleHeight(uv + vec2(u_TexelSize.x, 0.0));
	float downHeight = SampleHeight(uv - vec2(0.0, u_TexelSize.y));
	float upHeight = SampleHeight(uv + vec2(0.0, u_TexelSize.y));
	vec3 localPosition = a_Position;
	localPosition.xz = a_Position.xz * u_ChunkLocalScale
		+ u_ChunkLocalOffset;
	localPosition.y = height * u_MaxHeight;
	localPosition.y -= a_Skirt * u_SkirtDepth;
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
	v_TerrainUV = uv;
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
in vec2 v_TerrainUV;
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
uniform int u_TerrainSamplingMode;
uniform float u_TerrainDetailDistance;
uniform int u_TerrainLODVisualization;
uniform int u_TerrainLODLevel;
uniform sampler2D u_WaterDepthMap;
uniform sampler2D u_WaterVelocityMap;
uniform int u_HasHydrology;
uniform int u_HydrologyVisualization;
uniform sampler2D u_ShadowMaps[4];
uniform mat4 u_LightViewProjections[4];
uniform float u_ShadowCascadeSplits[4];
uniform float u_ShadowCascadeBlendWidths[4];
uniform mat4 u_ShadowCameraView;
uniform int u_ShadowCascadeCount;
uniform int u_ShadowEnabled;
uniform int u_ShadowCascadeDebug;
uniform float u_ShadowBias;
uniform float u_ShadowTexelSize;
uniform samplerCube u_DiffuseIrradianceMap;
uniform int u_HasDiffuseIrradiance;
uniform samplerCube u_SpecularPrefilterMap;
uniform int u_HasSpecularPrefilter;
uniform float u_SpecularPrefilterMaxLod;
uniform sampler2D u_BrdfLut;
uniform int u_HasBrdfLut;
uniform float u_SkyLightIntensity;

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

vec3 FresnelSchlickRoughness(
	float cosine, vec3 reflectance, float roughness)
{
	return reflectance + (max(vec3(1.0 - roughness), reflectance) - reflectance)
		* pow(clamp(1.0 - cosine, 0.0, 1.0), 5.0);
}

float SampleCascadeDepth(int cascadeIndex, vec2 uv)
{
	if (cascadeIndex == 0) return texture(u_ShadowMaps[0], uv).r;
	if (cascadeIndex == 1) return texture(u_ShadowMaps[1], uv).r;
	if (cascadeIndex == 2) return texture(u_ShadowMaps[2], uv).r;
	return texture(u_ShadowMaps[3], uv).r;
}

float SampleCascadeVisibility(int cascadeIndex,
	vec3 worldPosition, vec3 normal, vec3 lightDirection)
{
	vec4 lightClip = u_LightViewProjections[cascadeIndex]
		* vec4(worldPosition, 1.0);
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
			float closest = SampleCascadeDepth(cascadeIndex,
				projected.xy + vec2(x, y) * u_ShadowTexelSize);
			shadow += projected.z - slopeBias > closest ? 1.0 : 0.0;
		}
	return 1.0 - shadow / 9.0;
}

float DirectionalShadowVisibility(
	vec3 worldPosition, vec3 normal, vec3 lightDirection)
{
	if (u_ShadowEnabled == 0)
		return 1.0;
	float viewDepth = abs((u_ShadowCameraView * vec4(worldPosition, 1.0)).z);
	int cascadeIndex = 0;
	while (cascadeIndex < u_ShadowCascadeCount - 1
		&& viewDepth > u_ShadowCascadeSplits[cascadeIndex])
		cascadeIndex++;

	for (int boundary = 0; boundary < 3; ++boundary)
	{
		if (boundary >= u_ShadowCascadeCount - 1)
			break;
		float width = u_ShadowCascadeBlendWidths[boundary];
		float split = u_ShadowCascadeSplits[boundary];
		if (width > 0.0 && viewDepth >= split - width
			&& viewDepth <= split + width)
		{
			float nearVisibility = SampleCascadeVisibility(
				boundary, worldPosition, normal, lightDirection);
			float farVisibility = SampleCascadeVisibility(
				boundary + 1, worldPosition, normal, lightDirection);
			float blend = smoothstep(split - width, split + width, viewDepth);
			return mix(nearVisibility, farVisibility, blend);
		}
	}

	return SampleCascadeVisibility(
		cascadeIndex, worldPosition, normal, lightDirection);
}

vec3 CascadeDebugColor(int cascadeIndex)
{
	if (cascadeIndex == 0) return vec3(1.0, 0.12, 0.08);
	if (cascadeIndex == 1) return vec3(0.12, 1.0, 0.18);
	if (cascadeIndex == 2) return vec3(0.12, 0.28, 1.0);
	return vec3(1.0, 0.78, 0.08);
}

vec3 ResolveCascadeDebugColor(vec3 worldPosition)
{
	float viewDepth = abs((u_ShadowCameraView * vec4(worldPosition, 1.0)).z);
	int cascadeIndex = 0;
	while (cascadeIndex < u_ShadowCascadeCount - 1
		&& viewDepth > u_ShadowCascadeSplits[cascadeIndex])
		cascadeIndex++;

	for (int boundary = 0; boundary < 3; ++boundary)
	{
		if (boundary >= u_ShadowCascadeCount - 1)
			break;
		float width = u_ShadowCascadeBlendWidths[boundary];
		float split = u_ShadowCascadeSplits[boundary];
		if (width > 0.0 && viewDepth >= split - width
			&& viewDepth <= split + width)
		{
			float blend = smoothstep(split - width, split + width, viewDepth);
			return mix(CascadeDebugColor(boundary),
				CascadeDebugColor(boundary + 1), blend);
		}
	}
	return CascadeDebugColor(cascadeIndex);
}

void main()
{
	vec3 geometricNormal = normalize(v_Normal);
	vec3 projection = ProjectionWeights(geometricNormal);
	vec4 layerWeights = ResolveMaterialWeights(geometricNormal);
	int primaryLayer = 0;
	int secondaryLayer = 1;
	for (int index = 1; index < 4; ++index)
	{
		if (layerWeights[index] > layerWeights[primaryLayer])
		{
			secondaryLayer = primaryLayer;
			primaryLayer = index;
		}
		else if (index != primaryLayer
			&& layerWeights[index] > layerWeights[secondaryLayer])
			secondaryLayer = index;
	}
	int samplingMode = u_TerrainSamplingMode;
	float secondaryDetailBlend = 1.0;
	if (samplingMode == 3)
	{
		float cameraDistance = distance(u_CameraPos, v_WorldPos);
		secondaryDetailBlend = 1.0 - smoothstep(
			u_TerrainDetailDistance * 0.85,
			u_TerrainDetailDistance * 1.15,
			cameraDistance);
		samplingMode = secondaryDetailBlend > 0.0 ? 1 : 2;
	}
	if (samplingMode != 0)
	{
		vec4 selectedWeights = vec4(0.0);
		selectedWeights[primaryLayer] = layerWeights[primaryLayer];
		selectedWeights[secondaryLayer] = layerWeights[secondaryLayer];
		layerWeights = selectedWeights
			/ max(dot(selectedWeights, vec4(1.0)), 0.0001);
	}
	vec3 albedo = vec3(0.0);
	vec3 detailNormal = vec3(0.0);
	float metallic = 0.0;
	float roughness = 0.0;
	float ao = 0.0;
	for (int index = 0; index < 4; ++index)
	{
		if (layerWeights[index] <= 0.0)
			continue;
		TerrainLayer layer = u_Layers[index];
		vec3 layerAlbedo = pow(max(layer.BaseColor, vec3(0.0)), vec3(2.2));
		if (layer.HasAlbedo != 0)
			layerAlbedo *= SampleTriplanar(u_AlbedoTextures[index],
				v_WorldPos, projection, layer.Tiling).rgb;
		vec3 layerNormal = geometricNormal;
		bool sampleDetail = samplingMode != 2 || index == primaryLayer;
		if (layer.HasNormal != 0 && sampleDetail)
		{
			layerNormal = SampleTriplanarNormal(u_NormalTextures[index],
				v_WorldPos, geometricNormal, projection, layer.Tiling, layer.NormalScale);
			if (u_TerrainSamplingMode == 3 && index == secondaryLayer)
				layerNormal = normalize(mix(
					geometricNormal, layerNormal, secondaryDetailBlend));
		}
		float layerAO = 1.0;
		if (layer.HasAO != 0 && sampleDetail)
		{
			layerAO = mix(1.0, SampleTriplanar(u_AOTextures[index],
				v_WorldPos, projection, layer.Tiling).r, clamp(layer.AOStrength, 0.0, 1.0));
			if (u_TerrainSamplingMode == 3 && index == secondaryLayer)
				layerAO = mix(1.0, layerAO, secondaryDetailBlend);
		}
		float weight = layerWeights[index];
		albedo += layerAlbedo * weight;
		detailNormal += layerNormal * weight;
		metallic += clamp(layer.Metallic, 0.0, 1.0) * weight;
		roughness += clamp(layer.Roughness, 0.04, 1.0) * weight;
		ao += layerAO * weight;
	}
	vec3 normal = normalize(detailNormal);
	vec3 viewDirection = normalize(u_CameraPos - v_WorldPos);
	vec3 reflectance = mix(vec3(0.04), albedo, metallic);
	vec3 environmentFresnel = FresnelSchlickRoughness(
		max(dot(normal, viewDirection), 0.0), reflectance, roughness);
	vec3 result;
	if (u_HasDiffuseIrradiance != 0)
	{
		vec3 diffuseWeight =
			(vec3(1.0) - environmentFresnel) * (1.0 - metallic);
		vec3 irradiance = texture(u_DiffuseIrradianceMap, normal).rgb
			* max(u_SkyLightIntensity, 0.0);
		result = diffuseWeight * albedo * irradiance / PI * ao;
	}
	else
	{
		result = albedo * u_AmbientColorIntensity.rgb
			* u_AmbientColorIntensity.a * ao;
	}
	if (u_HasSpecularPrefilter != 0 && u_HasBrdfLut != 0)
	{
		vec3 reflection = reflect(-viewDirection, normal);
		vec3 prefiltered = textureLod(
			u_SpecularPrefilterMap,
			reflection,
			roughness * max(u_SpecularPrefilterMaxLod, 0.0)).rgb;
		vec2 brdf = texture(
			u_BrdfLut,
			vec2(max(dot(normal, viewDirection), 0.0), roughness)).rg;
		result += prefiltered * (reflectance * brdf.x + brdf.y)
			* max(u_SkyLightIntensity, 0.0) * ao;
	}
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
	if (u_ShadowCascadeDebug != 0 && u_ShadowEnabled != 0)
		result = mix(result, ResolveCascadeDebugColor(v_WorldPos), 0.65);
	if (u_TerrainLODVisualization != 0)
	{
		vec3 lodColor = u_TerrainLODLevel == 0
			? vec3(1.0, 0.12, 0.08)
			: (u_TerrainLODLevel == 1
				? vec3(0.12, 1.0, 0.18)
				: vec3(0.10, 0.28, 1.0));
		result = mix(result, lodColor, 0.72);
	}
	if (u_HydrologyVisualization != 0 && u_HasHydrology != 0)
	{
		float waterDepth = max(texture(u_WaterDepthMap, v_TerrainUV).r, 0.0);
		float speed = length(texture(u_WaterVelocityMap, v_TerrainUV).xy);
		float waterWeight = clamp(waterDepth * 8.0, 0.0, 0.85);
		vec3 waterColor = mix(vec3(0.02, 0.18, 0.42),
			vec3(0.05, 0.75, 1.5), clamp(speed * 0.25, 0.0, 1.0));
		result = mix(result, waterColor, waterWeight);
	}
	o_Color = vec4(max(result, vec3(0.0)), 1.0);
	o_EntityID = v_EntityID;
}
