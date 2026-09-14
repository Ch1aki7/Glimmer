#type vertex
#version 450 core

#include <Glimmer/PostProcessVertexABI.glslinc>

#type fragment
#version 450 core

#include <Glimmer/PostProcessABI.glslinc>

vec4 GlimmerPostProcess(GlimmerPostProcessInput inputData)
{
    if (!inputData.HasNormal || inputData.SceneNormalValidity <= 0.0)
        return inputData.SceneColor;

    vec3 center = inputData.SceneNormal;
    float edge = 0.0;
    const vec2 offsets[4] = vec2[4](
        vec2(1.0, 0.0), vec2(-1.0, 0.0),
        vec2(0.0, 1.0), vec2(0.0, -1.0));
    for (int index = 0; index < 4; ++index)
    {
        vec2 uv = inputData.UV + offsets[index] * inputData.TexelSize;
        float validity = GlimmerSampleNormalValidity(uv);
        if (validity <= 0.0)
        {
            edge = 1.0;
            continue;
        }
        edge = max(edge, 1.0 - max(dot(center, GlimmerSampleNormal(uv)), 0.0));
    }

    float outline = smoothstep(0.04, 0.22, edge);
    vec3 color = mix(inputData.SceneColor.rgb, vec3(0.01), outline * 0.9);
    return vec4(color, inputData.SceneColor.a);
}
