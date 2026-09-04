#include "Renderer/Gpu/StaticShadowCacheGpu.hpp"

#include "Renderer/Gpu/Gpu.hpp"
#include "Renderer/Gpu/StaticShadowShader.hpp"
#include "Renderer/Gpu/TriangleScene.hpp"
#include "Renderer/Material/Material.hpp"
#include "Renderer/Mesh/Mesh.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace Renderer
{
namespace Gpu
{
namespace
{

void setError (std::string *error, const char *message)
{
    if (error) *error = message ? message : "static shadow cache error";
}

int channelCode (char channel)
{
    switch (channel)
    {
        case 'g': case 'G': return 1;
        case 'b': case 'B': return 2;
        case 'a': case 'A': return 3;
        case 'm': case 'M': return 4;
        default: return 0;
    }
}

Math::Vec3 toVec3 (const Ecs::Vec3& value)
{
    return {value.x, value.y, value.z};
}

Math::Mat4 orthographicForViewBounds (
        const Math::Vec3& minimum,
        const Math::Vec3& maximum)
{
    const float dx = maximum.x - minimum.x;
    const float dy = maximum.y - minimum.y;
    const float dz = maximum.z - minimum.z;
    Math::Mat4 result = {};
    if (dx <= 1.0e-6f || dy <= 1.0e-6f || dz <= 1.0e-6f)
        return result;

    result.value[0] = 2.0f / dx;
    result.value[5] = 2.0f / dy;
    result.value[10] = -2.0f / dz;
    result.value[12] = -(maximum.x + minimum.x) / dx;
    result.value[13] = -(maximum.y + minimum.y) / dy;
    result.value[14] = (maximum.z + minimum.z) / dz;
    result.value[15] = 1.0f;
    return result;
}

void expandBounds (Math::Vec3 *minimum, Math::Vec3 *maximum)
{
    if (!minimum || !maximum) return;
    const Math::Vec3 extent = Math::subtract(*maximum, *minimum);
    const Math::Vec3 padding = {
        std::max(0.001f, extent.x * StaticShadowPolicy::PADDING),
        std::max(0.001f, extent.y * StaticShadowPolicy::PADDING),
        std::max(0.001f, extent.z * StaticShadowPolicy::PADDING),
    };
    *minimum = Math::subtract(*minimum, padding);
    *maximum = Math::add(*maximum, padding);
}

} // namespace

bool StaticShadowCacheGpu::init (std::string *error)
{
    shutdown();
    program_ = createGraphicsProgram(
        STATIC_SHADOW_VERTEX_SHADER,
        STATIC_SHADOW_FRAGMENT_SHADER,
        error
    );
    if (program_ == 0) return false;

    model_location_ = GL20.glGetUniformLocation(program_, "uModel");
    light_view_projection_location_ =
        GL20.glGetUniformLocation(program_, "uLightViewProjection");
    masked_location_ = GL20.glGetUniformLocation(program_, "uMasked");
    has_opacity_texture_location_ =
        GL20.glGetUniformLocation(program_, "uHasOpacityTexture");
    opacity_location_ = GL20.glGetUniformLocation(program_, "uOpacity");
    alpha_cutoff_location_ = GL20.glGetUniformLocation(program_, "uAlphaCutoff");
    opacity_scale_offset_location_ =
        GL20.glGetUniformLocation(program_, "uOpacityScaleOffset");
    opacity_channel_location_ =
        GL20.glGetUniformLocation(program_, "uOpacityChannel");
    opacity_sampler_location_ =
        GL20.glGetUniformLocation(program_, "uOpacityTexture");

    if (model_location_ < 0 || light_view_projection_location_ < 0
            || masked_location_ < 0 || has_opacity_texture_location_ < 0
            || opacity_location_ < 0 || alpha_cutoff_location_ < 0
            || opacity_scale_offset_location_ < 0
            || opacity_channel_location_ < 0 || opacity_sampler_location_ < 0)
    {
        setError(error, "static shadow shader uniforms are unavailable");
        shutdown();
        return false;
    }

    if (!material_gpu_.init(error) || !createStorage(error))
    {
        shutdown();
        return false;
    }

    GL20.glUseProgram(program_);
    GL20.glUniform1i(opacity_sampler_location_, 6);
    GL20.glUseProgram(0);
    if (error) error->clear();
    return true;
}

bool StaticShadowCacheGpu::createStorage (std::string *error)
{
    if (framebuffer_ == 0) GL30.glGenFramebuffers(1, &framebuffer_);
    if (depth_texture_ == 0) depth_texture_ = lwcgl_glGenTexture();
    if (framebuffer_ == 0 || depth_texture_ == 0)
    {
        setError(error, "failed to allocate static shadow cache resources");
        return false;
    }

    glBindTexture(GL_TEXTURE_2D, depth_texture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24,
        SIZE, SIZE, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr
    );

    GL30.glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
    GL30.glFramebufferTexture2D(
        GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_texture_, 0
    );
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    const GLenum status = GL30.glCheckFramebufferStatus(GL_FRAMEBUFFER);
    GL30.glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        setError(error, "static shadow framebuffer is incomplete");
        return false;
    }
    return true;
}

void StaticShadowCacheGpu::destroyMesh (MeshGpu *mesh)
{
    if (!mesh) return;
    if (mesh->vao != 0) GL30.glDeleteVertexArrays(1, &mesh->vao);
    if (mesh->vertex_buffer != 0) GL15.glDeleteBuffers(1, &mesh->vertex_buffer);
    if (mesh->index_buffer != 0) GL15.glDeleteBuffers(1, &mesh->index_buffer);
    *mesh = {};
}

void StaticShadowCacheGpu::clearMeshes ()
{
    for (MeshGpu& mesh : meshes_) destroyMesh(&mesh);
    meshes_.clear();
}

bool StaticShadowCacheGpu::createMesh (
        std::uint32_t handle,
        MeshGpu *mesh,
        std::string *error)
{
    if (!mesh)
    {
        setError(error, "null static shadow mesh destination");
        return false;
    }
    const Mesh::MeshData *source = Mesh::loadedMesh(handle);
    if (!source || source->vertices.empty() || source->indices.empty())
    {
        setError(error, "static shadow mesh is unavailable");
        return false;
    }

    mesh->handle = handle;
    GL30.glGenVertexArrays(1, &mesh->vao);
    GL15.glGenBuffers(1, &mesh->vertex_buffer);
    GL15.glGenBuffers(1, &mesh->index_buffer);
    if (mesh->vao == 0 || mesh->vertex_buffer == 0 || mesh->index_buffer == 0)
    {
        setError(error, "failed to allocate static shadow mesh");
        destroyMesh(mesh);
        return false;
    }

    GL30.glBindVertexArray(mesh->vao);
    GL15.glBindBuffer(GL_ARRAY_BUFFER, mesh->vertex_buffer);
    GL15.glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<LWCGLsizeiptr>(source->vertices.size() * sizeof(Mesh::Vertex)),
        source->vertices.data(),
        GL_STATIC_DRAW
    );
    GL15.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->index_buffer);
    GL15.glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        static_cast<LWCGLsizeiptr>(source->indices.size() * sizeof(std::uint32_t)),
        source->indices.data(),
        GL_STATIC_DRAW
    );
    GL20.glEnableVertexAttribArray(0);
    GL20.glVertexAttribPointer(
        0, 3, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(sizeof(Mesh::Vertex)),
        reinterpret_cast<const void*>(offsetof(Mesh::Vertex, position))
    );
    GL20.glEnableVertexAttribArray(1);
    GL20.glVertexAttribPointer(
        1, 2, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(sizeof(Mesh::Vertex)),
        reinterpret_cast<const void*>(offsetof(Mesh::Vertex, uv))
    );
    GL30.glBindVertexArray(0);
    GL15.glBindBuffer(GL_ARRAY_BUFFER, 0);
    GL15.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    mesh->index_count = static_cast<GLsizei>(source->indices.size());
    return true;
}

