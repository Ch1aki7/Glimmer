#type vertex
#version 450 core

#include <Glimmer/PostProcessVertexABI.glslinc>

#type fragment
#version 450 core

#include <Glimmer/PostProcessABI.glslinc>

vec4 GlimmerPostProcess(GlimmerPostProcessInput inputData)
{
    const float pixelSize = 6.0;
    vec2 pixelGrid = max(inputData.Resolution / pixelSize, vec2(1.0));
    vec2 pixelUV = (floor(inputData.UV * pixelGrid) + 0.5) / pixelGrid;
    return GlimmerSampleScene(pixelUV);
}
