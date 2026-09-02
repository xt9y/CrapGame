#include "GBufferGpu.hpp"

#include "Renderer/Gpu/Gpu.hpp"
#include "Renderer/Mesh/Mesh.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace Renderer
{
namespace Gpu
{
namespace
{

constexpr const char *GBUFFER_VERTEX_SHADER = R"GLSL(
#version 430 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUv;

struct InstanceData
{
    mat4 model;
    mat4 normalMatrix;
    vec4 albedoMetallic;
    vec4 emissiveRoughness;
};

layout(std430, binding = 0) readonly buffer InstanceBuffer
{
    InstanceData instances[];
};

uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vWorldPosition;
out vec3 vWorldNormal;
out vec3 vAlbedo;
out vec3 vEmissive;
out float vMetallic;
out float vRoughness;

void main ()
{
    InstanceData instance = instances[gl_InstanceID];

    vec4 world = instance.model * vec4(aPosition, 1.0);
    vWorldPosition = world.xyz;
    vWorldNormal = normalize(mat3(instance.normalMatrix) * aNormal);
    vAlbedo = instance.albedoMetallic.xyz;
    vMetallic = instance.albedoMetallic.w;
    vEmissive = instance.emissiveRoughness.xyz;
    vRoughness = instance.emissiveRoughness.w;

    gl_Position = uProjection * uView * world;
}
)GLSL";

constexpr const char *GBUFFER_FRAGMENT_SHADER = R"GLSL(
#version 430 core

in vec3 vWorldPosition;
in vec3 vWorldNormal;
in vec3 vAlbedo;
in vec3 vEmissive;
in float vMetallic;
in float vRoughness;

layout(location = 0) out vec4 oPositionDepth;
layout(location = 1) out vec4 oNormalRoughness;
layout(location = 2) out vec4 oAlbedoMetallic;
layout(location = 3) out vec4 oEmissive;

void main ()
{
    oPositionDepth = vec4(vWorldPosition, gl_FragCoord.z);
    oNormalRoughness = vec4(normalize(vWorldNormal), clamp(vRoughness, 0.04, 1.0));
    oAlbedoMetallic = vec4(max(vAlbedo, vec3(0.0)), clamp(vMetallic, 0.0, 1.0));
    oEmissive = vec4(max(vEmissive, vec3(0.0)), 1.0);
}
)GLSL";

Math::Vec3 toVec3 (const Ecs::Vec3& value)
{
    return {value.x, value.y, value.z};
}

float safeInverse (float value)
{
    if (std::fabs(value) < 0.00001f)
    {
        return value < 0.0f ? -100000.0f : 100000.0f;
    }

    return 1.0f / value;
}

GLuint createColorTexture (int width, int height)
{
    const GLuint texture = lwcgl_glGenTexture();

    if (texture == 0)
    {
        return 0;
    }

    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
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

GLuint createDepthTexture (int width, int height)
{
    const GLuint texture = lwcgl_glGenTexture();

    if (texture == 0)
    {
        return 0;
    }

    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_DEPTH_COMPONENT24,
            width,
            height,
            0,
            GL_DEPTH_COMPONENT,
            GL_UNSIGNED_INT,
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
        *error = message ? message : "GPU GBuffer error";
    }
}

} // namespace

bool GBufferGpu::init (std::string *error)
{
    shutdown();

    program_ = createGraphicsProgram(
            GBUFFER_VERTEX_SHADER,
            GBUFFER_FRAGMENT_SHADER,
            error
        );

    if (program_ == 0)
    {
        return false;
    }

    view_location_ = GL20.glGetUniformLocation(program_, "uView");
    projection_location_ = GL20.glGetUniformLocation(program_, "uProjection");

    if (view_location_ < 0 || projection_location_ < 0)
    {
        setError(error, "GPU GBuffer camera uniforms are unavailable");
        shutdown();
        return false;
    }

    if (!createMesh(Ecs::MeshType::Cube, &cubes_.mesh, error)
            || !createMesh(Ecs::MeshType::Plane, &planes_.mesh, error))
    {
        shutdown();
        return false;
    }

    GL15.glGenBuffers(1, &cubes_.instance_buffer);
    GL15.glGenBuffers(1, &planes_.instance_buffer);

    if (cubes_.instance_buffer == 0 || planes_.instance_buffer == 0)
    {
        setError(error, "failed to allocate GPU GBuffer instance buffers");
        shutdown();
        return false;
    }

    if (error)
    {
        error->clear();
    }

    return true;
}

