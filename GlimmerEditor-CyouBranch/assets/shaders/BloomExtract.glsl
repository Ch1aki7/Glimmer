#type vertex
#version 450 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoord;
layout(location = 0) out vec2 v_TexCoord;

void main()
{
    v_TexCoord = a_TexCoord;
    gl_Position = vec4(a_Position, 1.0);
}

#type fragment
#version 450 core

layout(location = 0) out vec4 o_Color;
layout(location = 0) in vec2 v_TexCoord;

uniform sampler2D u_SceneTexture;
uniform float u_Threshold;
uniform float u_SoftKnee;
uniform float u_ExposureEV;

void main()
{
    vec3 color = max(texture(u_SceneTexture, v_TexCoord).rgb, vec3(0.0));
    float exposure = exp2(clamp(u_ExposureEV, -10.0, 10.0));
    float brightness = max(max(color.r, color.g), color.b) * exposure;
    float threshold = max(u_Threshold, 0.0);
    float knee = max(threshold * clamp(u_SoftKnee, 0.0, 1.0), 0.0001);
    float soft = clamp(brightness - threshold + knee, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee + 0.0001);
    float contribution = max(brightness - threshold, soft)
        / max(brightness, 0.0001);
    o_Color = vec4(color * clamp(contribution, 0.0, 1.0), 1.0);
}
