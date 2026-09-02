#include "DirectLightingGpu.hpp"

#include "Renderer/Gpu/Gpu.hpp"

#include <algorithm>
#include <cstdio>

namespace Renderer
{
namespace Gpu
{
namespace
{

constexpr const char *DIRECT_LIGHTING_COMPUTE = R"GLSL(
#version 430 core

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(rgba16f, binding = 0) readonly uniform image2D gPositionDepth;
layout(rgba16f, binding = 1) readonly uniform image2D gNormalRoughness;
layout(rgba16f, binding = 2) readonly uniform image2D gAlbedoMetallic;
layout(rgba16f, binding = 3) readonly uniform image2D gEmissive;
layout(rgba16f, binding = 4) writeonly uniform image2D oDirect;

struct LightData
{
    vec4 positionType;
    vec4 directionRange;
    vec4 colorIntensity;
    vec4 coneShadow;
};

struct PrimitiveData
{
    vec4 positionType;
    vec4 rotation;
    vec4 scale;
    vec4 albedoMetallic;
    vec4 emissiveRoughness;
};

layout(std430, binding = 5) readonly buffer LightBuffer
{
    LightData lights[];
};

layout(std430, binding = 6) readonly buffer PrimitiveBuffer
{
    PrimitiveData primitives[];
};

uniform vec3 uCameraPosition;
uniform int uLightCount;
uniform int uPrimitiveCount;

const float PI = 3.14159265358979323846;
const float EPSILON = 0.00001;
const float SHADOW_BIAS = 0.004;

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

bool intersectBox (
        vec3 rayOrigin,
        vec3 rayDirection,
        PrimitiveData primitive,
        float maximumDistance)
{
    vec3 scaleValue = primitive.scale.xyz;
    vec3 safeScale = vec3(
        abs(scaleValue.x) > EPSILON ? scaleValue.x : 0.00001,
        abs(scaleValue.y) > EPSILON ? scaleValue.y : 0.00001,
        abs(scaleValue.z) > EPSILON ? scaleValue.z : 0.00001
    );

    vec3 localOrigin = inverseRotate(
        rayOrigin - primitive.positionType.xyz,
        primitive.rotation.xyz
    ) / safeScale;

    vec3 localDirection = inverseRotate(
        rayDirection,
        primitive.rotation.xyz
    ) / safeScale;

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

    if (tFar < max(tNear, SHADOW_BIAS))
    {
        return false;
    }

    float hitDistance = tNear > SHADOW_BIAS ? tNear : tFar;
    return hitDistance > SHADOW_BIAS && hitDistance < maximumDistance;
}

bool intersectPlane (
        vec3 rayOrigin,
        vec3 rayDirection,
        PrimitiveData primitive,
        float maximumDistance)
{
    vec3 scaleValue = primitive.scale.xyz;
    vec3 safeScale = vec3(
        abs(scaleValue.x) > EPSILON ? scaleValue.x : 0.00001,
        abs(scaleValue.y) > EPSILON ? scaleValue.y : 0.00001,
        abs(scaleValue.z) > EPSILON ? scaleValue.z : 0.00001
    );

    vec3 localOrigin = inverseRotate(
        rayOrigin - primitive.positionType.xyz,
        primitive.rotation.xyz
    ) / safeScale;

    vec3 localDirection = inverseRotate(
        rayDirection,
        primitive.rotation.xyz
    ) / safeScale;

    if (abs(localDirection.y) <= EPSILON)
    {
        return false;
    }

    float hitDistance = -localOrigin.y / localDirection.y;

    if (hitDistance <= SHADOW_BIAS || hitDistance >= maximumDistance)
    {
        return false;
    }

    vec3 localHit = localOrigin + localDirection * hitDistance;
    return abs(localHit.x) <= 0.5 && abs(localHit.z) <= 0.5;
}

bool shadowed (vec3 position, vec3 normal, vec3 direction, float maximumDistance)
{
    vec3 origin = position + normalize(normal) * SHADOW_BIAS * 2.0;

    for (int index = 0; index < uPrimitiveCount; ++index)
    {
        PrimitiveData primitive = primitives[index];
        int type = int(primitive.positionType.w + 0.5);

        bool hit = type == 0
            ? intersectBox(origin, direction, primitive, maximumDistance)
            : intersectPlane(origin, direction, primitive, maximumDistance);

        if (hit)
        {
            return true;
        }
    }

    return false;
}

float pointAttenuation (float distanceValue, float rangeValue)
{
    if (rangeValue <= EPSILON || distanceValue >= rangeValue)
    {
        return 0.0;
    }

    float ratio = distanceValue / rangeValue;
    float ratioSquared = ratio * ratio;
    float rangeFactor = clamp(1.0 - ratioSquared * ratioSquared, 0.0, 1.0);
    return rangeFactor * rangeFactor / (distanceValue * distanceValue + 1.0);
}

float geometrySchlickGgx (float cosineValue, float roughness)
{
    float r = roughness + 1.0;
    float k = r * r / 8.0;
    return cosineValue / (cosineValue * (1.0 - k) + k + EPSILON);
}

vec3 fresnelSchlick (float cosineValue, vec3 f0)
{
    float factor = pow(1.0 - clamp(cosineValue, 0.0, 1.0), 5.0);
    return f0 + (vec3(1.0) - f0) * factor;
}

float distributionGgx (vec3 normal, vec3 halfway, float roughness)
{
    float alpha = roughness * roughness;
    float alphaSquared = alpha * alpha;
    float cosineValue = clamp(dot(normal, halfway), 0.0, 1.0);
    float cosineSquared = cosineValue * cosineValue;
    float denominator = cosineSquared * (alphaSquared - 1.0) + 1.0;
    return alphaSquared / (PI * denominator * denominator + EPSILON);
}

float geometrySmith (vec3 normal, vec3 viewDirection, vec3 lightDirection, float roughness)
{
    float normalView = clamp(dot(normal, viewDirection), 0.0, 1.0);
    float normalLight = clamp(dot(normal, lightDirection), 0.0, 1.0);
    return geometrySchlickGgx(normalView, roughness)
         * geometrySchlickGgx(normalLight, roughness);
}

vec3 evaluatePbr (
        vec3 albedo,
        float metallic,
        float roughness,
        vec3 normal,
        vec3 viewDirection,
        vec3 lightDirection,
        vec3 radiance)
{
    vec3 n = normalize(normal);
    vec3 v = normalize(viewDirection);
    vec3 l = normalize(lightDirection);
    vec3 h = normalize(v + l);

    float normalLight = clamp(dot(n, l), 0.0, 1.0);
    float normalView = clamp(dot(n, v), 0.0, 1.0);

    if (normalLight <= 0.0 || normalView <= 0.0)
    {
        return vec3(0.0);
    }

    vec3 f0 = mix(vec3(0.04), albedo, clamp(metallic, 0.0, 1.0));
    vec3 fresnel = fresnelSchlick(dot(h, v), f0);
    float clampedRoughness = clamp(roughness, 0.04, 1.0);
    float distribution = distributionGgx(n, h, clampedRoughness);
    float geometry = geometrySmith(n, v, l, clampedRoughness);
    float denominator = 4.0 * normalView * normalLight + EPSILON;

    vec3 specular = fresnel * distribution * geometry / denominator;
    vec3 diffuseWeight = (vec3(1.0) - fresnel) * (1.0 - clamp(metallic, 0.0, 1.0));
    vec3 diffuse = albedo / PI;
    return (diffuseWeight * diffuse + specular) * radiance * normalLight;
}

void main ()
{
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    ivec2 dimensions = imageSize(gPositionDepth);

    if (pixel.x >= dimensions.x || pixel.y >= dimensions.y)
    {
        return;
    }

    vec4 positionDepth = imageLoad(gPositionDepth, pixel);

    if (positionDepth.w <= 0.0)
    {
        imageStore(oDirect, pixel, vec4(0.055, 0.070, 0.105, 1.0));
        return;
    }

    vec4 normalRoughness = imageLoad(gNormalRoughness, pixel);
    vec4 albedoMetallic = imageLoad(gAlbedoMetallic, pixel);
    vec3 emissive = imageLoad(gEmissive, pixel).xyz;

    vec3 position = positionDepth.xyz;
    vec3 normal = normalize(normalRoughness.xyz);
    float roughness = normalRoughness.w;
    vec3 albedo = albedoMetallic.xyz;
    float metallic = albedoMetallic.w;
    vec3 viewDirection = normalize(uCameraPosition - position);

    vec3 direct = emissive;

    for (int index = 0; index < uLightCount; ++index)
    {
        LightData light = lights[index];
        int type = int(light.positionType.w + 0.5);

        vec3 lightDirection = vec3(0.0);
        vec3 radiance = vec3(0.0);
        float maximumDistance = 10000.0;

        if (type == 0)
        {
            lightDirection = normalize(-light.directionRange.xyz);
            radiance = light.colorIntensity.xyz * light.colorIntensity.w;
        }
        else
        {
            vec3 toLight = light.positionType.xyz - position;
            float distanceValue = length(toLight);

            if (distanceValue <= EPSILON)
            {
                continue;
            }

            float attenuation = pointAttenuation(distanceValue, light.directionRange.w);

            if (attenuation <= 0.0)
            {
                continue;
            }

            lightDirection = toLight / distanceValue;
            maximumDistance = max(SHADOW_BIAS, distanceValue - SHADOW_BIAS * 2.0);

            float cone = 1.0;

            if (type == 2)
            {
                vec3 fromLight = normalize(position - light.positionType.xyz);
                float cosineValue = dot(normalize(light.directionRange.xyz), fromLight);
                float inner = light.coneShadow.x;
                float outer = light.coneShadow.y;

                if (inner <= outer + EPSILON)
                {
                    cone = cosineValue >= outer ? 1.0 : 0.0;
                }
                else
                {
                    cone = clamp((cosineValue - outer) / (inner - outer), 0.0, 1.0);
                }
            }

            if (cone <= 0.0)
            {
                continue;
            }

            radiance = light.colorIntensity.xyz
                     * light.colorIntensity.w
                     * attenuation
                     * cone;
        }

        if (dot(radiance, radiance) <= 0.0)
        {
            continue;
        }

        if (light.coneShadow.z > 0.5
                && shadowed(position, normal, lightDirection, maximumDistance))
        {
            continue;
        }

        direct += evaluatePbr(
            albedo,
            metallic,
            roughness,
            normal,
            viewDirection,
            lightDirection,
            radiance
        );
    }

    imageStore(oDirect, pixel, vec4(direct, 1.0));
}
)GLSL";

GLuint createTexture (int width, int height)
{
    const GLuint texture = lwcgl_glGenTexture();

    if (texture == 0)
    {
        return 0;
    }

    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGBA16F,
            width,
            height,
            0,
            GL_RGBA,
            GL_FLOAT,
            nullptr
        );
    return texture;
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
        *error = message ? message : "GPU direct lighting error";
    }
}

} // namespace

