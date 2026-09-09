#type vertex
#version 450 core
#include <Glimmer/ModelVertexABI.glslinc>

void main()
{
    GlimmerWriteModelVertex(a_Position, a_Normal, a_Tangent, a_TexCoord);
}

#type fragment
#version 450 core
#include <Glimmer/Surface.glslinc>
#include <Glimmer/ShadowCSM.glslinc>

uniform float u_ToonShadowThreshold;
uniform float u_ToonLightThreshold;
uniform float u_ToonRimStrength;

float ToonBand(float normalDotLight)
{
    if (normalDotLight >= u_ToonLightThreshold) return 1.0;
    if (normalDotLight >= u_ToonShadowThreshold) return 0.62;
    return 0.28;
}

void main()
{
    GlimmerSurface surface = GlimmerResolveSurface();
    vec3 result = surface.Albedo * u_AmbientColorIntensity.rgb
        * u_AmbientColorIntensity.a * surface.AmbientOcclusion;

    if (u_HasDiffuseIrradiance != 0)
        result = surface.Albedo * texture(
            u_DiffuseIrradianceMap, surface.Normal).rgb
            * max(u_SkyLightIntensity, 0.0) * surface.AmbientOcclusion / PI;

    if (u_DirectionalDirectionIntensity.w > 0.0)
    {
        vec3 lightDirection = normalize(-u_DirectionalDirectionIntensity.xyz);
        float visibility = GlimmerDirectionalShadow(
            v_WorldPosition, surface.Normal, lightDirection);
        result += surface.Albedo * u_DirectionalColor.rgb
            * u_DirectionalDirectionIntensity.w
            * ToonBand(dot(surface.Normal, lightDirection)) * visibility;
    }

    uint pointLightCount = min(u_LightCounts.x, 16u);
    for (uint index = 0u; index < pointLightCount; ++index)
    {
        vec3 toLight = u_PointLights[index].PositionRange.xyz - v_WorldPosition;
        float distanceToLight = length(toLight);
        float range = max(u_PointLights[index].PositionRange.w, 0.01);
        if (distanceToLight >= range) continue;
        vec3 lightDirection = toLight / max(distanceToLight, 0.0001);
        float falloff = max(1.0 - distanceToLight / range, 0.0);
        result += surface.Albedo * u_PointLights[index].ColorIntensity.rgb
            * u_PointLights[index].ColorIntensity.w * falloff * falloff
            * ToonBand(dot(surface.Normal, lightDirection));
    }

    float rim = pow(1.0 - max(dot(
        surface.Normal, surface.ViewDirection), 0.0), 3.0);
    result += surface.Albedo * rim * max(u_ToonRimStrength, 0.0);
    if (u_ShadowCascadeDebug != 0 && u_ShadowEnabled != 0)
        result = mix(result,
            GlimmerResolveCascadeDebugColor(v_WorldPosition), 0.65);
    GlimmerWriteColor(result, surface.Alpha);
}
