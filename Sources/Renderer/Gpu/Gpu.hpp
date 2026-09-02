#ifndef CRAPGAME_RENDERER_GPU_GPU_HPP
#define CRAPGAME_RENDERER_GPU_GPU_HPP

#include <lwcgl/lwcgl.h>
#include <lwcgl/glmodern.h>

#include <string>

namespace Renderer
{
namespace Gpu
{

bool available (std::string *reason = nullptr);

GLuint createGraphicsProgram (
                const char *vertex_source,
                const char *fragment_source,
                std::string *error = nullptr
        );

GLuint createComputeProgram (
                const char *compute_source,
                std::string *error = nullptr
        );

void destroyProgram (GLuint *program);

} // namespace Gpu
} // namespace Renderer

#endif