bool DirectLightingGpu::init (std::string *error)
{
    shutdown();

    program_ = createComputeProgram(DIRECT_LIGHTING_COMPUTE, error);

    if (program_ == 0)
    {
        return false;
    }

    camera_location_ = GL20.glGetUniformLocation(program_, "uCameraPosition");
    light_count_location_ = GL20.glGetUniformLocation(program_, "uLightCount");
    primitive_count_location_ = GL20.glGetUniformLocation(program_, "uPrimitiveCount");

    if (camera_location_ < 0
            || light_count_location_ < 0
            || primitive_count_location_ < 0)
    {
        setError(error, "GPU direct lighting uniforms are unavailable");
        shutdown();
        return false;
    }

    GL15.glGenBuffers(1, &light_buffer_);
    GL15.glGenBuffers(1, &primitive_buffer_);

    if (light_buffer_ == 0 || primitive_buffer_ == 0)
    {
        setError(error, "failed to allocate GPU lighting scene buffers");
        shutdown();
        return false;
    }

    if (error)
    {
        error->clear();
    }

    return true;
}

bool DirectLightingGpu::resize (int width, int height, std::string *error)
{
    const int new_width = std::max(1, width);
    const int new_height = std::max(1, height);

    if (new_width == width_
            && new_height == height_
            && direct_color_ != 0
            && final_color_ != 0)
    {
        return true;
    }

    width_ = new_width;
    height_ = new_height;
    destroyTextures();

    direct_color_ = createTexture(width_, height_);
    final_color_ = createTexture(width_, height_);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (direct_color_ == 0 || final_color_ == 0)
    {
        setError(error, "failed to allocate GPU direct lighting textures");
        destroyTextures();
        return false;
    }

    if (error)
    {
        error->clear();
    }

    return true;
}

