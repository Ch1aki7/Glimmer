#type vertex
#version 450 core

#include <Glimmer/PostProcessVertexABI.glslinc>

#type fragment
#version 450 core

#include <Glimmer/PostProcessABI.glslinc>

vec4 GlimmerPostProcess(GlimmerPostProcessInput inputData)
{
    float aspectRatio = inputData.Resolution.x / inputData.Resolution.y;
    vec2 centeredUV = inputData.UV - vec2(0.5);
    centeredUV.x *= aspectRatio;
    float distanceFromCenter = length(centeredUV);
    float vignette = 1.0 - smoothstep(0.35, 0.82, distanceFromCenter);
    vignette = mix(0.28, 1.0, vignette);
    return vec4(inputData.SceneColor.rgb * vignette, inputData.SceneColor.a);
}
