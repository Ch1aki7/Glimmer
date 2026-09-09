#type vertex
#version 450 core
#include <Glimmer/ModelVertexABI.glslinc>

uniform float u_OutlineWidth;

void main()
{
    mat4 transform = GlimmerModelTransform();
    vec4 worldPosition = transform * vec4(a_Position, 1.0);
    vec3 worldNormal = normalize(
        transpose(inverse(mat3(transform))) * a_Normal);
    worldPosition.xyz += worldNormal * max(u_OutlineWidth, 0.0);
    v_WorldPosition = worldPosition.xyz;
    v_WorldNormal = worldNormal;
    v_WorldTangent = normalize(mat3(transform) * a_Tangent);
    v_TexCoord = a_TexCoord;
    v_EntityID = GlimmerModelEntityID();
    gl_Position = u_ViewProjection * worldPosition;
}

#type fragment
#version 450 core
#include <Glimmer/ForwardFragmentABI.glslinc>

uniform vec4 u_OutlineColor;

void main()
{
    o_Color = u_OutlineColor;
    o_EntityID = v_EntityID;
}
