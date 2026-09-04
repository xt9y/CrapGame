#ifndef CRAPGAME_RENDERER_GPU_REFLECTIONCACHEGPU_HPP
#define CRAPGAME_RENDERER_GPU_REFLECTIONCACHEGPU_HPP

#include <lwcgl/lwcgl.h>
#include <lwcgl/glmodern.h>

#include <string>

namespace Renderer
{
namespace Gpu
{

class ReprojectionCacheGpu;

class ReflectionCacheGpu
{
public:
    bool bind(GLuint program,
              const ReprojectionCacheGpu& reprojection,
              GLuint previous_reflection,
              bool history_allowed,
              std::string *error=nullptr);
    void shutdown();

private:
    GLuint program_=0;
    GLint previous_view_projection_location_=-1;
    GLint history_valid_location_=-1;
};

} // namespace Gpu
} // namespace Renderer

#endif
