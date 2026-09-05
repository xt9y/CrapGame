#include "Renderer/Gpu/SmrtShadowGpu.hpp"

#include "Renderer/Gpu/GBufferGpu.hpp"
#include "Renderer/Gpu/GBufferReconstruct.hpp"
#include "Renderer/Gpu/Gpu.hpp"
#include "Renderer/Gpu/SmrtShadowShader.hpp"
#include "Renderer/Gpu/VirtualShadowMapGpu.hpp"

#include <string>

namespace Renderer
{
namespace Gpu
{
namespace
{

void setError (std::string *error, const char *message)
{
    if (error) *error = message ? message : "SMRT shadow error";
}

bool replaceRequired (
            std::string *source,
            const char *from,
            const char *to,
            std::string *error
    )
{
    if (!source || !from || !to)
    {
        setError(error, "invalid SMRT shader patch");
        return false;
    }

    const std::size_t at = source->find(from);
    if (at == std::string::npos)
    {
        setError(error, "SMRT shader patch point is unavailable");
        return false;
    }

    source->replace(at, std::char_traits<char>::length(from), to);
    return true;
}

Math::Vec3 lightForward (const Ecs::TransformComponent& transform)
{
    const Math::Vec3 rotation = {
        transform.rotation.x,
        transform.rotation.y,
        transform.rotation.z,
    };

    return Math::normalize(
            Math::transformDirection(
                    Math::rotationEuler(rotation),
                    {0.0f, 0.0f, -1.0f}
                )
        );
}

void deleteTexture (GLuint *texture)
{
    if (!texture || *texture == 0) return;
    glDeleteTextures(*texture);
    *texture = 0;
}

} // namespace

bool SmrtShadowGpu::init (std::string *error)
{
    shutdown();

    std::string shader = smrtShadowComputeShader();
    if (!replaceRequired(
            &shader,
            "uniform uint uSmrtFrameIndex;",
            "uniform int uSmrtFrameIndex;",
            error)
            || !replaceRequired(
                    &shader,
                    "shadowLight.sourceShape.x,shadowLight.sourceShape.y,uSmrtFrameIndex);",
                    "shadowLight.sourceShape.x,shadowLight.sourceShape.y,uint(max(uSmrtFrameIndex,0)));",
                    error))
    {
        return false;
    }

    program_ = createComputeProgram(shader.c_str(), error);
    if (program_ == 0)
    {
        return false;
    }

    inverse_view_projection_location_ = GL20.glGetUniformLocation(
            program_, "uSmrtInverseViewProjection");
    view_projection_location_ = GL20.glGetUniformLocation(
            program_, "uSmrtViewProjection");
    camera_location_ = GL20.glGetUniformLocation(
            program_, "uSmrtCameraPosition");
    light_direction_location_ = GL20.glGetUniformLocation(
            program_, "uSmrtLightDirection");
    light_index_location_ = GL20.glGetUniformLocation(
            program_, "uSmrtLightIndex");
    light_type_location_ = GL20.glGetUniformLocation(
            program_, "uSmrtLightType");
    clipmap_count_location_ = GL20.glGetUniformLocation(
            program_, "uSmrtShadowClipmapCount");
    frame_index_location_ = GL20.glGetUniformLocation(
            program_, "uSmrtFrameIndex");

    if (inverse_view_projection_location_ < 0
            || view_projection_location_ < 0
            || camera_location_ < 0
            || light_direction_location_ < 0
            || light_index_location_ < 0
            || light_type_location_ < 0
            || clipmap_count_location_ < 0
            || frame_index_location_ < 0)
    {
        setError(error, "SMRT shader uniforms are unavailable");
        shutdown();
        return false;
    }

    if (error) error->clear();
    return true;
}

bool SmrtShadowGpu::resize (
            int width,
            int height,
            std::string *error
    )
{
    const int target_width = width > 0 ? width : 1,
              target_height = height > 0 ? height : 1;

    if (visibility_ != 0
            && width_ == target_width
            && height_ == target_height)
    {
        if (error) error->clear();
        return true;
    }

    width_ = target_width;
    height_ = target_height;
    if (visibility_ == 0) visibility_ = lwcgl_glGenTexture();
    if (visibility_ == 0)
    {
        setError(error, "failed to allocate SMRT visibility texture");
        return false;
    }

    glBindTexture(GL_TEXTURE_2D, visibility_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_R16F,
            width_,
            height_,
            0,
            GL_RED,
            GL_FLOAT,
            nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (error) error->clear();
    return true;
}

bool SmrtShadowGpu::render (
            const Ecs::World& world,
            const GBufferGpu& gbuffer,
            const VirtualShadowMapGpu& virtual_shadow_map,
            const Math::Vec3& camera_position,
            std::uint64_t frame_index,
            std::string *error
    )
{
    if (!ready() || !gbuffer.ready() || !virtual_shadow_map.ready()
            || gbuffer.width() != width_
            || gbuffer.height() != height_)
    {
        setError(error, "SMRT resources are not ready");
        return false;
    }

    Math::Vec3 direction = {0.0f, -1.0f, 0.0f};
    light_index_ = -1;
    int active_light_index = 0;
    for (const Ecs::Entity entity : world.entities())
    {
        const Ecs::TransformComponent *transform = world.getTransform(entity);
        const Ecs::LightComponent *light = world.getLight(entity);
        if (!transform || !light || light->intensity <= 0.0f) continue;

        if (light_index_ < 0
                && light->casts_shadows
                && light->type == Ecs::LightType::Directional)
        {
            light_index_ = active_light_index;
            direction = lightForward(*transform);
        }
        ++active_light_index;
    }

    Math::Mat4 view_projection = {};
    if (!invertGBufferViewProjection(
            gbuffer.inverseViewProjection(),
            &view_projection))
    {
        setError(error, "SMRT view projection is singular");
        return false;
    }

    GL30.glBindBufferBase(
            GL_SHADER_STORAGE_BUFFER,
            0,
            virtual_shadow_map.pageCache().metadataBuffer());
    GL30.glBindBufferBase(
            GL_SHADER_STORAGE_BUFFER,
            1,
            virtual_shadow_map.pageCache().pageTableBuffer());
    GL30.glBindBufferBase(
            GL_SHADER_STORAGE_BUFFER,
            2,
            virtual_shadow_map.clipmapBuffer());
    GL30.glBindBufferBase(
            GL_SHADER_STORAGE_BUFFER,
            3,
            virtual_shadow_map.shadowLightBuffer());

    GLModern.glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gbuffer.depthTexture());
    GLModern.glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, gbuffer.normalRoughnessTexture());
    GLModern.glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, virtual_shadow_map.depthAtlas());