StaticShadowCacheGpu::MeshGpu *StaticShadowCacheGpu::meshFor (
        std::uint32_t handle,
        std::string *error)
{
    if (mesh_revision_ != Mesh::loadedMeshRevision())
    {
        clearMeshes();
        mesh_revision_ = Mesh::loadedMeshRevision();
    }
    for (MeshGpu& mesh : meshes_)
        if (mesh.handle == handle) return &mesh;

    MeshGpu mesh;
    if (!createMesh(handle, &mesh, error)) return nullptr;
    meshes_.push_back(mesh);
    return &meshes_.back();
}

bool StaticShadowCacheGpu::buildLightMatrix (
        const Ecs::World& world,
        const Ecs::TransformComponent& light_transform,
        std::string *error)
{
    const float largest = std::numeric_limits<float>::max();
    Math::Vec3 world_min = {largest, largest, largest};
    Math::Vec3 world_max = {-largest, -largest, -largest};
    bool found = false;

    for (const Ecs::Entity entity : world.entities())
    {
        const Ecs::TransformComponent *transform = world.getTransform(entity);
        const Ecs::MeshComponent *mesh = world.getMesh(entity);
        const Ecs::RenderableComponent *renderable = world.getRenderable(entity);
        const Ecs::MaterialComponent *material = world.getMaterial(entity);
        if (!transform || !mesh || !renderable || !renderable->visible || !material
                || mesh->loaded_mesh == Ecs::INVALID_ASSET_HANDLE)
            continue;

        const Material::Resource *resource =
            material->renderer_material != Ecs::INVALID_ASSET_HANDLE
            ? Material::get(material->renderer_material) : nullptr;
        if (resource && (resource->render_class == Material::RenderClass::Transparent
                || resource->render_class == Material::RenderClass::Transmissive))
            continue;

        const Mesh::MeshData *source = Mesh::loadedMesh(mesh->loaded_mesh);
        if (!source) continue;
        const Math::Mat4 model = Math::transform(
            toVec3(transform->position), toVec3(transform->rotation), toVec3(transform->scale)
        );
        for (int z = 0; z < 2; ++z)
            for (int y = 0; y < 2; ++y)
                for (int x = 0; x < 2; ++x)
                {
                    const Math::Vec3 local = {
                        x ? source->bounds.maximum.x : source->bounds.minimum.x,
                        y ? source->bounds.maximum.y : source->bounds.minimum.y,
                        z ? source->bounds.maximum.z : source->bounds.minimum.z,
                    };
                    const Math::Vec3 p = Math::transformPoint(model, local);
                    world_min.x = std::min(world_min.x, p.x);
                    world_min.y = std::min(world_min.y, p.y);
                    world_min.z = std::min(world_min.z, p.z);
                    world_max.x = std::max(world_max.x, p.x);
                    world_max.y = std::max(world_max.y, p.y);
                    world_max.z = std::max(world_max.z, p.z);
                    found = true;
                }
    }

    if (!found)
    {
        setError(error, "no imported opaque geometry for static shadow cache");
        return false;
    }

    const Math::Vec3 center = Math::multiply(Math::add(world_min, world_max), 0.5f);
    const Math::Vec3 rotation = toVec3(light_transform.rotation);
    const Math::Vec3 forward = Math::normalize(
        Math::transformDirection(Math::rotationEuler(rotation), {0.0f, 0.0f, -1.0f})
    );
    const Math::Vec3 up_hint = std::fabs(Math::dot(forward, {0.0f, 1.0f, 0.0f})) > 0.98f
        ? Math::Vec3{1.0f, 0.0f, 0.0f}
        : Math::Vec3{0.0f, 1.0f, 0.0f};
    const float radius = std::max(1.0f, Math::length(Math::subtract(world_max, world_min)) * 0.5f);
    const Math::Vec3 eye = Math::subtract(center, Math::multiply(forward, radius * 2.0f + 1.0f));
    const Math::Mat4 view = Math::lookAt(eye, center, up_hint);

    Math::Vec3 view_min = {largest, largest, largest};
    Math::Vec3 view_max = {-largest, -largest, -largest};
    for (int z = 0; z < 2; ++z)
        for (int y = 0; y < 2; ++y)
            for (int x = 0; x < 2; ++x)
            {
                const Math::Vec3 p = {
                    x ? world_max.x : world_min.x,
                    y ? world_max.y : world_min.y,
                    z ? world_max.z : world_min.z,
                };
                const Math::Vec3 q = Math::transformPoint(view, p);
                view_min.x = std::min(view_min.x, q.x);
                view_min.y = std::min(view_min.y, q.y);
                view_min.z = std::min(view_min.z, q.z);
                view_max.x = std::max(view_max.x, q.x);
                view_max.y = std::max(view_max.y, q.y);
                view_max.z = std::max(view_max.z, q.z);
            }
    expandBounds(&view_min, &view_max);
    const Math::Mat4 projection = orthographicForViewBounds(view_min, view_max);
    light_view_projection_ = Math::multiply(projection, view);
    return true;
}

