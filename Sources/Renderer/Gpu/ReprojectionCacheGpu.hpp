#ifndef CRAPGAME_RENDERER_GPU_REPROJECTIONCACHEGPU_HPP
#define CRAPGAME_RENDERER_GPU_REPROJECTIONCACHEGPU_HPP

#include "Renderer/Math/Math.hpp"

#include <lwcgl/lwcgl.h>
#include <lwcgl/glmodern.h>

#include <string>

namespace Renderer
{
namespace Gpu
{

class GBufferGpu;

class ReprojectionCacheGpu
{
public:
    ~ReprojectionCacheGpu() { shutdown(); }
    ReprojectionCacheGpu() = default;
    ReprojectionCacheGpu(const ReprojectionCacheGpu&) = delete;
    ReprojectionCacheGpu& operator=(const ReprojectionCacheGpu&) = delete;

    bool init(std::string *error=nullptr);
    bool resize(int width,int height,std::string *error=nullptr);
    bool ensure(int width,int height,std::string *error=nullptr);
    bool reproject(const GBufferGpu& gbuffer,
                   GLuint previous_indirect,
                   GLuint previous_reflection,
                   GLuint destination_indirect,
                   GLuint destination_reflection,
                   GLuint destination_position,
                   const Math::Vec3& camera_position,
                   std::string *error=nullptr);
    bool capture(const GBufferGpu& gbuffer,
                 const Math::Mat4& view_projection,
                 std::string *error=nullptr);
    void invalidate();
    void shutdown();

    bool ready() const;
    bool historyValid() const { return history_valid_; }
    GLuint validMaskTexture() const { return valid_mask_; }

private:
    void destroyTextures();

    GLuint reprojection_program_=0;
    GLuint capture_program_=0;
    GLuint previous_position_=0;
    GLuint previous_normal_=0;
    GLuint previous_material_=0;
    GLuint valid_mask_=0;

    GLint previous_view_projection_location_=-1;
    GLint camera_position_location_=-1;
    GLint history_valid_location_=-1;

    Math::Mat4 previous_view_projection_=Math::identity();
    int width_=0;
    int height_=0;
    int trace_width_=0;
    int trace_height_=0;
    bool history_valid_=false;
};

} // namespace Gpu
} // namespace Renderer

#endif
