#type vertex
#version 450 core
layout(location=0) in vec3 a_Position;
layout(location=1) in vec2 a_TexCoord;
uniform sampler2D u_Height;
uniform sampler2D u_Water;
uniform mat4 u_Transform;
uniform mat4 u_ViewProjection;
uniform float u_HeightScale;
uniform vec2 u_UVOffset;
uniform vec2 u_UVScale;
uniform vec2 u_LocalOffset;
uniform float u_LocalScale;
out vec2 v_UV;
out vec3 v_WorldPosition;
float finiteValue(float v) { return isnan(v) || isinf(v) ? 0.0 : v; }
void main()
{
    v_UV = u_UVOffset + a_TexCoord * u_UVScale;
    float ground = finiteValue(textureLod(u_Height, v_UV, 0).r) * u_HeightScale;
    float depth = clamp(finiteValue(textureLod(u_Water, v_UV, 0).r), 0.0, 1000.0);
    vec3 local = vec3(a_Position.x * u_LocalScale + u_LocalOffset.x,
        ground + depth, a_Position.z * u_LocalScale + u_LocalOffset.y);
    v_WorldPosition = (u_Transform * vec4(local, 1.0)).xyz;
    gl_Position = u_ViewProjection * vec4(v_WorldPosition, 1.0);
}