bool GBufferGpu::resize (int width, int height, std::string *error)
{
    const int new_width = std::max(1, width);
    const int new_height = std::max(1, height);

    if (new_width == width_
            && new_height == height_
            && framebuffer_ != 0)
    {
        return true;
    }

    width_ = new_width;
    height_ = new_height;

    destroyAttachments();
    return createAttachments(error);
}

bool GBufferGpu::createMesh (
                Ecs::MeshType type,
                MeshGpu *mesh,
                std::string *error
        )
{
    if (!mesh)
    {
        setError(error, "null GPU mesh destination");
        return false;
    }

    const Mesh::MeshData& source = Mesh::meshForType(type);

    if (source.vertices.empty() || source.indices.empty())
    {
        setError(error, "cannot upload an empty renderer mesh");
        return false;
    }

    GL30.glGenVertexArrays(1, &mesh->vao);
    GL15.glGenBuffers(1, &mesh->vertex_buffer);
    GL15.glGenBuffers(1, &mesh->index_buffer);

    if (mesh->vao == 0 || mesh->vertex_buffer == 0 || mesh->index_buffer == 0)
    {
        setError(error, "failed to allocate persistent GPU mesh resources");
        destroyMesh(mesh);
        return false;
    }

    GL30.glBindVertexArray(mesh->vao);

    GL15.glBindBuffer(GL_ARRAY_BUFFER, mesh->vertex_buffer);
    GL15.glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<LWCGLsizeiptr>(source.vertices.size() * sizeof(Mesh::Vertex)),
            source.vertices.data(),
            GL_STATIC_DRAW
        );

    GL15.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->index_buffer);
    GL15.glBufferData(
            GL_ELEMENT_ARRAY_BUFFER,
            static_cast<LWCGLsizeiptr>(source.indices.size() * sizeof(std::uint32_t)),
            source.indices.data(),
            GL_STATIC_DRAW
        );

    GL20.glEnableVertexAttribArray(0);
    GL20.glVertexAttribPointer(
            0,
            3,
            GL_FLOAT,
            GL_FALSE,
            static_cast<GLsizei>(sizeof(Mesh::Vertex)),
            reinterpret_cast<const void *>(offsetof(Mesh::Vertex, position))
        );

    GL20.glEnableVertexAttribArray(1);
    GL20.glVertexAttribPointer(
            1,
            3,
            GL_FLOAT,
            GL_FALSE,
            static_cast<GLsizei>(sizeof(Mesh::Vertex)),
            reinterpret_cast<const void *>(offsetof(Mesh::Vertex, normal))
        );

    GL20.glEnableVertexAttribArray(2);
    GL20.glVertexAttribPointer(
            2,
            2,
            GL_FLOAT,
            GL_FALSE,
            static_cast<GLsizei>(sizeof(Mesh::Vertex)),
            reinterpret_cast<const void *>(offsetof(Mesh::Vertex, uv))
        );

    GL30.glBindVertexArray(0);
    GL15.glBindBuffer(GL_ARRAY_BUFFER, 0);
    GL15.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    mesh->index_count = static_cast<GLsizei>(source.indices.size());
    return true;
}