bool DirectLightingGpu::uploadBuffer (
                GLuint buffer,
                std::size_t *capacity,
                const void *data,
                std::size_t size,
                std::string *error
        )
{
    if (buffer == 0 || !capacity)
    {
        setError(error, "invalid GPU lighting upload destination");
        return false;
    }

    GL15.glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);

    if (size == 0)
    {
        if (*capacity == 0)
        {
            GL15.glBufferData(
                    GL_SHADER_STORAGE_BUFFER,
                    16,
                    nullptr,
                    GL_DYNAMIC_DRAW
                );
            *capacity = 16;
        }
        return true;
    }

    if (size > *capacity)
    {
        std::size_t new_capacity = std::max<std::size_t>(256u, *capacity);

        while (new_capacity < size)
        {
            new_capacity *= 2u;
        }

        GL15.glBufferData(
                GL_SHADER_STORAGE_BUFFER,
                static_cast<LWCGLsizeiptr>(new_capacity),
                nullptr,
                GL_DYNAMIC_DRAW
            );
        *capacity = new_capacity;
    }

    GL15.glBufferSubData(
            GL_SHADER_STORAGE_BUFFER,
            0,
            static_cast<LWCGLsizeiptr>(size),
            data
        );

    return true;
}

bool DirectLightingGpu::render (
                const Ecs::World& world,
                const GBufferGpu& gbuffer,
                const Math::Vec3& camera_position,
                std::string *error
        )
{
    return updateScene(world, error)
        && dispatch(gbuffer, camera_position, error);
}

void DirectLightingGpu::destroyTextures ()
{
    deleteTexture(&direct_color_);
    deleteTexture(&final_color_);
}

void DirectLightingGpu::shutdown ()
{
    destroyTextures();

    if (light_buffer_ != 0)
    {
        GL15.glDeleteBuffers(1, &light_buffer_);
        light_buffer_ = 0;
    }

    if (primitive_buffer_ != 0)
    {
        GL15.glDeleteBuffers(1, &primitive_buffer_);
        primitive_buffer_ = 0;
    }

    destroyProgram(&program_);

    light_capacity_ = 0;
    primitive_capacity_ = 0;
    lights_.clear();
    primitives_.clear();
    uploaded_lights_.clear();
    uploaded_primitives_.clear();
    camera_location_ = -1;
    light_count_location_ = -1;
    primitive_count_location_ = -1;
    width_ = 0;
    height_ = 0;
}

} // namespace Gpu
} // namespace Renderer