#type fragment
#version 450 core
layout(location=0) out vec4 o_Color;
layout(location=1) out int o_EntityID;
layout(location=2) out vec4 o_Normal;
in vec2 v_UV;
in vec3 v_WorldPosition;
uniform sampler2D u_Height;
uniform sampler2D u_Water;
uniform sampler2D u_Velocity;
uniform sampler2D u_Sediment;
uniform sampler2D u_SceneColor;
uniform sampler2D u_SceneDepth;
uniform samplerCube u_SpecularPrefilterMap;
uniform int u_HasSpecularPrefilter;
uniform float u_SpecularPrefilterMaxLod;
uniform float u_SkyLightIntensity;
uniform mat4 u_Transform;
uniform mat4 u_ViewProjection;
uniform mat4 u_InverseViewProjection;
uniform vec3 u_CameraPosition;
uniform vec2 u_Resolution;
uniform float u_HeightScale;
uniform float u_WorldSize;
uniform float u_Time;
uniform float u_Absorption;
uniform float u_RefractionPixels;
uniform float u_FoamStrength;
uniform float u_SedimentTint;
uniform int u_EntityID;
struct PointLightData { vec4 PositionRange; vec4 ColorIntensity; };
layout(std140, binding=1) uniform LightEnvironment
{
    vec4 u_DirectionalDirectionIntensity;
    vec4 u_DirectionalColor;
    vec4 u_AmbientColorIntensity;
    uvec4 u_LightCounts;
    PointLightData u_PointLights[16];
};
float finiteValue(float v) { return isnan(v) || isinf(v) ? 0.0 : v; }
float water(vec2 uv) { return clamp(finiteValue(texture(u_Water, clamp(uv, vec2(0), vec2(1))).r), 0.0, 1000.0); }
float surface(vec2 uv)
{
    uv = clamp(uv, vec2(0), vec2(1));
    return finiteValue(texture(u_Height, uv).r) * u_HeightScale + water(uv);
}
vec3 worldAt(vec2 uv, float depth)
{
    vec4 p = u_InverseViewProjection * vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    return p.xyz / (abs(p.w) > 1e-6 ? p.w : 1e-6);
}
void main()
{
    float depth = water(v_UV);
    if (depth <= 0.002) discard;
    vec2 uv = gl_FragCoord.xy / u_Resolution;
    vec2 texel = 1.0 / vec2(textureSize(u_Water, 0));
    float spacing = max(u_WorldSize / max(float(textureSize(u_Water, 0).x - 1), 1.0), 0.0001);
    vec3 localNormal = normalize(vec3(
        (surface(v_UV - vec2(texel.x,0)) - surface(v_UV + vec2(texel.x,0))) / (2.0 * spacing),
        1.0, (surface(v_UV - vec2(0,texel.y)) - surface(v_UV + vec2(0,texel.y))) / (2.0 * spacing)));
    vec2 rawVelocity = texture(u_Velocity, v_UV).xy;
    // Hydrology velocity is diagnostic and can be huge in near-dry cells.
    vec2 velocity = clamp(vec2(finiteValue(rawVelocity.x), finiteValue(rawVelocity.y)), vec2(-20), vec2(20));
    velocity *= smoothstep(0.002, 0.04, depth);
    float speed = min(length(velocity), 20.0);
    vec2 phase = v_UV * u_WorldSize - velocity * u_Time * 0.15;
    localNormal.xz += 0.035 * vec2(sin(phase.x * 3.1 + phase.y), cos(phase.y * 2.7 - phase.x));
    vec3 normal = normalize(transpose(inverse(mat3(u_Transform))) * normalize(localNormal));
    vec3 viewDelta = u_CameraPosition - v_WorldPosition;
    vec3 viewDirection = viewDelta / max(length(viewDelta), 1e-5);
    if (dot(normal, viewDirection) < 0.0) normal = -normal;
    vec4 projectedNormal = u_ViewProjection * vec4(v_WorldPosition + normal, 1.0);
    vec2 projectedUV = projectedNormal.xy / max(abs(projectedNormal.w), 1e-5) * 0.5 + 0.5;
    vec2 distortion = clamp((projectedUV - uv) * u_Resolution, vec2(-1), vec2(1));
    float coverage = smoothstep(0.002, 0.02, depth);
    vec2 refractedUV = clamp(uv + distortion * u_RefractionPixels * coverage / u_Resolution,
        0.5 / u_Resolution, 1.0 - 0.5 / u_Resolution);
    // Foreground geometry must never be dragged across a water silhouette.
    float sceneDepth = texture(u_SceneDepth, refractedUV).r;
    if (sceneDepth <= gl_FragCoord.z + 1e-5)
    {
        refractedUV = uv;
        sceneDepth = texture(u_SceneDepth, uv).r;
    }
    vec3 background = texture(u_SceneColor, refractedUV).rgb;
    float thickness = min(depth * length(u_Transform[1].xyz), 100.0);
    if (sceneDepth < 0.999999)
        thickness = min(length(worldAt(refractedUV, sceneDepth) - v_WorldPosition), 100.0);
    float sediment = max(finiteValue(texture(u_Sediment, v_UV).r), 0.0);
    float concentration = clamp(sediment / max(depth, 0.02), 0.0, 20.0);
    float turbidity = 1.0 - exp(-concentration * u_SedimentTint);
    vec3 absorption = mix(vec3(0.8, 0.25, 0.12), vec3(1.2, 1.5, 2.0), turbidity) * u_Absorption;
    vec3 transmission = exp(-absorption * max(thickness, 0.0));
    vec3 scatter = mix(vec3(0.025, 0.15, 0.19), vec3(0.22, 0.12, 0.045), turbidity);
    vec3 color = background * transmission + scatter * (1.0 - transmission);
    float fresnel = 0.02 + 0.98 * pow(1.0 - clamp(dot(normal, viewDirection), 0.0, 1.0), 5.0);
    vec3 reflection = u_HasSpecularPrefilter != 0
        ? textureLod(u_SpecularPrefilterMap, reflect(-viewDirection, normal), 0.12 * u_SpecularPrefilterMaxLod).rgb * u_SkyLightIntensity
        : vec3(0.08, 0.12, 0.16);
    color = mix(color, reflection, fresnel);
    vec3 lightDirection = normalize(-u_DirectionalDirectionIntensity.xyz + vec3(0, 1e-5, 0));
    vec3 halfVector = normalize(lightDirection + viewDirection + vec3(0, 1e-5, 0));
    color += u_DirectionalColor.rgb * u_DirectionalDirectionIntensity.w
        * pow(max(dot(normal, halfVector), 0.0), 128.0) * 0.3;
    float foam = smoothstep(0.8, 6.0, speed) * (0.5 + 0.5 * sin(phase.x * 4.0) * sin(phase.y * 3.0));
    foam *= u_FoamStrength * smoothstep(0.002, 0.025, depth);
    color = mix(color, vec3(0.8), clamp(foam, 0.0, 1.0));
    // Explicit background composite: no fixed-function blend or feedback loop.
    color = mix(texture(u_SceneColor, uv).rgb, color, coverage);
    o_Color = vec4(clamp(color, vec3(0), vec3(65000)), 1.0);
    o_EntityID = u_EntityID;
    o_Normal = vec4(normal * 0.5 + 0.5, 1.0);
}
