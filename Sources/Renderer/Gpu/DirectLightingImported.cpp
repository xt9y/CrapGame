#include "Renderer/Gpu/DirectLightingGpu.hpp"
#include "Renderer/Gpu/TriangleScene.hpp"

namespace Renderer { namespace Gpu {

bool DirectLightingGpu::bindImportedScene(const TriangleScene& triangles,
                                          std::string *error)
{
    if (!triangles.ready())
    {
        if (error) *error = "imported triangle scene is not ready";
        return false;
    }
    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER,0,triangles.triangleBuffer());
    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER,1,triangles.meshBuffer());
    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER,2,triangles.instanceBuffer());
    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER,3,triangles.blasNodeBuffer());
    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER,4,triangles.tlasNodeBuffer());
    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER,7,triangles.traceRecordBuffer());
    GLModern.glActiveTexture(GL_TEXTURE0+4);
    glBindTexture(GL_TEXTURE_2D_ARRAY,triangles.colorAtlas());
    GLModern.glActiveTexture(GL_TEXTURE0+5);
    glBindTexture(GL_TEXTURE_2D_ARRAY,triangles.dataAtlas());
    GLModern.glActiveTexture(GL_TEXTURE0);

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
