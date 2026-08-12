#type vertex
#version 450 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoord;

layout(location = 0) out vec2 v_TexCoord;

void main()
{
    v_TexCoord = a_TexCoord;
    gl_Position = vec4(a_Position, 1.0);
}

#type fragment
#version 450 core

layout(location = 0) out vec4 o_Color;
layout(location = 0) in vec2 v_TexCoord;

uniform sampler2D u_SceneTexture;
uniform sampler2D u_SceneDepth;
uniform float u_Exposure;
uniform int u_ApplyGrayscale;
uniform int u_DistanceFogEnabled;
uniform float u_DistanceFogDensity;
uniform float u_DistanceFogStart;
uniform float u_DistanceFogEnd;
uniform vec3 u_DistanceFogColor;
uniform int u_HeightFogEnabled;
uniform float u_HeightFogBaseHeight;
uniform float u_HeightFogFalloff;
uniform int u_FogColorSource;
uniform samplerCube u_FogSkyLight;
uniform float u_FogSkyLightIntensity;
uniform vec3 u_CameraPosition;
uniform mat4 u_InverseViewProjection;

vec3 ACESFilm(vec3 color)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((color * (a * color + b))
        / (color * (c * color + d) + e), 0.0, 1.0);
}

void main()
{
    vec4 sceneColor = texture(u_SceneTexture, v_TexCoord);
    vec3 linearColor = max(sceneColor.rgb, vec3(0.0));
    float sceneDepth = texture(u_SceneDepth, v_TexCoord).r;
    if (u_DistanceFogEnabled != 0 && sceneDepth < 0.999999)
    {
        vec4 clipPosition = vec4(
            v_TexCoord * 2.0 - 1.0,
            sceneDepth * 2.0 - 1.0,
            1.0);
        vec4 worldPosition = u_InverseViewProjection * clipPosition;
        worldPosition /= max(abs(worldPosition.w), 0.000001);
        float distanceToCamera = length(worldPosition.xyz - u_CameraPosition);
        float fogRange = max(u_DistanceFogEnd - u_DistanceFogStart, 0.0001);
        float rangeWeight = smoothstep(
            u_DistanceFogStart, u_DistanceFogEnd, distanceToCamera);
        float opticalDistance = max(distanceToCamera - u_DistanceFogStart, 0.0);
        float exponentialWeight = 1.0 - exp(
            -max(u_DistanceFogDensity, 0.0) * opticalDistance);
        float heightIntegral = 1.0;
        if (u_HeightFogEnabled != 0)
        {
            float falloff = max(u_HeightFogFalloff, 0.0);
            float cameraDensity = exp(clamp(-falloff
                * (u_CameraPosition.y - u_HeightFogBaseHeight), -20.0, 20.0));
            float heightDelta = worldPosition.y - u_CameraPosition.y;
            float denominator = falloff * heightDelta;
            heightIntegral = abs(denominator) > 0.0001
                ? cameraDensity * (1.0 - exp(clamp(-denominator, -20.0, 20.0)))
                    / denominator
                : cameraDensity;
            heightIntegral = clamp(heightIntegral, 0.0, 8.0);
        }
        float fogWeight = clamp(rangeWeight
            * (1.0 - pow(max(1.0 - exponentialWeight, 0.0), heightIntegral)),
            0.0, 1.0);
        vec3 fogColor = max(u_DistanceFogColor, vec3(0.0));
        if (u_FogColorSource == 1)
        {
            vec3 viewDirection = normalize(worldPosition.xyz - u_CameraPosition);
            fogColor = textureLod(u_FogSkyLight, viewDirection, 5.0).rgb
                * max(u_FogSkyLightIntensity, 0.0);
        }
        linearColor = mix(linearColor, fogColor, fogWeight);
    }
    vec3 hdrColor = linearColor * max(u_Exposure, 0.0);
    vec3 mappedColor = ACESFilm(hdrColor);

    if (u_ApplyGrayscale != 0)
    {
        float luminance = dot(mappedColor, vec3(0.2126, 0.7152, 0.0722));
        mappedColor = vec3(luminance);
    }

    vec3 displayColor = pow(mappedColor, vec3(1.0 / 2.2));
    o_Color = vec4(displayColor, sceneColor.a);
}
