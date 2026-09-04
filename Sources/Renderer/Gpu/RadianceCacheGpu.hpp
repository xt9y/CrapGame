#ifndef CRAPGAME_RENDERER_GPU_RADIANCECACHEGPU_HPP
#define CRAPGAME_RENDERER_GPU_RADIANCECACHEGPU_HPP

#include "Renderer/Gpu/RadianceCachePolicy.hpp"

#include <lwcgl/lwcgl.h>
#include <lwcgl/glmodern.h>

#include <cstdint>
#include <string>

namespace Renderer
{
namespace Gpu
{

class RadianceCacheGpu
{
public:
    ~RadianceCacheGpu(){shutdown();}
    RadianceCacheGpu()=default;
    RadianceCacheGpu(const RadianceCacheGpu&)=delete;
    RadianceCacheGpu& operator=(const RadianceCacheGpu&)=delete;

    bool init(std::string *error=nullptr);
    bool ensure(std::string *error=nullptr);
    bool updateGeneration(const RevisionState& revisions,std::string *error=nullptr);
    bool bind(GLuint binding=static_cast<GLuint>(RadianceCachePolicy::BUFFER_BINDING),
              std::string *error=nullptr) const;
    void shutdown();

    bool ready() const { return buffer_!=0; }
    GLuint buffer() const { return buffer_; }
    std::uint32_t generation() const { return generation_; }

private:
    bool resetStorage(std::string *error);

    GLuint buffer_=0;
    RevisionState revisions_={};
    std::uint32_t generation_=1u;
    bool revisions_valid_=false;
};

} // namespace Gpu
} // namespace Renderer

#endif
