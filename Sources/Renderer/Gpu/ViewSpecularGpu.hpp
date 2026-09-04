#ifndef CRAPGAME_RENDERER_GPU_VIEWSPECULARGPU_HPP
#define CRAPGAME_RENDERER_GPU_VIEWSPECULARGPU_HPP

#include "Renderer/Math/Math.hpp"

#include <lwcgl/lwcgl.h>
#include <lwcgl/glmodern.h>

#include <string>

namespace Renderer
{
namespace Gpu
{

class DirtyTileGpu;
class GBufferGpu;
class StaticShadowCacheGpu;

class ViewSpecularGpu
{
public:
    ~ViewSpecularGpu() { shutdown(); }
    ViewSpecularGpu() = default;
    ViewSpecularGpu(const ViewSpecularGpu&) = delete;
    ViewSpecularGpu& operator=(const ViewSpecularGpu&) = delete;

    bool init(std::string *error=nullptr);
    bool resize(int width,int height,std::string *error=nullptr);
    void setLightSource(GLuint light_buffer,int static_light_index);
    bool render(const GBufferGpu& gbuffer,
                const StaticShadowCacheGpu& shadow,
                const Math::Vec3& camera,
                const DirtyTileGpu *dirty_tiles=nullptr,
                std::string *error=nullptr);
    void shutdown();

    GLuint texture() const { return texture_; }
    bool ready() const { return program_!=0&&texture_!=0; }

private:
    GLuint program_=0;
    GLuint texture_=0;
    GLuint light_buffer_=0;
    GLint camera_location_=-1;
    GLint static_light_index_location_=-1;
    GLint shadow_enabled_location_=-1;
    GLint shadow_matrix_location_=-1;
    int static_light_index_=-1;
    int width_=0,height_=0;
};

} // namespace Gpu
} // namespace Renderer

#endif
