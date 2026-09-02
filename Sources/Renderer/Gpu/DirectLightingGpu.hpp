#ifndef CRAPGAME_RENDERER_GPU_DIRECTLIGHTINGGPU_HPP
#define CRAPGAME_RENDERER_GPU_DIRECTLIGHTINGGPU_HPP

#include "Ecs/Ecs.hpp"
#include "Renderer/Gpu/GBufferGpu.hpp"
#include "Renderer/Math/Math.hpp"

#include <lwcgl/lwcgl.h>
#include <lwcgl/glmodern.h>

#include <cmath>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

namespace Renderer
{
namespace Gpu
{

class DirectLightingGpu
{
public:
    bool init (std::string *error = nullptr);
    bool resize (int width, int height, std::string *error = nullptr);

    bool updateScene (
                const Ecs::World& world,
                std::string *error = nullptr
        )
    {
        if (program_ == 0 || light_buffer_ == 0 || primitive_buffer_ == 0)
        {
            setInlineError(error, "GPU direct-light scene buffers are not initialized");
            return false;
        }

        lights_.clear();
        primitives_.clear();

        for (const Ecs::Entity entity : world.entities())
        {
            const Ecs::TransformComponent *transform = world.getTransform(entity);
            const Ecs::LightComponent *light = world.getLight(entity);

            if (transform && light && light->intensity > 0.0f)
            {
                LightGpu gpu = {};
                const Math::Vec3 forward = lightForwardInline(*transform);

                gpu.position_type[0] = transform->position.x;
                gpu.position_type[1] = transform->position.y;
                gpu.position_type[2] = transform->position.z;
                gpu.position_type[3] = light->type == Ecs::LightType::Directional
                    ? 0.0f
                    : light->type == Ecs::LightType::Point ? 1.0f : 2.0f;

                gpu.direction_range[0] = forward.x;
                gpu.direction_range[1] = forward.y;
                gpu.direction_range[2] = forward.z;
                gpu.direction_range[3] = light->range;

                gpu.color_intensity[0] = light->color.x;
                gpu.color_intensity[1] = light->color.y;
                gpu.color_intensity[2] = light->color.z;
                gpu.color_intensity[3] = light->intensity;

                gpu.cone_shadow[0] = std::cos(Math::radians(light->inner_cone));
                gpu.cone_shadow[1] = std::cos(Math::radians(light->outer_cone));
                gpu.cone_shadow[2] = light->casts_shadows ? 1.0f : 0.0f;
                gpu.cone_shadow[3] = light->indirect_intensity;

                lights_.push_back(gpu);
            }

            const Ecs::MeshComponent *mesh = world.getMesh(entity);
            const Ecs::RenderableComponent *renderable = world.getRenderable(entity);
            const Ecs::MaterialComponent *material = world.getMaterial(entity);

            if (!transform
                    || !mesh
                    || !renderable
                    || !renderable->visible
                    || !material)
            {
                continue;
            }

            PrimitiveGpu primitive = {};
            primitive.position_type[0] = transform->position.x;
            primitive.position_type[1] = transform->position.y;
            primitive.position_type[2] = transform->position.z;
            primitive.position_type[3] = mesh->mesh == Ecs::MeshType::Cube ? 0.0f : 1.0f;

            primitive.rotation[0] = transform->rotation.x;
            primitive.rotation[1] = transform->rotation.y;
            primitive.rotation[2] = transform->rotation.z;

            primitive.scale[0] = transform->scale.x;
            primitive.scale[1] = transform->scale.y;
            primitive.scale[2] = transform->scale.z;
            primitive.scale[3] = 1.0f;

            primitive.albedo_metallic[0] = material->albedo.x;
            primitive.albedo_metallic[1] = material->albedo.y;
            primitive.albedo_metallic[2] = material->albedo.z;
            primitive.albedo_metallic[3] = material->metallic;

            primitive.emissive_roughness[0] = material->emissive.x * material->emissive_strength;
            primitive.emissive_roughness[1] = material->emissive.y * material->emissive_strength;
            primitive.emissive_roughness[2] = material->emissive.z * material->emissive_strength;
            primitive.emissive_roughness[3] = material->roughness;

            primitives_.push_back(primitive);
        }

        if (light_capacity_ == 0 || !sameVectorBytes(lights_, uploaded_lights_))
        {
            if (!uploadBuffer(
                    light_buffer_,
                    &light_capacity_,
                    lights_.data(),
                    lights_.size() * sizeof(LightGpu),
                    error
                ))
            {
                return false;
            }
            uploaded_lights_ = lights_;
        }

        if (primitive_capacity_ == 0 || !sameVectorBytes(primitives_, uploaded_primitives_))
        {
            if (!uploadBuffer(
                    primitive_buffer_,
                    &primitive_capacity_,
                    primitives_.data(),
                    primitives_.size() * sizeof(PrimitiveGpu),
                    error
                ))
            {
                return false;
            }
            uploaded_primitives_ = primitives_;
        }

        if (error)
        {
            error->clear();
        }
        return true;
    }

