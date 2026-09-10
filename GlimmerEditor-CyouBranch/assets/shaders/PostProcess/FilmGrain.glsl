#type vertex
#version 450 core

#include <Glimmer/PostProcessVertexABI.glslinc>

#type fragment
#version 450 core

#include <Glimmer/PostProcessABI.glslinc>

float GlimmerFilmGrainNoise(vec2 pixelPosition, float frame)
{
    vec3 value = fract(vec3(pixelPosition.xyx) * 0.1031);
    value += dot(value, value.yzx + 33.33 + frame);
    return fract((value.x + value.y) * value.z);
}

vec4 GlimmerPostProcess(GlimmerPostProcessInput inputData)
{
    vec2 pixelPosition = floor(inputData.UV * inputData.Resolution);
    float frame = floor(inputData.Time * 24.0);
    float noise = GlimmerFilmGrainNoise(pixelPosition, frame) - 0.5;
    float luminance = dot(
        inputData.SceneColor.rgb, vec3(0.2126, 0.7152, 0.0722));
    float grainStrength = mix(0.038, 0.016, clamp(luminance, 0.0, 1.0));
    vec3 color = max(
        inputData.SceneColor.rgb + vec3(noise * grainStrength), vec3(0.0));
    return vec4(color, inputData.SceneColor.a);
}
