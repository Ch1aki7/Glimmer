#type vertex
#version 450 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec3 a_Tangent;
layout(location = 3) in vec2 a_TexCoord;

uniform mat4 u_ViewProjection;
uniform mat4 u_Transform;

layout(location = 0) out vec3 v_WorldPosition;
layout(location = 1) out vec3 v_WorldNormal;
layout(location = 2) out vec2 v_TexCoord;

void main()
{
    vec4 worldPosition = u_Transform * vec4(a_Position, 1.0);
    mat3 normalMatrix = transpose(inverse(mat3(u_Transform)));

    v_WorldPosition = worldPosition.xyz;
    v_WorldNormal = normalize(normalMatrix * a_Normal);
    v_TexCoord = a_TexCoord;
    gl_Position = u_ViewProjection * worldPosition;
}

#type fragment
#version 450 core

layout(location = 0) out vec4 o_Color;
layout(location = 1) out int o_EntityID;

layout(location = 0) in vec3 v_WorldPosition;
layout(location = 1) in vec3 v_WorldNormal;
layout(location = 2) in vec2 v_TexCoord;

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
uniform float u_TilingFactor;
uniform sampler2D u_BaseColorTexture;
uniform int u_HasBaseColorTexture;
uniform int u_EntityID;

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

    vec3 albedo = pow(max(u_BaseColor.rgb * sampledColor.rgb, vec3(0.0)), vec3(2.2));
    float metallic = clamp(u_Metallic, 0.0, 1.0);
    float roughness = clamp(u_Roughness, 0.04, 1.0);
    vec3 normal = normalize(v_WorldNormal);
    vec3 viewDirection = normalize(u_CameraPos - v_WorldPosition);

    vec3 result = albedo * u_AmbientColorIntensity.rgb
        * u_AmbientColorIntensity.a;

    if (u_DirectionalDirectionIntensity.w > 0.0)
    {
        vec3 lightDirection = normalize(-u_DirectionalDirectionIntensity.xyz);
        vec3 radiance = u_DirectionalColor.rgb
            * u_DirectionalDirectionIntensity.w;
        result += EvaluateBRDF(normal, viewDirection, lightDirection,
            radiance, albedo, metallic, roughness);
    }

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

    o_Color = vec4(max(result, vec3(0.0)), u_BaseColor.a * sampledColor.a);
    o_EntityID = u_EntityID;
}
