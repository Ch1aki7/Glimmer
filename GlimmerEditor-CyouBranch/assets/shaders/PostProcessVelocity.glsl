#type vertex
#version 450 core

#include <Glimmer/PostProcessVertexABI.glslinc>

#type fragment
#version 450 core

layout(location = 0) out vec2 o_Velocity;
layout(location = 0) in vec2 v_TexCoord;

uniform sampler2D u_SceneDepth;
uniform int u_HasCamera;
uniform int u_HasPreviousCamera;
uniform mat4 u_InverseViewProjection;
uniform mat4 u_PreviousViewProjection;

void main()
{
    float depth = texture(u_SceneDepth, v_TexCoord).r;
    if (u_HasCamera == 0 || u_HasPreviousCamera == 0 || depth >= 1.0)
    {
        o_Velocity = vec2(0.0);
        return;
    }

    vec4 clipPosition = vec4(v_TexCoord * 2.0 - 1.0,
        depth * 2.0 - 1.0, 1.0);
    vec4 worldPosition = u_InverseViewProjection * clipPosition;
    if (abs(worldPosition.w) <= 0.000001)
    {
        o_Velocity = vec2(0.0);
        return;
    }
    worldPosition /= worldPosition.w;

    vec4 previousClip = u_PreviousViewProjection * worldPosition;
    if (previousClip.w <= 0.000001)
    {
        o_Velocity = vec2(0.0);
        return;
    }
    vec2 previousUV = previousClip.xy / previousClip.w * 0.5 + 0.5;
    o_Velocity = v_TexCoord - previousUV;
}
