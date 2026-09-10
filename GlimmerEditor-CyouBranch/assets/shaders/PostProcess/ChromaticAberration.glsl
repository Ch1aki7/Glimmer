#type vertex
#version 450 core

#include <Glimmer/PostProcessVertexABI.glslinc>

#type fragment
#version 450 core

#include <Glimmer/PostProcessABI.glslinc>

vec4 GlimmerPostProcess(GlimmerPostProcessInput inputData)
{
    vec2 direction = inputData.UV - vec2(0.5);
    vec2 offset = direction * inputData.TexelSize * 8.0;
    vec2 redUV = clamp(inputData.UV + offset, vec2(0.0), vec2(1.0));
    vec2 blueUV = clamp(inputData.UV - offset, vec2(0.0), vec2(1.0));

    vec4 center = inputData.SceneColor;
    float red = GlimmerSampleScene(redUV).r;
    float blue = GlimmerSampleScene(blueUV).b;
    return vec4(red, center.g, blue, center.a);
}
