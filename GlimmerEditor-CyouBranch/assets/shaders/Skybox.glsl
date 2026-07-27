#type vertex
#version 450 core

layout(location = 0) in vec3 a_Position;
layout(location = 0) out vec3 v_Direction;

uniform mat4 u_View;
uniform mat4 u_Projection;

void main()
{
    v_Direction = a_Position;
    mat4 rotationOnlyView = mat4(mat3(u_View));
    vec4 clipPosition = u_Projection * rotationOnlyView * vec4(a_Position, 1.0);
    gl_Position = clipPosition.xyww;
}

#type fragment
#version 450 core

layout(location = 0) out vec4 o_Color;
layout(location = 0) in vec3 v_Direction;

uniform samplerCube u_Skybox;
uniform float u_Intensity;

void main()
{
    vec3 environment = texture(u_Skybox, normalize(v_Direction)).rgb;
    o_Color = vec4(environment * max(u_Intensity, 0.0), 1.0);
}