bool StaticShadowCacheGpu::bindMaterial (
        const Ecs::MaterialComponent& material,
        std::string *error)
{
    const Material::Resource *resource =
        material.renderer_material != Ecs::INVALID_ASSET_HANDLE
        ? Material::get(material.renderer_material) : nullptr;
    const bool masked = resource && resource->render_class == Material::RenderClass::Masked;
    GL20.glUniform1i(masked_location_, masked ? 1 : 0);
    GL20.glUniform1f(opacity_location_, resource ? resource->opacity : material.opacity);
    GL20.glUniform1f(alpha_cutoff_location_, resource ? resource->alpha_cutoff : 0.5f);

    if (!masked || material.renderer_material == Ecs::INVALID_ASSET_HANDLE)
    {
        GL20.glUniform1i(has_opacity_texture_location_, 0);
        return true;
    }

    if (!material_gpu_.ensure(material.renderer_material, error)
            || !material_gpu_.bind(material.renderer_material, 0, error))
        return false;

    const Material::TextureBinding& opacity =
        resource->textures[Material::slotIndex(Material::Slot::Opacity)];
    const bool has_texture = opacity.texture != Models::INVALID_TEXTURE;
    GL20.glUniform1i(has_opacity_texture_location_, has_texture ? 1 : 0);
    GL20.glUniform4f(
        opacity_scale_offset_location_,
        opacity.scale.x, opacity.scale.y,
        opacity.offset.x + opacity.turbulence.x,
        opacity.offset.y + opacity.turbulence.y
    );
    GL20.glUniform1i(opacity_channel_location_, channelCode(opacity.channel));
    return true;
}

