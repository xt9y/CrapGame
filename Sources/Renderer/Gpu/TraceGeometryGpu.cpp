#include "Renderer/Gpu/TraceGeometryGpu.hpp"

#include "Renderer/Gpu/Gpu.hpp"
#include "Renderer/Gpu/TraceGeometryShader.hpp"

#include <algorithm>

#ifndef GL_SHADER_STORAGE_BARRIER_BIT
#define GL_SHADER_STORAGE_BARRIER_BIT 0x00002000
#endif

namespace Renderer
{
namespace Gpu
{
namespace
{

void setError(std::string *error,const char *message)
{
    if(error)*error=message?message:"GPU trace geometry error";
}

} // namespace

bool TraceGeometryGpu::allocate(std::size_t triangle_count,std::string *error)
{
    if(shadow_triangle_buffer_==0)GL15.glGenBuffers(1,&shadow_triangle_buffer_);
    if(shadow_triangle_buffer_==0)
    {
        setError(error,"failed to allocate compact shadow triangle buffer");
        return false;
    }
    const std::size_t size=triangle_count*sizeof(ShadowTriangleGpu);
    const std::size_t required=std::max<std::size_t>(16u,size);
    if(required<=shadow_triangle_capacity_)return true;
    std::size_t capacity=std::max<std::size_t>(256u,shadow_triangle_capacity_);
    while(capacity<required)capacity*=2u;
    GL15.glBindBuffer(GL_SHADER_STORAGE_BUFFER,shadow_triangle_buffer_);
    GL15.glBufferData(GL_SHADER_STORAGE_BUFFER,
                      static_cast<LWCGLsizeiptr>(capacity),nullptr,GL_STATIC_DRAW);
    GL15.glBindBuffer(GL_SHADER_STORAGE_BUFFER,0);
    shadow_triangle_capacity_=capacity;
    return true;
}

bool TraceGeometryGpu::ensure(GLuint source_triangle_buffer,
                              std::size_t triangle_count,
                              std::uint64_t mesh_revision,
                              std::string *error)
{
    if(source_triangle_buffer==0)
    {
        setError(error,"full trace triangle buffer is unavailable");
        return false;
    }
    if(valid_&&source_triangle_buffer_==source_triangle_buffer
            &&triangle_count_==triangle_count&&mesh_revision_==mesh_revision)
    {
        if(error)error->clear();
        return true;
    }
    if(program_==0)
    {
        program_=createComputeProgram(TRACE_GEOMETRY_COMPACT_COMPUTE,error);
        if(program_==0)return false;
        triangle_count_location_=GL20.glGetUniformLocation(program_,"uTriangleCount");
        if(triangle_count_location_<0)
        {
            setError(error,"trace geometry compact uniform is unavailable");
            shutdown();
            return false;
        }
    }
    if(!allocate(triangle_count,error))return false;

    GL20.glUseProgram(program_);
    GL20.glUniform1i(triangle_count_location_,static_cast<GLint>(triangle_count));
    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER,0,source_triangle_buffer);
    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER,1,shadow_triangle_buffer_);
    if(triangle_count!=0u)
        GL43.glDispatchCompute(static_cast<GLuint>((triangle_count+63u)/64u),1,1);
    GL42.glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER,0,0);
    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER,1,0);
    GL20.glUseProgram(0);

    source_triangle_buffer_=source_triangle_buffer;
    triangle_count_=triangle_count;
    mesh_revision_=mesh_revision;
    valid_=true;
    if(error)error->clear();
    return true;
}

void TraceGeometryGpu::shutdown()
{
    if(shadow_triangle_buffer_!=0)GL15.glDeleteBuffers(1,&shadow_triangle_buffer_);
    shadow_triangle_buffer_=0;
    destroyProgram(&program_);
    triangle_count_location_=-1;
    shadow_triangle_capacity_=0u;
    triangle_count_=0u;
    source_triangle_buffer_=0;
    mesh_revision_=0u;
    valid_=false;
}

} // namespace Gpu
} // namespace Renderer
