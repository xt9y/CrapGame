#ifndef CRAPGAME_RENDERER_GPU_STATICSHADOWSHADER_HPP
#define CRAPGAME_RENDERER_GPU_STATICSHADOWSHADER_HPP

namespace Renderer
{
namespace Gpu
{

constexpr const char *STATIC_SHADOW_VERTEX_SHADER = R"GLSL(
#version 430 core
layout(location=0) in vec3 aPosition;
layout(location=1) in vec2 aUv;
uniform mat4 uModel;
uniform mat4 uLightViewProjection;
out vec2 vUv;
void main()
{
    vUv = aUv;
    gl_Position = uLightViewProjection * uModel * vec4(aPosition, 1.0);
}
)GLSL";

constexpr const char *STATIC_SHADOW_FRAGMENT_SHADER = R"GLSL(
#version 430 core
in vec2 vUv;
uniform int uMasked;
uniform int uHasOpacityTexture;
uniform float uOpacity;
uniform float uAlphaCutoff;
uniform vec4 uOpacityScaleOffset;
uniform int uOpacityChannel;
uniform sampler2D uOpacityTexture;

float opacityChannel(vec4 value, int channel)
{
    if (channel == 1) return value.g;
    if (channel == 2) return value.b;
    if (channel == 3) return value.a;
    if (channel == 4) return dot(value.rgb, vec3(1.0 / 3.0));
    return value.r;
}

void main()
{
    if (uMasked == 0) return;
    float opacity = clamp(uOpacity, 0.0, 1.0);
    if (uHasOpacityTexture != 0)
    {
        vec2 uv = vUv * uOpacityScaleOffset.xy + uOpacityScaleOffset.zw;
        opacity *= opacityChannel(texture(uOpacityTexture, uv), uOpacityChannel);
    }
    if (opacity < uAlphaCutoff) discard;
}
)GLSL";

constexpr const char *STATIC_SHADOW_DIRECT_GLSL = R"GLSL(
layout(binding=6) uniform sampler2D sStaticShadow;
uniform int uStaticShadowEnabled;
uniform int uStaticShadowLightIndex;
uniform mat4 uStaticShadowViewProjection;

float staticShadowVisibility(vec3 position, vec3 normal, vec3 lightDirection)
{
    vec4 clip = uStaticShadowViewProjection * vec4(position, 1.0);
    if (abs(clip.w) <= EPSILON) return 1.0;
    vec3 ndc = clip.xyz / clip.w;
    vec2 uv = ndc.xy * 0.5 + 0.5;
    float receiverDepth = ndc.z * 0.5 + 0.5;
    if (uv.x <= 0.0 || uv.x >= 1.0 || uv.y <= 0.0 || uv.y >= 1.0
            || receiverDepth <= 0.0 || receiverDepth >= 1.0)
        return 1.0;

    vec2 texel = 1.0 / vec2(textureSize(sStaticShadow, 0));
    float slope = 1.0 - max(dot(normalize(normal), normalize(lightDirection)), 0.0);
    float bias = 0.00035 + slope * 0.00125;
    float visible = 0.0;
    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            float blocker = texture(sStaticShadow, uv + vec2(x, y) * texel).r;
            visible += receiverDepth - bias <= blocker ? 1.0 : 0.0;
        }
    }
    return visible / 9.0;
}
)GLSL";

} // namespace Gpu
} // namespace Renderer

#endif
