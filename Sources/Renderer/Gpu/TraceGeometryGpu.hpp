#ifndef CRAPGAME_RENDERER_GPU_TRACEGEOMETRYGPU_HPP
#define CRAPGAME_RENDERER_GPU_TRACEGEOMETRYGPU_HPP

#include "Renderer/Gpu/ShadowTriangleGpu.hpp"

#include <lwcgl/lwcgl.h>
#include <lwcgl/glmodern.h>

#include <cstddef>
#include <cstdint>
#include <string>

namespace Renderer
{
namespace Gpu
{

class TraceGeometryGpu
{
public:
    ~TraceGeometryGpu(){shutdown();}
    TraceGeometryGpu()=default;
    TraceGeometryGpu(const TraceGeometryGpu&)=delete;
    TraceGeometryGpu& operator=(const TraceGeometryGpu&)=delete;

    bool ensure(GLuint source_triangle_buffer,
                std::size_t triangle_count,
                std::uint64_t mesh_revision,
                std::string *error=nullptr);
    void shutdown();

    GLuint shadowTriangleBuffer() const { return shadow_triangle_buffer_; }
    std::size_t shadowTriangleCount() const { return triangle_count_; }
    bool ready() const { return program_!=0&&shadow_triangle_buffer_!=0; }

private:
    bool allocate(std::size_t triangle_count,std::string *error);

    GLuint program_=0;
    GLuint shadow_triangle_buffer_=0;
    GLint triangle_count_location_=-1;
    std::size_t shadow_triangle_capacity_=0u;
    std::size_t triangle_count_=0u;
    GLuint source_triangle_buffer_=0;
    std::uint64_t mesh_revision_=0u;
    bool valid_=false;
};

} // namespace Gpu
} // namespace Renderer

#endif
