#include "Renderer/Gpu/ReflectionCacheGpu.hpp"

#include "Renderer/Gpu/ReprojectionCacheGpu.hpp"

namespace Renderer
{
namespace Gpu
{
namespace
{

void setError(std::string *error,const char *message)
{
    if(error)*error=message?message:"GPU reflection cache error";
}

void bindTexture(GLuint unit,GLuint texture)
{
    GLModern.glActiveTexture(static_cast<GLenum>(GL_TEXTURE0+unit));
    glBindTexture(GL_TEXTURE_2D,texture);
}

} // namespace

bool ReflectionCacheGpu::bind(GLuint program,
                              const ReprojectionCacheGpu& reprojection,
                              GLuint previous_reflection,
                              bool history_allowed,
                              std::string *error)
{
    if(program==0||!reprojection.ready()||previous_reflection==0)
    {
        setError(error,"GPU reflection cache resources are not ready");
        return false;
    }
    if(program_!=program)
    {
        program_=program;
        previous_view_projection_location_=GL20.glGetUniformLocation(
            program_,"uReflectionPreviousViewProjection");
        history_valid_location_=GL20.glGetUniformLocation(
            program_,"uReflectionHistoryValid");
    }
    if(previous_view_projection_location_<0||history_valid_location_<0)
    {
        setError(error,"GPU reflection cache uniforms are unavailable");
        return false;
    }

    bindTexture(11,reprojection.previousPositionTexture());
    bindTexture(12,reprojection.previousNormalTexture());
    bindTexture(13,reprojection.previousMaterialTexture());
    GLModern.glActiveTexture(GL_TEXTURE0);

    GL20.glUseProgram(program_);
    GL20.glUniformMatrix4fv(previous_view_projection_location_,1,GL_FALSE,
                            reprojection.previousViewProjection().value);
    GL20.glUniform1i(history_valid_location_,
                     history_allowed&&reprojection.historyValid()?1:0);
    if(error)error->clear();
    return true;
}

void ReflectionCacheGpu::shutdown()
{
    program_=0;
    previous_view_projection_location_=-1;
    history_valid_location_=-1;
}

} // namespace Gpu
} // namespace Renderer