bool StaticShadowCacheGpu::drawScene (
        const Ecs::World& world,
        std::string *error)
{
    GL30.glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
    glViewport(0, 0, SIZE, SIZE);
    glClearDepth(1.0);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    GL20.glUseProgram(program_);
    GL20.glUniformMatrix4fv(
        light_view_projection_location_, 1, GL_FALSE, light_view_projection_.value
    );

    for (const Ecs::Entity entity : world.entities())
    {
        const Ecs::TransformComponent *transform = world.getTransform(entity);
        const Ecs::MeshComponent *mesh = world.getMesh(entity);
        const Ecs::RenderableComponent *renderable = world.getRenderable(entity);
        const Ecs::MaterialComponent *material = world.getMaterial(entity);
        if (!transform || !mesh || !renderable || !renderable->visible || !material
                || mesh->loaded_mesh == Ecs::INVALID_ASSET_HANDLE)
            continue;

        const Material::Resource *resource =
            material->renderer_material != Ecs::INVALID_ASSET_HANDLE
            ? Material::get(material->renderer_material) : nullptr;
        if (resource && (resource->render_class == Material::RenderClass::Transparent
                || resource->render_class == Material::RenderClass::Transmissive))
            continue;

        MeshGpu *gpu = meshFor(mesh->loaded_mesh, error);
        if (!gpu || !bindMaterial(*material, error))
        {
            GL30.glBindVertexArray(0);
            GL20.glUseProgram(0);
            GL30.glBindFramebuffer(GL_FRAMEBUFFER, 0);
            return false;
        }

        const Math::Mat4 model = Math::transform(
            toVec3(transform->position), toVec3(transform->rotation), toVec3(transform->scale)
        );
        GL20.glUniformMatrix4fv(model_location_, 1, GL_FALSE, model.value);
        GL30.glBindVertexArray(gpu->vao);
        glDrawElements(GL_TRIANGLES, gpu->index_count, GL_UNSIGNED_INT, nullptr);
    }

    GL30.glBindVertexArray(0);
    GL20.glUseProgram(0);
    GL30.glBindFramebuffer(GL_FRAMEBUFFER, 0);
    GLModern.glActiveTexture(GL_TEXTURE0);
    if (error) error->clear();
    return true;
}

void StaticShadowCacheGpu::disable ()
{
    enabled_ = false;
    light_entity_ = Ecs::INVALID_ENTITY;
}

