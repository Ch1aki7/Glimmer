#type vertex
#version 450 core

#include <Glimmer/PostProcessVertexABI.glslinc>

#type fragment
#version 450 core

#include <Glimmer/PostProcessABI.glslinc>

vec4 GlimmerPostProcess(GlimmerPostProcessInput inputData)
{
    vec2 texel = inputData.TexelSize;
    float centerDepth = inputData.SceneDepth;
    float depthDifference = 0.0;
    depthDifference = max(depthDifference, abs(
        centerDepth - GlimmerSampleDepth(inputData.UV + vec2(texel.x, 0.0))));
    depthDifference = max(depthDifference, abs(
        centerDepth - GlimmerSampleDepth(inputData.UV - vec2(texel.x, 0.0))));
    depthDifference = max(depthDifference, abs(
        centerDepth - GlimmerSampleDepth(inputData.UV + vec2(0.0, texel.y))));
    depthDifference = max(depthDifference, abs(
        centerDepth - GlimmerSampleDepth(inputData.UV - vec2(0.0, texel.y))));

    float outline = smoothstep(0.0004, 0.0025, depthDifference);
    vec3 outlinedColor = mix(
        inputData.SceneColor.rgb, vec3(0.015), outline * 0.88);
    return vec4(outlinedColor, inputData.SceneColor.a);
}
