#ifndef CRAPGAME_RENDERER_GPU_STATICSHADOWSHADER_HPP
#define CRAPGAME_RENDERER_GPU_STATICSHADOWSHADER_HPP

#include <string>

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
    vec4 clip = uLightViewProjection * uModel * vec4(aPosition, 1.0);
    clip.z += 0.00045 * clip.w;
    gl_Position = clip;
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

/* Canonical receiver-side static-shadow comparison.  Caster depth is offset
 * in the shadow vertex shader, so this path intentionally avoids deriving bias
 * from the normal-mapped shading normal.  Doing that made brick and stone
 * normal-map detail modulate the shadow test itself and produced the speckled
 * acne pattern visible on Sponza. */
constexpr const char *STATIC_SHADOW_VISIBILITY_GLSL = R"GLSL(
float staticShadowVisibility(vec3 position)
{
    vec4 clip = uStaticShadowViewProjection * vec4(position, 1.0);
    if (abs(clip.w) <= EPSILON) return 1.0;
    vec3 ndc = clip.xyz / clip.w;
    vec2 uv = ndc.xy * 0.5 + 0.5;
    float receiverDepth = ndc.z * 0.5 + 0.5;
    if (uv.x <= 0.0 || uv.x >= 1.0 || uv.y <= 0.0 || uv.y >= 1.0
            || receiverDepth <= 0.0 || receiverDepth >= 1.0)
        return 1.0;

    ivec2 dimensions = textureSize(sStaticShadow, 0);
    vec2 texelPosition = uv * vec2(dimensions) - vec2(0.5);
    ivec2 basePixel = ivec2(floor(texelPosition));
    vec2 phase = fract(texelPosition);
    const float receiverBias = 0.00030;

    float visible = 0.0;
    float totalWeight = 0.0;
    for (int y = -1; y <= 2; ++y)
    {
        float wy = max(0.0, 2.0 - abs(float(y) - phase.y));
        for (int x = -1; x <= 2; ++x)
        {
            float wx = max(0.0, 2.0 - abs(float(x) - phase.x));
            float weight = wx * wy;
            if (weight <= 0.0) continue;
            ivec2 samplePixel = clamp(
                basePixel + ivec2(x, y),
                ivec2(0),
                dimensions - ivec2(1)
            );
            float blocker = texelFetch(sStaticShadow, samplePixel, 0).r;
            visible += receiverDepth - receiverBias <= blocker ? weight : 0.0;
            totalWeight += weight;
        }
    }
    return visible / max(totalWeight, 1.0);
}
)GLSL";

constexpr const char *STATIC_SHADOW_DIRECT_DECLARATIONS_GLSL = R"GLSL(
layout(binding=6) uniform sampler2D sStaticShadow;
uniform int uStaticShadowEnabled;
uniform int uStaticShadowLightIndex;
uniform mat4 uStaticShadowViewProjection;
)GLSL";

inline const std::string STATIC_SHADOW_DIRECT_GLSL =
    std::string(STATIC_SHADOW_DIRECT_DECLARATIONS_GLSL)
    + STATIC_SHADOW_VISIBILITY_GLSL;

} // namespace Gpu
} // namespace Renderer

#endif
