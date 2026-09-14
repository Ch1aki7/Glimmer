#type vertex
#version 450 core

#include <Glimmer/PostProcessVertexABI.glslinc>

#type fragment
#version 450 core

#include <Glimmer/PostProcessABI.glslinc>

vec4 GlimmerPostProcess(GlimmerPostProcessInput inputData)
{
    if (!inputData.HasVelocity)
        return inputData.SceneColor;

    vec2 velocity = inputData.Velocity;
    float strength = smoothstep(0.0005, 0.035, length(velocity));
    vec4 accumulated = vec4(0.0);
    const int sampleCount = 8;
    for (int index = 0; index < sampleCount; ++index)
    {
        float alongMotion = float(index) / float(sampleCount - 1);
        vec2 sampleUV = clamp(inputData.UV - velocity * alongMotion,
            vec2(0.0), vec2(1.0));
        accumulated += GlimmerSampleScene(sampleUV);
    }
    accumulated /= float(sampleCount);
    return mix(inputData.SceneColor, accumulated, strength * 0.8);
}
