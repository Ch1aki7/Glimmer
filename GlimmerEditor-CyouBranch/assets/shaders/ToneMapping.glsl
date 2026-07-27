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
uniform float u_Exposure;
uniform int u_ApplyGrayscale;

vec3 ACESFilm(vec3 color)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((color * (a * color + b))
        / (color * (c * color + d) + e), 0.0, 1.0);
}

void main()
{
    vec4 sceneColor = texture(u_SceneTexture, v_TexCoord);
    vec3 hdrColor = max(sceneColor.rgb, vec3(0.0)) * max(u_Exposure, 0.0);
    vec3 mappedColor = ACESFilm(hdrColor);

    if (u_ApplyGrayscale != 0)
    {
        float luminance = dot(mappedColor, vec3(0.2126, 0.7152, 0.0722));
        mappedColor = vec3(luminance);
    }

    vec3 displayColor = pow(mappedColor, vec3(1.0 / 2.2));
    o_Color = vec4(displayColor, sceneColor.a);
}
