#type vertex
#version 450 core

#include <Glimmer/PostProcessVertexABI.glslinc>

#type fragment
#version 450 core

#include <Glimmer/PostProcessABI.glslinc>

vec4 GlimmerPostProcess(GlimmerPostProcessInput inputData)
{
    float horizontalWave = sin(
        inputData.UV.y * 72.0 + inputData.Time * 3.0);
    float verticalWave = cos(
        inputData.UV.x * 46.0 + inputData.Time * 2.2);
    vec2 offset = vec2(horizontalWave, verticalWave)
        * inputData.TexelSize * 2.5;
    vec2 distortedUV = clamp(
        inputData.UV + offset, vec2(0.0), vec2(1.0));
    return GlimmerSampleScene(distortedUV);
}