    GL20.glUseProgram(program_);
    GL20.glUniformMatrix4fv(
            inverse_view_projection_location_,
            1,
            GL_FALSE,
            gbuffer.inverseViewProjection().value);
    GL20.glUniformMatrix4fv(
            view_projection_location_,
            1,
            GL_FALSE,
            view_projection.value);
    GL20.glUniform3f(
            camera_location_,
            camera_position.x,
            camera_position.y,
            camera_position.z);
    GL20.glUniform3f(
            light_direction_location_,
            direction.x,
            direction.y,
            direction.z);
    GL20.glUniform1i(light_index_location_, light_index_);
    GL20.glUniform1i(light_type_location_, 0);
    GL20.glUniform1i(
            clipmap_count_location_,
            VirtualShadowMapGpu::CLIPMAP_COUNT);
    GL20.glUniform1i(
            frame_index_location_,
            static_cast<GLint>(frame_index & 0x7fffffffu));

    GL42.glBindImageTexture(
            0,
            visibility_,
            0,
            GL_FALSE,
            0,
            GL_WRITE_ONLY,
            GL_R16F);
    GL43.glDispatchCompute(
            static_cast<GLuint>((width_ + 7) / 8),
            static_cast<GLuint>((height_ + 7) / 8),
            1);
    GL42.glMemoryBarrier(
            GL_SHADER_IMAGE_ACCESS_BARRIER_BIT |
            GL_TEXTURE_FETCH_BARRIER_BIT);
    GL20.glUseProgram(0);

    GLModern.glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, 0);
    GLModern.glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    GLModern.glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (error) error->clear();
    return true;
}

void SmrtShadowGpu::shutdown ()
{
    deleteTexture(&visibility_);
    destroyProgram(&program_);
    inverse_view_projection_location_ = -1;
    view_projection_location_ = -1;
    camera_location_ = -1;
    light_direction_location_ = -1;
    light_index_location_ = -1;
    light_type_location_ = -1;
    clipmap_count_location_ = -1;
    frame_index_location_ = -1;
    width_ = 0;
    height_ = 0;
    light_index_ = -1;
}

} // namespace Gpu
} // namespace Renderer
