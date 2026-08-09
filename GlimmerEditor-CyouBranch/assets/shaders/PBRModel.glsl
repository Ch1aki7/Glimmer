#type vertex
#version 450 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec3 a_Tangent;
layout(location = 3) in vec2 a_TexCoord;
layout(location = 4) in mat4 a_InstanceTransform;
layout(location = 8) in ivec4 a_InstanceEntityData;

uniform mat4 u_ViewProjection;
uniform mat4 u_Transform;
uniform int u_EntityID;
uniform int u_UseInstancing;

layout(location = 0) out vec3 v_WorldPosition;
layout(location = 1) out vec3 v_WorldNormal;
layout(location = 2) out vec2 v_TexCoord;
layout(location = 3) flat out int v_EntityID;
layout(location = 4) out vec3 v_WorldTangent;

void main()
{
    mat4 transform = u_UseInstancing != 0
        ? a_InstanceTransform : u_Transform;
    vec4 worldPosition = transform * vec4(a_Position, 1.0);
    mat3 normalMatrix = transpose(inverse(mat3(transform)));

    v_WorldPosition = worldPosition.xyz;
    v_WorldNormal = normalize(normalMatrix * a_Normal);
    v_WorldTangent = normalize(mat3(transform) * a_Tangent);
    v_TexCoord = a_TexCoord;
    v_EntityID = u_UseInstancing != 0
        ? a_InstanceEntityData.x : u_EntityID;
    gl_Position = u_ViewProjection * worldPosition;
}

#type fragment
#version 450 core

layout(location = 0) out vec4 o_Color;
layout(location = 1) out int o_EntityID;

layout(location = 0) in vec3 v_WorldPosition;
layout(location = 1) in vec3 v_WorldNormal;
layout(location = 2) in vec2 v_TexCoord;
layout(location = 3) flat in int v_EntityID;
layout(location = 4) in vec3 v_WorldTangent;

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

uniform vec3 u_CameraPos;
uniform vec4 u_BaseColor;
uniform float u_Metallic;
uniform float u_Roughness;
uniform float u_NormalScale;
uniform float u_AOStrength;
uniform vec3 u_EmissiveColor;
uniform float u_EmissiveStrength;
uniform float u_TilingFactor;
uniform sampler2D u_BaseColorTexture;
uniform sampler2D u_NormalTexture;
uniform sampler2D u_AOTexture;
uniform sampler2D u_EmissiveTexture;
uniform int u_HasBaseColorTexture;
uniform int u_HasNormalTexture;
uniform int u_HasAOTexture;
uniform int u_HasEmissiveTexture;
uniform int u_AlphaMode;
uniform float u_AlphaCutoff;

const float PI = 3.14159265359;

float DistributionGGX(vec3 normal, vec3 halfway, float roughness)
{
    float alpha = roughness * roughness;
    float alphaSquared = alpha * alpha;
    float normalDotHalfway = max(dot(normal, halfway), 0.0);
    float denominator = normalDotHalfway * normalDotHalfway
        * (alphaSquared - 1.0) + 1.0;
    return alphaSquared / max(PI * denominator * denominator, 0.000001);
}

float GeometrySchlickGGX(float normalDotDirection, float roughness)
{
    float radius = roughness + 1.0;
    float k = radius * radius / 8.0;
    return normalDotDirection
        / max(normalDotDirection * (1.0 - k) + k, 0.000001);
}

float GeometrySmith(vec3 normal, vec3 viewDirection,
    vec3 lightDirection, float roughness)
{
    return GeometrySchlickGGX(max(dot(normal, viewDirection), 0.0), roughness)
        * GeometrySchlickGGX(max(dot(normal, lightDirection), 0.0), roughness);
}

vec3 FresnelSchlick(float cosine, vec3 reflectance)
{
    return reflectance + (1.0 - reflectance)
        * pow(clamp(1.0 - cosine, 0.0, 1.0), 5.0);
}