bool GBufferGpu::uploadBatch (Batch *batch, std::string *error)
{
    if (!batch || batch->instance_buffer == 0)
    {
        setError(error, "invalid GPU GBuffer batch");
        return false;
    }

    if (batch->instances.empty())
    {
        return true;
    }

    const std::size_t required = batch->instances.size() * sizeof(InstanceGpu);

    GL15.glBindBuffer(GL_SHADER_STORAGE_BUFFER, batch->instance_buffer);

    if (required > batch->instance_capacity)
    {
        std::size_t capacity = std::max<std::size_t>(sizeof(InstanceGpu) * 16u, batch->instance_capacity);

        while (capacity < required)
        {
            capacity *= 2u;
        }

        GL15.glBufferData(
                GL_SHADER_STORAGE_BUFFER,
                static_cast<LWCGLsizeiptr>(capacity),
                nullptr,
                GL_DYNAMIC_DRAW
            );

        batch->instance_capacity = capacity;
    }

    GL15.glBufferSubData(
            GL_SHADER_STORAGE_BUFFER,
            0,
            static_cast<LWCGLsizeiptr>(required),
            batch->instances.data()
        );

    return true;
}

bool GBufferGpu::createAttachments (std::string *error)
{
    GL30.glGenFramebuffers(1, &framebuffer_);

    if (framebuffer_ == 0)
    {
        setError(error, "failed to allocate GPU GBuffer framebuffer");
        return false;
    }

    position_depth_ = createColorTexture(width_, height_);
    normal_roughness_ = createColorTexture(width_, height_);
    albedo_metallic_ = createColorTexture(width_, height_);
    emissive_ = createColorTexture(width_, height_);
    depth_ = createDepthTexture(width_, height_);

    if (position_depth_ == 0
            || normal_roughness_ == 0
            || albedo_metallic_ == 0
            || emissive_ == 0
            || depth_ == 0)
    {
        setError(error, "failed to allocate GPU GBuffer textures");
        destroyAttachments();
        return false;
    }

    GL30.glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
    GL30.glFramebufferTexture2D(
            GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D,
            position_depth_,
            0
        );
    GL30.glFramebufferTexture2D(
            GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT1,
            GL_TEXTURE_2D,
            normal_roughness_,
            0
        );
    GL30.glFramebufferTexture2D(
            GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT2,
            GL_TEXTURE_2D,
            albedo_metallic_,
            0
        );
    GL30.glFramebufferTexture2D(
            GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT3,
            GL_TEXTURE_2D,
            emissive_,
            0
        );
    GL30.glFramebufferTexture2D(
            GL_FRAMEBUFFER,
            GL_DEPTH_ATTACHMENT,
            GL_TEXTURE_2D,
            depth_,
            0
        );

    const GLenum outputs[] = {
        GL_COLOR_ATTACHMENT0,
        GL_COLOR_ATTACHMENT1,
        GL_COLOR_ATTACHMENT2,
        GL_COLOR_ATTACHMENT3,
    };

    GL20.glDrawBuffers(4, outputs);

    const GLenum status = GL30.glCheckFramebufferStatus(GL_FRAMEBUFFER);
    GL30.glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        char message[128];
        std::snprintf(
                message,
                sizeof(message),
                "GPU GBuffer framebuffer incomplete: 0x%04x",
                static_cast<unsigned int>(status)
            );
        setError(error, message);
        destroyAttachments();
        return false;
    }

    return true;
}