bool StaticShadowCacheGpu::ensure (
        const Ecs::World& world,
        const TriangleScene& triangles,
        const RevisionState& revisions,
        std::string *error)
{
    if (validFor(revisions))
    {
        if (error) error->clear();
        return true;
    }

    disable();
    if (!triangles.ready())
    {
        revisions_ = revisions;
        valid_ = true;
        if (error) error->clear();
        return true;
    }

    const Ecs::TransformComponent *light_transform = nullptr;
    for (const Ecs::Entity entity : world.entities())
    {
        const Ecs::LightComponent *light = world.getLight(entity);
        const Ecs::TransformComponent *transform = world.getTransform(entity);
        if (light && transform && light->casts_shadows
                && light->intensity > 0.0f
                && light->type == Ecs::LightType::Directional)
        {
            light_entity_ = entity;
            light_transform = transform;
            break;
        }
    }

    if (!light_transform)
    {
        revisions_ = revisions;
        valid_ = true;
        if (error) error->clear();
        return true;
    }

    for (const Ecs::Entity entity : world.entities())
    {
        const Ecs::RenderableComponent *renderable = world.getRenderable(entity);
        const Ecs::MaterialComponent *material = world.getMaterial(entity);
        const Ecs::MeshComponent *mesh = world.getMesh(entity);
        if (!renderable || !renderable->visible || !material || !mesh
                || mesh->loaded_mesh == Ecs::INVALID_ASSET_HANDLE)
            continue;
        const Material::Resource *resource =
            material->renderer_material != Ecs::INVALID_ASSET_HANDLE
            ? Material::get(material->renderer_material) : nullptr;
        if (resource && (resource->render_class == Material::RenderClass::Transparent
                || resource->render_class == Material::RenderClass::Transmissive))
        {
            revisions_ = revisions;
            valid_ = true;
            disable();
            if (error) error->clear();
            return true;
        }
    }

    if (!buildLightMatrix(world, *light_transform, error)
            || !drawScene(world, error))
    {
        disable();
        valid_ = false;
        return false;
    }

    revisions_ = revisions;
    valid_ = true;
    enabled_ = true;
    if (error) error->clear();
    return true;
}

bool StaticShadowCacheGpu::validFor (const RevisionState& revisions) const
{
    return valid_ && staticShadowValid(revisions_, revisions);
}

bool StaticShadowCacheGpu::bind (
        GLuint direct_program,
        int light_index,
        std::string *error)
{
    if (direct_program == 0)
    {
        setError(error, "invalid direct-light program for static shadow cache");
        return false;
    }
    if (direct_program_ != direct_program)
    {
        direct_program_ = direct_program;
        direct_enabled_location_ =
            GL20.glGetUniformLocation(direct_program, "uStaticShadowEnabled");
        direct_light_index_location_ =
            GL20.glGetUniformLocation(direct_program, "uStaticShadowLightIndex");
        direct_matrix_location_ =
            GL20.glGetUniformLocation(direct_program, "uStaticShadowViewProjection");
    }
    if (direct_enabled_location_ < 0 || direct_light_index_location_ < 0
            || direct_matrix_location_ < 0)
    {
        setError(error, "static shadow direct-light uniforms are unavailable");
        return false;
    }

    GL20.glUseProgram(direct_program);
    const bool active = enabled_ && light_index >= 0;
    GL20.glUniform1i(direct_enabled_location_, active ? 1 : 0);
    GL20.glUniform1i(direct_light_index_location_, active ? light_index : -1);
    GL20.glUniformMatrix4fv(
        direct_matrix_location_, 1, GL_FALSE, light_view_projection_.value
    );
    GL20.glUseProgram(0);

    GLModern.glActiveTexture(GL_TEXTURE0 + 6);
    glBindTexture(GL_TEXTURE_2D, active ? depth_texture_ : 0);
    GLModern.glActiveTexture(GL_TEXTURE0);
    if (error) error->clear();
    return true;
}

void StaticShadowCacheGpu::shutdown ()
{
    clearMeshes();
    material_gpu_.shutdown();
    if (depth_texture_ != 0) glDeleteTextures(depth_texture_);
    if (framebuffer_ != 0) GL30.glDeleteFramebuffers(1, &framebuffer_);
    destroyProgram(&program_);
    depth_texture_ = 0;
    framebuffer_ = 0;
    model_location_ = light_view_projection_location_ = -1;
    masked_location_ = has_opacity_texture_location_ = -1;
    opacity_location_ = alpha_cutoff_location_ = -1;
    opacity_scale_offset_location_ = opacity_channel_location_ = -1;
    opacity_sampler_location_ = -1;
    direct_program_ = 0;
    direct_enabled_location_ = direct_light_index_location_ = direct_matrix_location_ = -1;
    revisions_ = {};
    light_view_projection_ = Math::identity();
    light_entity_ = Ecs::INVALID_ENTITY;
    mesh_revision_ = 0u;
    enabled_ = false;
    valid_ = false;
}

} // namespace Gpu
} // namespace Renderer
