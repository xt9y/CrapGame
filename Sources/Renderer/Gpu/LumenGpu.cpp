#include "LumenGpu.hpp"

#include "Renderer/Gpu/Gpu.hpp"
#include "Renderer/Gpu/ResourceLifecycle.hpp"
#include "Renderer/Gpu/SurfaceFormats.hpp"

#include <algorithm>
#include <cmath>

namespace Renderer
{
namespace Gpu
{
namespace
{

constexpr const char *LUMEN_TRACE_COMPUTE = R"GLSL(
#version 430 core

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(binding = 0) uniform sampler2D sPositionDepth;
layout(binding = 1) uniform sampler2D sNormalRoughness;
layout(binding = 2) uniform sampler2D sAlbedoMetallic;
layout(binding = 3) uniform sampler2D sDirect;
layout(binding = 4) uniform sampler2D sPreviousIndirect;
layout(binding = 5) uniform sampler2D sPreviousReflection;
layout(binding = 6) uniform sampler2D sPreviousPosition;

layout(rgba16f, binding = 0) writeonly uniform image2D oIndirect;
layout(rgba16f, binding = 1) writeonly uniform image2D oReflection;
layout(rgba16f, binding = 2) writeonly uniform image2D oPositionHistory;

struct PrimitiveData
{
    vec4 positionType;
    vec4 rotation;
    vec4 scale;
    vec4 albedoMetallic;
    vec4 emissiveRoughness;
};

layout(std430, binding = 7) readonly buffer PrimitiveBuffer
{
    PrimitiveData primitives[];
};

uniform mat4 uViewProjection;
uniform vec3 uCameraPosition;
uniform int uPrimitiveCount;
uniform int uFrameIndex;
uniform int uHistoryValid;

const float EPSILON = 0.00001;
const float TRACE_BIAS = 0.012;

vec3 rotateX (vec3 v, float angle)
{
    float s = sin(angle);
    float c = cos(angle);
    return vec3(v.x, c * v.y - s * v.z, s * v.y + c * v.z);
}

vec3 rotateY (vec3 v, float angle)
{
    float s = sin(angle);
    float c = cos(angle);
    return vec3(c * v.x + s * v.z, v.y, -s * v.x + c * v.z);
}

vec3 rotateZ (vec3 v, float angle)
{
    float s = sin(angle);
    float c = cos(angle);
    return vec3(c * v.x - s * v.y, s * v.x + c * v.y, v.z);
}

vec3 inverseRotate (vec3 v, vec3 degreesValue)
{
    vec3 r = radians(degreesValue);
    v = rotateY(v, -r.y);
    v = rotateX(v, -r.x);
    v = rotateZ(v, -r.z);
    return v;
}

vec3 forwardRotate (vec3 v, vec3 degreesValue)
{
    vec3 r = radians(degreesValue);
    v = rotateZ(v, r.z);
    v = rotateX(v, r.x);
    v = rotateY(v, r.y);
    return v;
}

vec3 safeScale (vec3 value)
{
    return vec3(
        abs(value.x) > EPSILON ? value.x : 0.00001,
        abs(value.y) > EPSILON ? value.y : 0.00001,
        abs(value.z) > EPSILON ? value.z : 0.00001
    );
}

bool intersectBox (
        vec3 rayOrigin,
        vec3 rayDirection,
        PrimitiveData primitive,
        float maximumDistance,
        out float hitDistance,
        out vec3 hitNormal)
{
    vec3 scaleValue = safeScale(primitive.scale.xyz);
    vec3 localOrigin = inverseRotate(
        rayOrigin - primitive.positionType.xyz,
        primitive.rotation.xyz
    ) / scaleValue;
    vec3 localDirection = inverseRotate(
        rayDirection,
        primitive.rotation.xyz
    ) / scaleValue;

    vec3 invDirection = vec3(
        abs(localDirection.x) > EPSILON ? 1.0 / localDirection.x : 1e20,
        abs(localDirection.y) > EPSILON ? 1.0 / localDirection.y : 1e20,
        abs(localDirection.z) > EPSILON ? 1.0 / localDirection.z : 1e20
    );

    vec3 t0 = (vec3(-0.75) - localOrigin) * invDirection;
    vec3 t1 = (vec3( 0.75) - localOrigin) * invDirection;
    vec3 tMin3 = min(t0, t1);
    vec3 tMax3 = max(t0, t1);

    float tNear = max(max(tMin3.x, tMin3.y), tMin3.z);
    float tFar = min(min(tMax3.x, tMax3.y), tMax3.z);

    if (tFar < max(tNear, TRACE_BIAS))
    {
        return false;
    }

    hitDistance = tNear > TRACE_BIAS ? tNear : tFar;

    if (hitDistance <= TRACE_BIAS || hitDistance >= maximumDistance)
    {
        return false;
    }

    vec3 localHit = localOrigin + localDirection * hitDistance;
    vec3 magnitude = abs(localHit);
    vec3 localNormal;

    if (magnitude.x >= magnitude.y && magnitude.x >= magnitude.z)
    {
        localNormal = vec3(sign(localHit.x), 0.0, 0.0);
    }
    else if (magnitude.y >= magnitude.z)
    {
        localNormal = vec3(0.0, sign(localHit.y), 0.0);
    }
    else
    {
        localNormal = vec3(0.0, 0.0, sign(localHit.z));
    }

    hitNormal = normalize(
        forwardRotate(localNormal / scaleValue, primitive.rotation.xyz)
    );
    return true;
}

bool intersectPlane (
        vec3 rayOrigin,
        vec3 rayDirection,
        PrimitiveData primitive,
        float maximumDistance,
        out float hitDistance,
        out vec3 hitNormal)
{
    vec3 scaleValue = safeScale(primitive.scale.xyz);
    vec3 localOrigin = inverseRotate(
        rayOrigin - primitive.positionType.xyz,
        primitive.rotation.xyz
    ) / scaleValue;
    vec3 localDirection = inverseRotate(
        rayDirection,
        primitive.rotation.xyz
    ) / scaleValue;

    if (abs(localDirection.y) <= EPSILON)
    {
        return false;
    }

    hitDistance = -localOrigin.y / localDirection.y;

    if (hitDistance <= TRACE_BIAS || hitDistance >= maximumDistance)
    {
        return false;
    }

    vec3 localHit = localOrigin + localDirection * hitDistance;

    if (abs(localHit.x) > 0.5 || abs(localHit.z) > 0.5)
    {
        return false;
    }

    vec3 localNormal = localDirection.y < 0.0
        ? vec3(0.0, 1.0, 0.0)
        : vec3(0.0, -1.0, 0.0);

    hitNormal = normalize(
        forwardRotate(localNormal / scaleValue, primitive.rotation.xyz)
    );
    return true;
}

bool traceScene (
        vec3 rayOrigin,
        vec3 rayDirection,
        float maximumDistance,
        out int hitIndex,
        out float hitDistance,
        out vec3 hitNormal)
{
    hitIndex = -1;
    hitDistance = maximumDistance;
    hitNormal = vec3(0.0);

    for (int index = 0; index < uPrimitiveCount; ++index)
    {
        PrimitiveData primitive = primitives[index];
        float candidateDistance = maximumDistance;
        vec3 candidateNormal = vec3(0.0);
        int type = int(primitive.positionType.w + 0.5);

        bool hit = type == 0
            ? intersectBox(
                rayOrigin,
                rayDirection,
                primitive,
                hitDistance,
                candidateDistance,
                candidateNormal
            )
            : intersectPlane(
                rayOrigin,
                rayDirection,
                primitive,
                hitDistance,
                candidateDistance,
                candidateNormal
            );

        if (hit && candidateDistance < hitDistance)
        {
            hitIndex = index;
            hitDistance = candidateDistance;
            hitNormal = candidateNormal;
        }
    }

    return hitIndex >= 0;
}

uint hashValue (uvec2 value)
{
    uint h = value.x * 0x8da6b343u + value.y * 0xd8163841u;
    h ^= h >> 13;
    h *= 0xcb1ab31fu;
    h ^= h >> 16;
    return h;
}

float randomValue (inout uint state)
{
    state = state * 1664525u + 1013904223u;
    return float(state & 0x00ffffffu) / float(0x01000000u);
}

vec3 hemisphereDirection (vec3 normal, ivec2 pixel)
{
    uint state = hashValue(uvec2(pixel)) ^ uint(uFrameIndex * 747796405);
    float r1 = randomValue(state);
    float r2 = randomValue(state);

    float phi = 6.28318530718 * r1;
    float radius = sqrt(r2);
    float z = sqrt(max(0.0, 1.0 - r2));
    vec3 local = vec3(cos(phi) * radius, sin(phi) * radius, z);

    vec3 n = normalize(normal);
    vec3 helper = abs(n.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(helper, n));
    vec3 bitangent = cross(n, tangent);
    return normalize(tangent * local.x + bitangent * local.y + n * local.z);
}

vec3 primitiveFallbackRadiance (int primitiveIndex)
{
    if (primitiveIndex < 0)
    {
        return vec3(0.018, 0.022, 0.032);
    }

    PrimitiveData primitive = primitives[primitiveIndex];
    return primitive.emissiveRoughness.xyz
         + primitive.albedoMetallic.xyz * 0.045;
}

vec3 screenRadiance (vec3 position, int primitiveIndex)
{
    vec4 clip = uViewProjection * vec4(position, 1.0);

    if (clip.w <= EPSILON)
    {
        return primitiveFallbackRadiance(primitiveIndex);
    }

    vec2 uv = clip.xy / clip.w * 0.5 + 0.5;

    if (uv.x <= 0.0 || uv.x >= 1.0 || uv.y <= 0.0 || uv.y >= 1.0)
    {
        return primitiveFallbackRadiance(primitiveIndex);
    }

    ivec2 dimensions = textureSize(sPositionDepth, 0);
    ivec2 samplePixel = clamp(
        ivec2(uv * vec2(dimensions)),
        ivec2(0),
        dimensions - ivec2(1)
    );

    vec4 samplePosition = texelFetch(sPositionDepth, samplePixel, 0);

    if (samplePosition.w > 0.0
            && distance(samplePosition.xyz, position) < 0.35)
    {
        return texelFetch(sDirect, samplePixel, 0).xyz;
    }

    return primitiveFallbackRadiance(primitiveIndex);
}

void main ()
{
    ivec2 tracePixel = ivec2(gl_GlobalInvocationID.xy);
    ivec2 traceDimensions = imageSize(oIndirect);

    if (tracePixel.x >= traceDimensions.x || tracePixel.y >= traceDimensions.y)
    {
        return;
    }

    ivec2 fullDimensions = textureSize(sPositionDepth, 0);
    ivec2 pixel = min(tracePixel * 2 + ivec2(1), fullDimensions - ivec2(1));

    vec4 positionDepth = texelFetch(sPositionDepth, pixel, 0);

    if (positionDepth.w <= 0.0)
    {
        imageStore(oIndirect, tracePixel, vec4(0.0));
        imageStore(oReflection, tracePixel, vec4(0.0));
        imageStore(oPositionHistory, tracePixel, vec4(0.0));
        return;
    }

    vec4 normalRoughness = texelFetch(sNormalRoughness, pixel, 0);
    vec4 albedoMetallic = texelFetch(sAlbedoMetallic, pixel, 0);

    vec3 position = positionDepth.xyz;
    vec3 normal = normalize(normalRoughness.xyz);
    float roughness = clamp(normalRoughness.w, 0.04, 1.0);
    vec3 albedo = albedoMetallic.xyz;
    float metallic = clamp(albedoMetallic.w, 0.0, 1.0);

    vec3 giDirection = hemisphereDirection(normal, pixel);
    int giHitIndex;
    float giHitDistance;
    vec3 giHitNormal;
    vec3 indirect = vec3(0.0);

    if (traceScene(
            position + normal * TRACE_BIAS * 2.0,
            giDirection,
            28.0,
            giHitIndex,
            giHitDistance,
            giHitNormal))
    {
        vec3 hitPosition = position + normal * TRACE_BIAS * 2.0
                         + giDirection * giHitDistance;
        vec3 source = screenRadiance(hitPosition, giHitIndex);
        float diffuseWeight = 1.0 - metallic;
        indirect = source * albedo * diffuseWeight * 0.32;
    }
    else
    {
        float sky = clamp(normal.y * 0.5 + 0.5, 0.0, 1.0);
        indirect = albedo * (1.0 - metallic) * mix(0.008, 0.028, sky);
    }

    vec3 incident = normalize(position - uCameraPosition);
    vec3 reflectionDirection = normalize(reflect(incident, normal));
    vec3 reflection = vec3(0.0);

    if (metallic > 0.08 || roughness < 0.45)
    {
        int reflectionHitIndex;
        float reflectionHitDistance;
        vec3 reflectionHitNormal;

        if (traceScene(
                position + normal * TRACE_BIAS * 2.0,
                reflectionDirection,
                48.0,
                reflectionHitIndex,
                reflectionHitDistance,
                reflectionHitNormal))
        {
            vec3 hitPosition = position + normal * TRACE_BIAS * 2.0
                             + reflectionDirection * reflectionHitDistance;
            vec3 source = screenRadiance(hitPosition, reflectionHitIndex);
            vec3 f0 = mix(vec3(0.04), albedo, metallic);
            float fresnel = pow(
                1.0 - clamp(dot(normal, normalize(uCameraPosition - position)), 0.0, 1.0),
                5.0
            );
            vec3 weight = f0 + (vec3(1.0) - f0) * fresnel;
            reflection = source * weight * (1.0 - roughness * 0.82);
        }
    }

    if (uHistoryValid != 0)
    {
        vec4 previousPosition = texelFetch(sPreviousPosition, tracePixel, 0);

        if (previousPosition.w > 0.0
                && distance(previousPosition.xyz, position) < 0.18)
        {
            vec3 previousIndirect = texelFetch(sPreviousIndirect, tracePixel, 0).xyz;
            vec3 previousReflection = texelFetch(sPreviousReflection, tracePixel, 0).xyz;

            indirect = mix(indirect, previousIndirect, 0.84);
            reflection = mix(reflection, previousReflection, 0.72);
        }
    }

    imageStore(oIndirect, tracePixel, vec4(max(indirect, vec3(0.0)), 1.0));
    imageStore(oReflection, tracePixel, vec4(max(reflection, vec3(0.0)), 1.0));
    imageStore(oPositionHistory, tracePixel, vec4(position, 1.0));
}
)GLSL";

constexpr const char *LUMEN_COMPOSITE_COMPUTE = R"GLSL(
#version 430 core

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(binding = 0) uniform sampler2D sPositionDepth;
layout(binding = 1) uniform sampler2D sNormalRoughness;
layout(binding = 2) uniform sampler2D sDirect;
layout(binding = 3) uniform sampler2D sIndirect;
layout(binding = 4) uniform sampler2D sReflection;

layout(rgba8, binding = 0) writeonly uniform image2D oFinal;

vec3 toneMap (vec3 colorValue)
{
    vec3 positive = max(colorValue, vec3(0.0));
    vec3 mapped = positive / (vec3(1.0) + positive);
    return pow(clamp(mapped, vec3(0.0), vec3(1.0)), vec3(1.0 / 2.2));
}

float shortRangeAo (ivec2 pixel, vec3 position, vec3 normal)
{
    ivec2 dimensions = textureSize(sPositionDepth, 0);
    const ivec2 offsets[8] = ivec2[8](
        ivec2( 2,  0),
        ivec2(-2,  0),
        ivec2( 0,  2),
        ivec2( 0, -2),
        ivec2( 2,  2),
        ivec2(-2,  2),
        ivec2( 2, -2),
        ivec2(-2, -2)
    );

    float occlusion = 0.0;

    for (int index = 0; index < 8; ++index)
    {
        ivec2 samplePixel = clamp(pixel + offsets[index], ivec2(0), dimensions - ivec2(1));
        vec4 samplePosition = texelFetch(sPositionDepth, samplePixel, 0);

        if (samplePosition.w <= 0.0)
        {
            continue;
        }

        vec3 delta = samplePosition.xyz - position;
        float distanceValue = length(delta);

        if (distanceValue <= 0.01 || distanceValue >= 0.85)
        {
            continue;
        }

        float facing = max(dot(normalize(delta), normal) - 0.08, 0.0);
        occlusion += facing * (1.0 - distanceValue / 0.85);
    }

    return clamp(1.0 - occlusion * 0.20, 0.52, 1.0);
}

void main ()
{
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    ivec2 dimensions = textureSize(sPositionDepth, 0);

    if (pixel.x >= dimensions.x || pixel.y >= dimensions.y)
    {
        return;
    }

    vec4 positionDepth = texelFetch(sPositionDepth, pixel, 0);
    vec3 direct = texelFetch(sDirect, pixel, 0).xyz;

    if (positionDepth.w <= 0.0)
    {
        imageStore(oFinal, pixel, vec4(direct, 1.0));
        return;
    }

    vec3 normal = normalize(texelFetch(sNormalRoughness, pixel, 0).xyz);
    ivec2 halfDimensions = textureSize(sIndirect, 0);
    ivec2 halfPixel = clamp(pixel / 2, ivec2(0), halfDimensions - ivec2(1));

    vec3 indirect = texelFetch(sIndirect, halfPixel, 0).xyz;
    vec3 reflection = texelFetch(sReflection, halfPixel, 0).xyz;
    float ao = shortRangeAo(pixel, positionDepth.xyz, normal);

    vec3 finalHdr = direct + indirect * ao + reflection;
    imageStore(oFinal, pixel, vec4(toneMap(finalHdr), 1.0));
}
)GLSL";

GLint surfaceInternalFormat (SurfaceFormat format)
{
    return format == SurfaceFormat::Rgba8 ? GL_RGBA8 : GL_RGBA16F;
}

GLenum surfacePixelType (SurfaceFormat format)
{
    return format == SurfaceFormat::Rgba8 ? GL_UNSIGNED_BYTE : GL_FLOAT;
}

bool ensureTexture (
            GLuint *texture,
            int width,
            int height,
            GLint filter,
            SurfaceFormat format
    )
{
    if (!texture)
    {
        return false;
    }

    if (*texture == 0)
    {
        *texture = lwcgl_glGenTexture();
    }

    if (*texture == 0)
    {
        return false;
    }

    glBindTexture(GL_TEXTURE_2D, *texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(
            GL_TEXTURE_2D,
            0,
            surfaceInternalFormat(format),
            width,
            height,
            0,
            GL_RGBA,
            surfacePixelType(format),
            nullptr
        );
    return true;
}

void deleteTexture (GLuint *texture)
{
    if (!texture || *texture == 0)
    {
        return;
    }

    glDeleteTextures(*texture);
    *texture = 0;
}

void setError (std::string *error, const char *message)
{
    if (error)
    {
        *error = message ? message : "GPU Lumen error";
    }
}

} // namespace

bool LumenGpu::init (std::string *error)
{
    shutdown();

    trace_program_ = createComputeProgram(LUMEN_TRACE_COMPUTE, error);

    if (trace_program_ == 0)
    {
        return false;
    }

    composite_program_ = createComputeProgram(LUMEN_COMPOSITE_COMPUTE, error);

    if (composite_program_ == 0)
    {
        shutdown();
        return false;
    }

    trace_view_projection_location_ = GL20.glGetUniformLocation(
            trace_program_,
            "uViewProjection"
        );
    trace_camera_location_ = GL20.glGetUniformLocation(
            trace_program_,
            "uCameraPosition"
        );
    trace_primitive_count_location_ = GL20.glGetUniformLocation(
            trace_program_,
            "uPrimitiveCount"
        );
    trace_frame_location_ = GL20.glGetUniformLocation(
            trace_program_,
            "uFrameIndex"
        );
    trace_history_valid_location_ = GL20.glGetUniformLocation(
            trace_program_,
            "uHistoryValid"
        );

    if (trace_view_projection_location_ < 0
            || trace_camera_location_ < 0
            || trace_primitive_count_location_ < 0
            || trace_frame_location_ < 0
            || trace_history_valid_location_ < 0)
    {
        setError(error, "GPU Lumen trace uniforms are unavailable");
        shutdown();
        return false;
    }

    if (error)
    {
        error->clear();
    }

    return true;
}

bool LumenGpu::resize (int width, int height, std::string *error)
{
    const bool resources_ready =
        indirect_history_[0] != 0
        && indirect_history_[1] != 0
        && reflection_history_[0] != 0
        && reflection_history_[1] != 0
        && position_history_[0] != 0
        && position_history_[1] != 0
        && final_color_ != 0;

    if (!resizeStorageRequired(
            width_, height_, resources_ready, width, height))
    {
        return true;
    }

    width_ = normalizedExtent(width);
    height_ = normalizedExtent(height);
    trace_width_ = (width_ + 1) / 2;
    trace_height_ = (height_ + 1) / 2;

    bool storage_ok = true;

    for (int index = 0; index < 2; ++index)
    {
        storage_ok = storage_ok && ensureTexture(
                &indirect_history_[index],
                trace_width_, trace_height_, GL_LINEAR, LUMEN_HISTORY_FORMAT
            );
        storage_ok = storage_ok && ensureTexture(
                &reflection_history_[index],
                trace_width_, trace_height_, GL_LINEAR, LUMEN_HISTORY_FORMAT
            );
        storage_ok = storage_ok && ensureTexture(
                &position_history_[index],
                trace_width_, trace_height_, GL_NEAREST, LUMEN_POSITION_HISTORY_FORMAT
            );
    }

    storage_ok = storage_ok && ensureTexture(
            &final_color_, width_, height_, GL_LINEAR, LUMEN_FINAL_FORMAT
        );
    glBindTexture(GL_TEXTURE_2D, 0);

    if (!storage_ok)
    {
        setError(error, "failed to allocate GPU Lumen temporal textures");
        destroyTextures();
        return false;
    }

    history_index_ = 0;
    history_valid_ = false;

    if (error)
    {
        error->clear();
    }

    return true;
}

void LumenGpu::destroyTextures ()
{
    for (int index = 0; index < 2; ++index)
    {
        deleteTexture(&indirect_history_[index]);
        deleteTexture(&reflection_history_[index]);
        deleteTexture(&position_history_[index]);
    }

    deleteTexture(&final_color_);
    history_index_ = 0;
    history_valid_ = false;
}

void LumenGpu::shutdown ()
{
    destroyTextures();

    destroyProgram(&trace_program_);
    destroyProgram(&composite_program_);

    trace_view_projection_location_ = -1;
    trace_camera_location_ = -1;
    trace_primitive_count_location_ = -1;
    trace_bvh_node_count_location_ = -1;
    trace_frame_location_ = -1;
    trace_history_valid_location_ = -1;
    bvh_shader_validated_ = false;
    bvh_trace_active_ = false;

    width_ = 0;
    height_ = 0;
    trace_width_ = 0;
    trace_height_ = 0;
}

bool LumenGpu::ready () const
{
    return trace_program_ != 0
        && composite_program_ != 0
        && indirect_history_[0] != 0
        && indirect_history_[1] != 0
        && reflection_history_[0] != 0
        && reflection_history_[1] != 0
        && position_history_[0] != 0
        && position_history_[1] != 0
        && final_color_ != 0;
}

} // namespace Gpu
} // namespace Renderer
