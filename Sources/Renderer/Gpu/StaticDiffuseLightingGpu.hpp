#ifndef CRAPGAME_RENDERER_GPU_STATICDIFFUSELIGHTINGGPU_HPP
#define CRAPGAME_RENDERER_GPU_STATICDIFFUSELIGHTINGGPU_HPP

#include "Renderer/Gpu/RevisionState.hpp"

#include <lwcgl/lwcgl.h>
#include <lwcgl/glmodern.h>

#include <string>

namespace Renderer
{
namespace Gpu
{

class GBufferGpu;
class StaticShadowCacheGpu;

class StaticDiffuseLightingGpu
{
public:
    ~StaticDiffuseLightingGpu() { shutdown(); }
    StaticDiffuseLightingGpu() = default;
    StaticDiffuseLightingGpu(const StaticDiffuseLightingGpu&) = delete;
    StaticDiffuseLightingGpu& operator=(const StaticDiffuseLightingGpu&) = delete;

    bool init(std::string *error=nullptr);
    bool resize(int width,int height,std::string *error=nullptr);
    void setLightSource(GLuint light_buffer,int static_light_index);
    bool updateIfNeeded(const GBufferGpu& gbuffer,
                        const StaticShadowCacheGpu& shadow,
                        const RevisionState& revisions,
                        std::string *error=nullptr);
    bool validFor(const RevisionState& revisions) const;
    void invalidate();
    void shutdown();

    GLuint texture() const { return texture_; }
    bool ready() const { return program_!=0&&texture_!=0; }

private:
    GLuint program_=0;
    GLuint texture_=0;
    GLuint light_buffer_=0;
    GLint static_light_index_location_=-1;
    GLint shadow_enabled_location_=-1;
    GLint shadow_matrix_location_=-1;
    GLint inverse_view_projection_location_=-1;
    RevisionState revisions_={};
    int static_light_index_=-1;
    int width_=0,height_=0;
    bool valid_=false;
};

} // namespace Gpu
} // namespace Renderer

#endif