vec3 EvaluateBRDF(vec3 normal, vec3 viewDirection, vec3 lightDirection,
    vec3 radiance, vec3 albedo, float metallic, float roughness)
{
    vec3 halfway = normalize(viewDirection + lightDirection);
    vec3 reflectance = mix(vec3(0.04), albedo, metallic);
    vec3 fresnel = FresnelSchlick(max(dot(halfway, viewDirection), 0.0), reflectance);
    float distribution = DistributionGGX(normal, halfway, roughness);
    float geometry = GeometrySmith(normal, viewDirection, lightDirection, roughness);

    vec3 specular = distribution * geometry * fresnel
        / max(4.0 * max(dot(normal, viewDirection), 0.0)
            * max(dot(normal, lightDirection), 0.0), 0.0001);
    vec3 diffuseWeight = (vec3(1.0) - fresnel) * (1.0 - metallic);
    float normalDotLight = max(dot(normal, lightDirection), 0.0);
    return (diffuseWeight * albedo / PI + specular) * radiance * normalDotLight;
}

void main()
{
    vec4 sampledColor = vec4(1.0);
    if (u_HasBaseColorTexture != 0)
        sampledColor = texture(u_BaseColorTexture, v_TexCoord * u_TilingFactor);

    float effectiveAlpha = clamp(u_BaseColor.a * sampledColor.a, 0.0, 1.0);
    if (u_AlphaMode == 1 && effectiveAlpha < u_AlphaCutoff)
        discard;
    if (u_AlphaMode == 2 && effectiveAlpha <= (1.0 / 255.0))
        discard;

	vec3 linearBaseColor = pow(max(u_BaseColor.rgb, vec3(0.0)), vec3(2.2));
	vec3 albedo = linearBaseColor * sampledColor.rgb;
    float metallic = clamp(u_Metallic, 0.0, 1.0);
    float roughness = clamp(u_Roughness, 0.04, 1.0);
    vec3 normal = normalize(v_WorldNormal);
    if (u_HasNormalTexture != 0)
    {
        vec3 tangent = normalize(v_WorldTangent
            - normal * dot(v_WorldTangent, normal));
        vec3 bitangent = normalize(cross(normal, tangent));
        vec3 tangentNormal = texture(
            u_NormalTexture, v_TexCoord * u_TilingFactor).xyz * 2.0 - 1.0;
        tangentNormal.xy *= clamp(u_NormalScale, 0.0, 2.0);
        normal = normalize(mat3(tangent, bitangent, normal)
            * normalize(tangentNormal));
    }
    vec3 viewDirection = normalize(u_CameraPos - v_WorldPosition);

    float ambientOcclusion = 1.0;
    if (u_HasAOTexture != 0)
    {
        float sampledAO = texture(
            u_AOTexture, v_TexCoord * u_TilingFactor).r;
        ambientOcclusion = mix(1.0, sampledAO, clamp(u_AOStrength, 0.0, 1.0));
    }

    vec3 result = albedo * u_AmbientColorIntensity.rgb
        * u_AmbientColorIntensity.a * ambientOcclusion;

    if (u_DirectionalDirectionIntensity.w > 0.0)
    {
        vec3 lightDirection = normalize(-u_DirectionalDirectionIntensity.xyz);
        vec3 radiance = u_DirectionalColor.rgb
            * u_DirectionalDirectionIntensity.w;
        result += EvaluateBRDF(normal, viewDirection, lightDirection,
            radiance, albedo, metallic, roughness);
    }

    vec3 emissiveSample = vec3(1.0);
    if (u_HasEmissiveTexture != 0)
        emissiveSample = texture(
            u_EmissiveTexture, v_TexCoord * u_TilingFactor).rgb;
    vec3 linearEmissiveColor = pow(
        max(u_EmissiveColor, vec3(0.0)), vec3(2.2));
    result += linearEmissiveColor * emissiveSample
        * max(u_EmissiveStrength, 0.0);

    uint pointLightCount = min(u_LightCounts.x, 16u);
    for (uint index = 0u; index < pointLightCount; ++index)
    {
        vec3 toLight = u_PointLights[index].PositionRange.xyz - v_WorldPosition;
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
        result += EvaluateBRDF(normal, viewDirection, lightDirection,
            radiance, albedo, metallic, roughness);
    }

    float outputAlpha = u_AlphaMode == 0 ? 1.0 : effectiveAlpha;
    o_Color = vec4(max(result, vec3(0.0)), outputAlpha);
    o_EntityID = v_EntityID;
}