    bool dispatch (
                const GBufferGpu& gbuffer,
                const Math::Vec3& camera_position,
                std::string *error = nullptr
        )
    {
        if (!ready()
                || !gbuffer.ready()
                || gbuffer.width() != width_
                || gbuffer.height() != height_)
        {
            setInlineError(error, "GPU direct lighting resources are not ready for this GBuffer");
            return false;
        }

        GL20.glUseProgram(program_);
        GL20.glUniform3f(
                camera_location_,
                camera_position.x,
                camera_position.y,
                camera_position.z
            );
        GL20.glUniform1i(light_count_location_, static_cast<GLint>(lights_.size()));
        GL20.glUniform1i(primitive_count_location_, static_cast<GLint>(primitives_.size()));

        GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, light_buffer_);
        GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, primitive_buffer_);

        GL42.glBindImageTexture(0, gbuffer.positionDepthTexture(), 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);
        GL42.glBindImageTexture(1, gbuffer.normalRoughnessTexture(), 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);
        GL42.glBindImageTexture(2, gbuffer.albedoMetallicTexture(), 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);
        GL42.glBindImageTexture(3, gbuffer.emissiveTexture(), 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);
        GL42.glBindImageTexture(4, direct_color_, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
        GL42.glBindImageTexture(5, final_color_, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);

        GL43.glDispatchCompute(
                static_cast<GLuint>((width_ + 7) / 8),
                static_cast<GLuint>((height_ + 7) / 8),
                1
            );

        GL42.glMemoryBarrier(
                GL_SHADER_IMAGE_ACCESS_BARRIER_BIT |
                GL_TEXTURE_FETCH_BARRIER_BIT
            );
        GL20.glUseProgram(0);

        if (error)
        {
            error->clear();
        }
        return true;
    }

    bool render (
                const Ecs::World& world,
                const GBufferGpu& gbuffer,
                const Math::Vec3& camera_position,
                std::string *error = nullptr
        );

    void shutdown ();

    bool ready () const { return program_ != 0 && direct_color_ != 0 && final_color_ != 0; }
    GLuint directTexture () const { return direct_color_; }
    GLuint finalTexture () const { return final_color_; }

    /* The analytic-shadow primitive layout is intentionally shared with
     * LumenGpu. One ECS extraction/upload feeds both compute pipelines. */
    GLuint primitiveBuffer () const { return primitive_buffer_; }
    std::size_t primitiveCount () const { return primitives_.size(); }

private:
    struct LightGpu
    {
        float position_type[4];
        float direction_range[4];
        float color_intensity[4];
        float cone_shadow[4];
    };

    struct PrimitiveGpu
    {
        float position_type[4];
        float rotation[4];
        float scale[4];
        float albedo_metallic[4];
        float emissive_roughness[4];
    };

    template <typename T>
    static bool sameVectorBytes (
                const std::vector<T>& a,
                const std::vector<T>& b
        )
    {
        return a.size() == b.size()
            && (a.empty()
                || std::memcmp(
                        a.data(),
                        b.data(),
                        a.size() * sizeof(T)
                    ) == 0);
    }

    static Math::Vec3 toVec3Inline (const Ecs::Vec3& value)
    {
        return {value.x, value.y, value.z};
    }

    static Math::Vec3 lightForwardInline (
                const Ecs::TransformComponent& transform
        )
    {
        return Math::normalize(
                Math::transformDirection(
                        Math::rotationEuler(toVec3Inline(transform.rotation)),
                        {0.0f, 0.0f, -1.0f}
                    )
            );
    }

    static void setInlineError (std::string *error, const char *message)
    {
        if (error)
        {
            *error = message ? message : "GPU direct lighting error";
        }
    }

    bool uploadBuffer (
                GLuint buffer,
                std::size_t *capacity,
                const void *data,
                std::size_t size,
                std::string *error
        );

    void destroyTextures ();

    GLuint program_ = 0;
    GLuint light_buffer_ = 0;
    GLuint primitive_buffer_ = 0;
    GLuint direct_color_ = 0;
    GLuint final_color_ = 0;

    GLint camera_location_ = -1;
    GLint light_count_location_ = -1;
    GLint primitive_count_location_ = -1;

    std::size_t light_capacity_ = 0;
    std::size_t primitive_capacity_ = 0;

    std::vector<LightGpu> lights_;
    std::vector<PrimitiveGpu> primitives_;
    std::vector<LightGpu> uploaded_lights_;
    std::vector<PrimitiveGpu> uploaded_primitives_;

    int width_ = 0;
    int height_ = 0;
};

} // namespace Gpu
} // namespace Renderer

#endif
