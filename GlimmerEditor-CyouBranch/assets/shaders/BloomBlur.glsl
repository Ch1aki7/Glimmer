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
uniform vec2 u_TexelSize;
uniform int u_Horizontal;

void main()
{
    const float weights[5] = float[](0.227027, 0.1945946,
        0.1216216, 0.054054, 0.016216);
    vec2 direction = u_Horizontal != 0
        ? vec2(u_TexelSize.x, 0.0)
        : vec2(0.0, u_TexelSize.y);
    vec3 result = texture(u_SceneTexture, v_TexCoord).rgb * weights[0];
    for (int index = 1; index < 5; ++index)
    {
        vec2 offset = direction * float(index);
        result += texture(u_SceneTexture, v_TexCoord + offset).rgb * weights[index];
        result += texture(u_SceneTexture, v_TexCoord - offset).rgb * weights[index];
    }
    o_Color = vec4(result, 1.0);
}
