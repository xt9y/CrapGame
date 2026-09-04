#include "Renderer/Gpu/RadianceCacheGpu.hpp"

#include <vector>

namespace Renderer
{
namespace Gpu
{
namespace
{

void setError(std::string *error,const char *message)
{
    if(error)*error=message?message:"GPU radiance cache error";
}

} // namespace

bool RadianceCacheGpu::resetStorage(std::string *error)
{
    if(buffer_==0)
    {
        setError(error,"radiance cache buffer is unavailable");
        return false;
    }
    const std::vector<RadianceCacheRecordGpu> zeros(
        static_cast<std::size_t>(RadianceCachePolicy::INITIAL_CAPACITY));
    GL15.glBindBuffer(GL_SHADER_STORAGE_BUFFER,buffer_);
    GL15.glBufferData(
        GL_SHADER_STORAGE_BUFFER,
        static_cast<LWCGLsizeiptr>(zeros.size()*sizeof(RadianceCacheRecordGpu)),
        zeros.data(),
        GL_DYNAMIC_DRAW);
    GL15.glBindBuffer(GL_SHADER_STORAGE_BUFFER,0);
    if(error)error->clear();
    return true;
}

bool RadianceCacheGpu::init(std::string *error)
{
    shutdown();
    GL15.glGenBuffers(1,&buffer_);
    if(buffer_==0)
    {
        setError(error,"failed to allocate GPU radiance cache buffer");
        return false;
    }
    generation_=1u;
    revisions_valid_=false;
    if(!resetStorage(error))
    {
        shutdown();
        return false;
    }
    if(error)error->clear();
    return true;
}

bool RadianceCacheGpu::ensure(std::string *error)
{
    if(buffer_==0)return init(error);
    if(error)error->clear();
    return true;
}

bool RadianceCacheGpu::updateGeneration(const RevisionState& revisions,std::string *error)
{
    if(!ensure(error))return false;
    if(!revisions_valid_)
    {
        revisions_=revisions;
        revisions_valid_=true;
        if(error)error->clear();
        return true;
    }
    if(RadianceCachePolicy::generationChanges(revisions_,revisions))
    {
        if(generation_>=0x7fffffffu)
        {
            generation_=1u;
            if(!resetStorage(error))return false;
        }
        else
        {
            ++generation_;
        }
    }
    revisions_=revisions;
    if(error)error->clear();
    return true;
}

bool RadianceCacheGpu::bind(GLuint binding,std::string *error) const
{
    if(!ready())
    {
        setError(error,"GPU radiance cache is not initialized");
        return false;
    }
    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER,binding,buffer_);
    if(error)error->clear();
    return true;
}

void RadianceCacheGpu::shutdown()
{
    if(buffer_!=0)GL15.glDeleteBuffers(1,&buffer_);
    buffer_=0;
    revisions_={};
    generation_=1u;
    revisions_valid_=false;
}

} // namespace Gpu
} // namespace Renderer
