#type vertex
#version 450 core

#include <Glimmer/PostProcessVertexABI.glslinc>

#type fragment
#version 450 core

#include <Glimmer/PostProcessABI.glslinc>

vec4 GlimmerPostProcess(GlimmerPostProcessInput inputData)
{
    if (!inputData.HistoryValid)
        return inputData.SceneColor;

    vec2 historyUV = inputData.UV - inputData.Velocity;
    bool inside = all(greaterThanEqual(historyUV, vec2(0.0)))
        && all(lessThanEqual(historyUV, vec2(1.0)));
    if (!inside)
        return inputData.SceneColor;

    vec4 history = GlimmerSampleHistory(historyUV);
    float motion = clamp(length(inputData.Velocity) * 24.0, 0.0, 1.0);
    float historyWeight = mix(0.3, 0.5, motion);
    return mix(inputData.SceneColor, history, historyWeight);
}
