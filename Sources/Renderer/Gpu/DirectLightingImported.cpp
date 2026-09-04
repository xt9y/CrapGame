#include "Renderer/Gpu/DirectLightingGpu.hpp"
#include "Renderer/Gpu/TriangleScene.hpp"

#include "Renderer/Material/Material.hpp"
#include "Renderer/Mesh/Mesh.hpp"

namespace Renderer { namespace Gpu {

namespace
{
RevisionState staticShadowRevisions(std::uint64_t scene_revision)
{
    RevisionState revisions = {};
    revisions.geometry = scene_revision;
    revisions.material = scene_revision;
    revisions.lighting = scene_revision;
    revisions.mesh_registry = Mesh::loadedMeshRevision();
    revisions.material_registry = Material::revision();
    return revisions;
}
} // namespace

int DirectLightingGpu::staticShadowLightIndex() const
{
    if (!scene_world_ || !static_shadow_cache_.enabled() || !primitives_.empty()) return -1;
    int index = 0;
    for (const Ecs::Entity entity : scene_world_->entities())
    {
        const Ecs::TransformComponent *transform = scene_world_->getTransform(entity);
        const Ecs::LightComponent *light = scene_world_->getLight(entity);
        if (!transform || !light || light->intensity <= 0.0f) continue;
        if (entity == static_shadow_cache_.lightEntity()) return index;
        ++index;
    }
    return -1;
}

bool DirectLightingGpu::ensureStaticShadowCache(std::string *error)
{
    if (!scene_world_)
    {
        if (error) *error = "static shadow cache has no scene world";
        return false;
    }
    return static_shadow_cache_.ensure(
        *scene_world_,
        triangle_scene_,
        staticShadowRevisions(scene_revision_),
        error
    );
}

bool DirectLightingGpu::bindImportedScene(const TriangleScene& triangles,
                                          std::string *error)
{
    if (!triangles.ready())
    {
        if (error) *error = "imported triangle scene is not ready";
        return false;
    }
    if (!trace_geometry_.ensure(triangles.triangleBuffer(),triangles.triangleCount(),
                                Mesh::loadedMeshRevision(),error))
        return false;

    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER,0,triangles.triangleBuffer());
    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER,1,triangles.meshBuffer());
    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER,2,triangles.instanceBuffer());
    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER,3,triangles.blasNodeBuffer());
    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER,4,triangles.tlasNodeBuffer());
    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER,7,triangles.traceRecordBuffer());
    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER,8,trace_geometry_.shadowTriangleBuffer());
    GLModern.glActiveTexture(GL_TEXTURE0+4);
    glBindTexture(GL_TEXTURE_2D_ARRAY,triangles.colorAtlas());
    GLModern.glActiveTexture(GL_TEXTURE0+5);
    glBindTexture(GL_TEXTURE_2D_ARRAY,triangles.dataAtlas());
    GLModern.glActiveTexture(GL_TEXTURE0);

    if (!ensureStaticShadowCache(error)) return false;
    if (!static_shadow_cache_.bind(program_, staticShadowLightIndex(), error)) return false;

    GL20.glUseProgram(program_);
    const GLint instance_location = GL20.glGetUniformLocation(program_,"uImportedInstanceCount");
    const GLint tlas_location = GL20.glGetUniformLocation(program_,"uImportedTlasNodeCount");
    const GLint material_location = GL20.glGetUniformLocation(program_,"uTraceMaterialCount");
    if (instance_location < 0 || tlas_location < 0 || material_location < 0)
    {
        GL20.glUseProgram(0);
        if (error) *error = "imported direct-light uniforms are unavailable";
        return false;
    }
    GL20.glUniform1i(instance_location,static_cast<GLint>(triangles.instanceCount()));
    GL20.glUniform1i(tlas_location,static_cast<GLint>(triangles.tlasNodeCount()));
    GL20.glUniform1i(material_location,static_cast<GLint>(triangles.traceMaterialCount()));
    GL20.glUseProgram(0);
    if (error) error->clear();
    return true;
}

} }
