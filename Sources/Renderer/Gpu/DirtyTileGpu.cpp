#include "Renderer/Gpu/DirtyTileGpu.hpp"

#include "Renderer/Gpu/DirtyTileShader.hpp"
#include "Renderer/Gpu/Gpu.hpp"
#include "Renderer/Gpu/ResourceLifecycle.hpp"

#include <cstddef>

#ifndef GL_DISPATCH_INDIRECT_BUFFER
#define GL_DISPATCH_INDIRECT_BUFFER 0x90EE
#endif
#ifndef GL_COMMAND_BARRIER_BIT
#define GL_COMMAND_BARRIER_BIT 0x00000040
#endif
#ifndef GL_SHADER_STORAGE_BARRIER_BIT
#define GL_SHADER_STORAGE_BARRIER_BIT 0x00002000
#endif

namespace Renderer
{
namespace Gpu
{
namespace
{

struct TileCoordinate { std::uint32_t x,y; };

void setError(std::string *error,const char *message)
{
    if(error)*error=message?message:"GPU dirty tile error";
}

} // namespace

bool DirtyTileGpu::init(std::string *error)
{
    shutdown();
    program_=createComputeProgram(DIRTY_TILE_COMPACT_COMPUTE,error);
    if(program_==0)return false;

    slice_index_location_=GL20.glGetUniformLocation(program_,"uSliceIndex");
    slice_count_location_=GL20.glGetUniformLocation(program_,"uSliceCount");
    if(slice_index_location_<0||slice_count_location_<0)
    {
        setError(error,"GPU dirty tile slice uniforms are unavailable");
        shutdown();
        return false;
    }

    GL15.glGenBuffers(1,&tile_buffer_);
    GL15.glGenBuffers(1,&indirect_buffer_);
    if(tile_buffer_==0||indirect_buffer_==0)
    {
        setError(error,"failed to allocate GPU dirty tile buffers");
        shutdown();
        return false;
    }
    if(error)error->clear();
    return true;
}

bool DirtyTileGpu::ensure(int width,int height,std::string *error)
{
    if(program_==0||tile_buffer_==0||indirect_buffer_==0)
        if(!init(error))return false;
    return resize(width,height,error);
}

bool DirtyTileGpu::resize(int width,int height,std::string *error)
{
    const int target_width=normalizedExtent(width);
    const int target_height=normalizedExtent(height);
    if(width_==target_width&&height_==target_height&&total_tiles_!=0u)return true;

    width_=target_width;height_=target_height;
    tiles_x_=DirtyTilePolicy::tileCount(width_);
    tiles_y_=DirtyTilePolicy::tileCount(height_);
    total_tiles_=static_cast<std::uint32_t>(tiles_x_*tiles_y_);

    GL15.glBindBuffer(GL_SHADER_STORAGE_BUFFER,tile_buffer_);
    GL15.glBufferData(GL_SHADER_STORAGE_BUFFER,
        static_cast<LWCGLsizeiptr>(static_cast<std::size_t>(total_tiles_)*sizeof(TileCoordinate)),
        nullptr,GL_DYNAMIC_DRAW);
    GL15.glBindBuffer(GL_SHADER_STORAGE_BUFFER,indirect_buffer_);
    const std::uint32_t args[3]={0u,1u,1u};
    GL15.glBufferData(GL_SHADER_STORAGE_BUFFER,sizeof(args),args,GL_DYNAMIC_DRAW);
    GL15.glBindBuffer(GL_SHADER_STORAGE_BUFFER,0);
    if(error)error->clear();
    return true;
}

bool DirtyTileGpu::compact(GLuint valid_mask,
                           std::uint32_t slice_index,
                           std::uint32_t slice_count,
                           std::string *error)
{
    if(!ready()||valid_mask==0)
    {
        setError(error,"GPU dirty tile resources are not ready");
        return false;
    }

    const std::uint32_t count=slice_count==0u?1u:slice_count;
    const std::uint32_t index=slice_index%count;
    const std::uint32_t args[3]={0u,1u,1u};
    GL15.glBindBuffer(GL_SHADER_STORAGE_BUFFER,indirect_buffer_);
    GL15.glBufferSubData(GL_SHADER_STORAGE_BUFFER,0,sizeof(args),args);
    GL15.glBindBuffer(GL_SHADER_STORAGE_BUFFER,0);

    GLModern.glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,valid_mask);
    GL20.glUseProgram(program_);
    GL20.glUniform1i(slice_index_location_,static_cast<GLint>(index));
    GL20.glUniform1i(slice_count_location_,static_cast<GLint>(count));
    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER,0,tile_buffer_);
    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER,1,indirect_buffer_);
    GL43.glDispatchCompute(static_cast<GLuint>(tiles_x_),static_cast<GLuint>(tiles_y_),1);
    GL42.glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT|GL_COMMAND_BARRIER_BIT);
    GL20.glUseProgram(0);
    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER,0,0);
    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER,1,0);
    GL15.glBindBuffer(GL_SHADER_STORAGE_BUFFER,0);
    glBindTexture(GL_TEXTURE_2D,0);
    if(error)error->clear();
    return true;
}

void DirtyTileGpu::bindTiles(GLuint binding) const
{
    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER,binding,tile_buffer_);
}

void DirtyTileGpu::dispatchIndirect() const
{
    GL15.glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER,indirect_buffer_);
    GL43.glDispatchComputeIndirect(0);
    GL15.glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER,0);
}

bool DirtyTileGpu::ready() const
{
    return program_!=0&&tile_buffer_!=0&&indirect_buffer_!=0&&total_tiles_!=0u
        &&slice_index_location_>=0&&slice_count_location_>=0;
}

void DirtyTileGpu::shutdown()
{
    if(indirect_buffer_!=0)GL15.glDeleteBuffers(1,&indirect_buffer_);
    if(tile_buffer_!=0)GL15.glDeleteBuffers(1,&tile_buffer_);
    indirect_buffer_=0;tile_buffer_=0;
    destroyProgram(&program_);
    slice_index_location_=-1;slice_count_location_=-1;
    width_=0;height_=0;tiles_x_=0;tiles_y_=0;total_tiles_=0u;
}

} // namespace Gpu
} // namespace Renderer