bool GBufferGpu::render (
                const Ecs::World& world,
                const Math::Mat4& view,
                const Math::Mat4& projection,
                std::string *error
        )
{
    if (!ready())
    {
        setError(error, "GPU GBuffer is not initialized or resized");
        return false;
    }

    cubes_.instances.clear();
    planes_.instances.clear();

    for (const Ecs::Entity entity : world.entities())
    {
        const Ecs::TransformComponent *transform = world.getTransform(entity);
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

        InstanceGpu instance = {};

        const Math::Vec3 position = toVec3(transform->position);
        const Math::Vec3 rotation = toVec3(transform->rotation);
        const Math::Vec3 scale = toVec3(transform->scale);

        const Math::Mat4 model = Math::transform(position, rotation, scale);
        const Math::Mat4 normal_matrix = Math::multiply(
                Math::rotationEuler(rotation),
                Math::scaling({
                    safeInverse(scale.x),
                    safeInverse(scale.y),
                    safeInverse(scale.z),
                })
            );

        std::memcpy(instance.model, model.value, sizeof(instance.model));
        std::memcpy(instance.normal_matrix, normal_matrix.value, sizeof(instance.normal_matrix));

        instance.albedo_metallic[0] = material->albedo.x;
        instance.albedo_metallic[1] = material->albedo.y;
        instance.albedo_metallic[2] = material->albedo.z;
        instance.albedo_metallic[3] = material->metallic;

        instance.emissive_roughness[0] = material->emissive.x * material->emissive_strength;
        instance.emissive_roughness[1] = material->emissive.y * material->emissive_strength;
        instance.emissive_roughness[2] = material->emissive.z * material->emissive_strength;
        instance.emissive_roughness[3] = material->roughness;

        if (mesh->mesh == Ecs::MeshType::Cube)
        {
            cubes_.instances.push_back(instance);
        }
        else
        {
            planes_.instances.push_back(instance);
        }
    }

    if (!uploadBatch(&cubes_, error) || !uploadBatch(&planes_, error))
    {
        return false;
    }

    GL30.glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
    glViewport(0, 0, width_, height_);

    const GLfloat zero[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    GL30.glClearBufferfv(GL_COLOR, 0, zero);
    GL30.glClearBufferfv(GL_COLOR, 1, zero);
    GL30.glClearBufferfv(GL_COLOR, 2, zero);
    GL30.glClearBufferfv(GL_COLOR, 3, zero);
    glClearDepth(1.0);
    glClear(GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    GL20.glUseProgram(program_);
    GL20.glUniformMatrix4fv(view_location_, 1, GL_FALSE, view.value);
    GL20.glUniformMatrix4fv(projection_location_, 1, GL_FALSE, projection.value);

    const auto draw_batch = [] (const Batch& batch)
    {
        if (batch.instances.empty())
        {
            return;
        }

        GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, batch.instance_buffer);
        GL30.glBindVertexArray(batch.mesh.vao);
        GL31.glDrawElementsInstanced(
                GL_TRIANGLES,
                batch.mesh.index_count,
                GL_UNSIGNED_INT,
                nullptr,
                static_cast<GLsizei>(batch.instances.size())
            );
    };

    draw_batch(cubes_);
    draw_batch(planes_);

    GL30.glBindVertexArray(0);
    GL20.glUseProgram(0);
    GL30.glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    GL42.glMemoryBarrier(GL_FRAMEBUFFER_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

    if (error)
    {
        error->clear();
    }

    return true;
}

void GBufferGpu::destroyAttachments ()
{
    deleteTexture(&position_depth_);
    deleteTexture(&normal_roughness_);
    deleteTexture(&albedo_metallic_);
    deleteTexture(&emissive_);
    deleteTexture(&depth_);

    if (framebuffer_ != 0)
    {
        GL30.glDeleteFramebuffers(1, &framebuffer_);
        framebuffer_ = 0;
    }
}

void GBufferGpu::destroyMesh (MeshGpu *mesh)
{
    if (!mesh)
    {
        return;
    }

    if (mesh->index_buffer != 0)
    {
        GL15.glDeleteBuffers(1, &mesh->index_buffer);
    }

    if (mesh->vertex_buffer != 0)
    {
        GL15.glDeleteBuffers(1, &mesh->vertex_buffer);
    }

    if (mesh->vao != 0)
    {
        GL30.glDeleteVertexArrays(1, &mesh->vao);
    }

    *mesh = {};
}

void GBufferGpu::shutdown ()
{
    destroyAttachments();

    if (cubes_.instance_buffer != 0)
    {
        GL15.glDeleteBuffers(1, &cubes_.instance_buffer);
    }

    if (planes_.instance_buffer != 0)
    {
        GL15.glDeleteBuffers(1, &planes_.instance_buffer);
    }

    cubes_.instance_buffer = 0;
    planes_.instance_buffer = 0;
    cubes_.instance_capacity = 0;
    planes_.instance_capacity = 0;
    cubes_.instances.clear();
    planes_.instances.clear();

    destroyMesh(&cubes_.mesh);
    destroyMesh(&planes_.mesh);
    destroyProgram(&program_);

    view_location_ = -1;
    projection_location_ = -1;
    width_ = 0;
    height_ = 0;
}

} // namespace Gpu
} // namespace Renderer
