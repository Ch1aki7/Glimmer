#type vertex
#version 450 core
#include <Glimmer/ModelVertexABI.glslinc>

void main()
{
	GlimmerWriteModelVertex(a_Position, a_Normal, a_Tangent, a_TexCoord);
}

#type fragment
#version 450 core
#include <Glimmer/ForwardFragmentABI.glslinc>

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

vec3 FresnelSchlickRoughness(
    float cosine, vec3 reflectance, float roughness)
{
    return reflectance + (max(vec3(1.0 - roughness), reflectance) - reflectance)
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

#include <Glimmer/ShadowCSM.glslinc>

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
    if (u_HasMetallicTexture != 0)
        metallic = clamp(texture(
            u_MetallicTexture, v_TexCoord * u_TilingFactor).r, 0.0, 1.0);
    if (u_HasRoughnessTexture != 0)
        roughness = clamp(texture(
            u_RoughnessTexture, v_TexCoord * u_TilingFactor).r, 0.04, 1.0);
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
        result = diffuseWeight * albedo * irradiance / PI
            * ambientOcclusion;
    }
    else
    {
        result = albedo * u_AmbientColorIntensity.rgb
            * u_AmbientColorIntensity.a * ambientOcclusion;
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
            * max(u_SkyLightIntensity, 0.0) * ambientOcclusion;
    }

    if (u_DirectionalDirectionIntensity.w > 0.0)
    {
        vec3 lightDirection = normalize(-u_DirectionalDirectionIntensity.xyz);
        vec3 radiance = u_DirectionalColor.rgb
            * u_DirectionalDirectionIntensity.w;
        float visibility = GlimmerDirectionalShadow(
            v_WorldPosition, normal, lightDirection);
        result += EvaluateBRDF(normal, viewDirection, lightDirection,
            radiance, albedo, metallic, roughness) * visibility;
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
    if (u_ShadowCascadeDebug != 0 && u_ShadowEnabled != 0)
        result = mix(result, GlimmerResolveCascadeDebugColor(v_WorldPosition), 0.65);
    o_Color = vec4(max(result, vec3(0.0)), outputAlpha);
    o_EntityID = v_EntityID;
}
